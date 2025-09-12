#include "NeneApp.h"
#include <iostream>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace SimpleMath;

const int gNumFrameResources = 3;

NeneApp::NeneApp(HINSTANCE mhAppInst, HWND mhMainWnd, std::shared_ptr<InputDevice> inputDevice) 
    : DX12App(mhAppInst, mhMainWnd), m_inputDevice(inputDevice), m_uiManager(mhMainWnd) 
{}

NeneApp::~NeneApp()
{
    if (m_device != nullptr)
        FlushCommandQueue();
}

bool NeneApp::Initialize()
{
    if (!DX12App::Initialize()) 
        return false;

    // Reset the command list to prep for initialization commands.
    ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), nullptr));

    BuildDescriptorHeaps();
    BuildRootSignature();
    BuildShadersAndInputLayout();
    LoadTextures();
    BuildMaterials();
    BuildGeometry();
    LoadObjModel("assets/sponza.obj", Matrix::CreateScale(0.02f));
    BuildRenderItems();
    BuildFrameResources();
    BuildPSOs();

    InitCamera();

    m_uiManager.InitImGui(m_device.Get(), SwapChainBufferCount, mSrvDescriptorHeap.Get());

    // Execute the initialization commands.
    ThrowIfFailed(m_commandList->Close());
    ID3D12CommandList* cmdsLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
    
    // Wait until initialization is complete.
    FlushCommandQueue();
    return true;
}

void NeneApp::OnResize()
{
    DX12App::OnResize();

    m_camera.SetLens(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);

    // The window resized, so update the aspect ratio and recompute the projection matrix.
    XMMATRIX P = XMMatrixPerspectiveFovLH(m_camera.GetFovY(), m_camera.GetAspect(), m_camera.GetNearZ(), m_camera.GetFarZ());
    XMStoreFloat4x4(&mProj, P);
}

void NeneApp::UpdateInputs(const GameTimer& gt)
{
    m_mousePos = m_inputDevice->MousePosition;
    m_mouseDelta = m_inputDevice->MouseOffset;
    m_mouseWheelDelta = m_inputDevice->MouseWheelDelta;

    if (m_inputDevice->IsKeyDown(Keys::W))
        m_camera.Walk(m_cameraSpeed * gt.DeltaTime());
    if (m_inputDevice->IsKeyDown(Keys::A))
        m_camera.Strafe(-m_cameraSpeed * gt.DeltaTime());
    if (m_inputDevice->IsKeyDown(Keys::S))
        m_camera.Walk(-m_cameraSpeed * gt.DeltaTime());
    if (m_inputDevice->IsKeyDown(Keys::D))
        m_camera.Strafe(m_cameraSpeed * gt.DeltaTime());
    if (m_inputDevice->IsKeyDown(Keys::RightButton)) {
        if (m_mouseDelta != Vector2::Zero) {
            float yaw = m_mouseDelta.x * m_mouseSensitivity;
            float pitch = m_mouseDelta.y * m_mouseSensitivity;
            m_camera.RotateY(yaw);
            m_camera.Pitch(pitch);
        }
    }

    m_inputDevice->MouseOffset = Vector2(0.0f, 0.0f);
    m_mouseDelta = Vector2::Zero;
}

void NeneApp::UpdateCamera(const GameTimer& gt)
{
    m_camera.UpdateViewMatrix();
    XMMATRIX view = m_camera.GetView();
    XMStoreFloat4x4(&mView, view);

    // std::cout << "Camera Pos: " << m_camera.GetPosition3f().x << ", " << m_camera.GetPosition3f().y << ", " << m_camera.GetPosition3f().z << std::endl;
}

void NeneApp::AnimateMaterials(const GameTimer& gt)
{
}

void NeneApp::UpdateObjectCBs(const GameTimer& gt)
{
    auto currObjectCB = mCurrFrameResource->ObjectCB.get();
    for (auto& e : mAllRitems)
    {
        // Only update the cbuffer data if the constants have changed.  
        // This needs to be tracked per frame resource.
        if (e->NumFramesDirty > 0)
        {
            XMMATRIX world = XMLoadFloat4x4(&e->World);
            XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);

            ObjectConstants objConstants;
            XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
            XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));

            currObjectCB->CopyData(e->ObjCBIndex, objConstants);

            // Next FrameResource need to be updated too.
            e->NumFramesDirty--;
        }
    }
}

void NeneApp::UpdateMaterialCBs(const GameTimer& gt)
{
    auto currMaterialCB = mCurrFrameResource->MaterialCB.get();
    for (auto& e : mMaterials)
    {
        // Only update the cbuffer data if the constants have changed.  If the cbuffer
        // data changes, it needs to be updated for each FrameResource.
        Material* mat = e.second.get();
        if (mat->NumFramesDirty > 0)
        {
            XMMATRIX matTransform = XMLoadFloat4x4(&mat->MatTransform);

            MaterialConstants matConstants;
            matConstants.DiffuseAlbedo = mat->DiffuseAlbedo;
            matConstants.FresnelR0 = mat->FresnelR0;
            matConstants.Roughness = mat->Roughness;
            XMStoreFloat4x4(&matConstants.MatTransform, XMMatrixTranspose(matTransform));

            currMaterialCB->CopyData(mat->MatCBIndex, matConstants);

            // Next FrameResource need to be updated too.
            mat->NumFramesDirty--;
        }
    }
}

void NeneApp::UpdateMainPassCB(const GameTimer& gt)
{
    XMMATRIX view = XMLoadFloat4x4(&mView);
    XMMATRIX proj = XMLoadFloat4x4(&mProj);

    XMMATRIX viewProj = XMMatrixMultiply(view, proj);
    XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
    XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
    XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

    XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
    XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(invView));
    XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
    XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
    XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));
    XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
    XMStoreFloat3(&mMainPassCB.EyePosW, m_camera.GetPosition());
    mMainPassCB.RenderTargetSize = XMFLOAT2((float)m_clientWidth, (float)m_clientHeight);
    mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / m_clientWidth, 1.0f / m_clientHeight);
    mMainPassCB.NearZ = m_camera.GetNearZ();
    mMainPassCB.FarZ = m_camera.GetFarZ();
    mMainPassCB.TotalTime = gt.TotalTime();
    mMainPassCB.DeltaTime = gt.DeltaTime();
    mMainPassCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };
    mMainPassCB.Lights[0].Direction = { 0.57735f, -0.57735f, 0.57735f };
    mMainPassCB.Lights[0].Strength = { 0.6f, 0.6f, 0.6f };
    mMainPassCB.Lights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
    mMainPassCB.Lights[1].Strength = { 0.3f, 0.3f, 0.3f };
    mMainPassCB.Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
    mMainPassCB.Lights[2].Strength = { 0.15f, 0.15f, 0.15f };

    auto currPassCB = mCurrFrameResource->PassCB.get();
    currPassCB->CopyData(0, mMainPassCB);
}

DirectX::BoundingBox NeneApp::ComputeBounds(const std::vector<Vertex>& verts)
{
    if (verts.empty()) return DirectX::BoundingBox();
    DirectX::XMFLOAT3 _min = { FLT_MAX, FLT_MAX, FLT_MAX };
    DirectX::XMFLOAT3 _max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
    for (const auto& v : verts)
    {
        _min.x = std::min(_min.x, v.Pos.x);
        _min.y = std::min(_min.y, v.Pos.y);
        _min.z = std::min(_min.z, v.Pos.z);
        _max.x = std::max(_max.x, v.Pos.x);
        _max.y = std::max(_max.y, v.Pos.y);
        _max.z = std::max(_max.z, v.Pos.z);
    }
    DirectX::XMFLOAT3 center = { (_min.x + _max.x) / 2, (_min.y + _max.y) / 2, (_min.z + _max.z) / 2 };
    float extent = std::max({ _max.x - _min.x, _max.y - _min.y, _max.z - _min.z }) / 2;
    return DirectX::BoundingBox(center, { extent, extent, extent });
}

void NeneApp::Update(const GameTimer& gt)
{
    UpdateInputs(gt);
    UpdateCamera(gt);

    // Cycle through the circular frame resource array.
    mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
    mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

    // Has the GPU finished processing the commands of the current frame resource?
    // If not, wait until the GPU has completed commands up to this fence point.
    if (mCurrFrameResource->Fence != 0 && m_fence->GetCompletedValue() < mCurrFrameResource->Fence)
    {
        HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
        ThrowIfFailed(m_fence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }

    AnimateMaterials(gt);
    UpdateObjectCBs(gt);
    UpdateMaterialCBs(gt);
    UpdateMainPassCB(gt);
}

void NeneApp::Draw(const GameTimer& gt)
{
    PopulateCommandList();
    
    // Add the command list to the queue for execution.
    ID3D12CommandList* cmdLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

    // Swap the back and front buffers
    ThrowIfFailed(m_swapChain->Present(0, 0)); // TODO: add DXGI_PRESENT_ALLOW_TEARING flag if want to disable fps lock.

    m_currBackBuffer = (m_currBackBuffer + 1) % SwapChainBufferCount;

    // Advance the fence value to mark commands up to this fence point.
    mCurrFrameResource->Fence = ++m_currentFence;

    // Add an instruction to the command queue to set a new fence point. 
    // Because we are on the GPU timeline, the new fence point won't be 
    // set until the GPU finishes processing all the commands prior to this Signal().
    m_commandQueue->Signal(m_fence.Get(), m_currentFence);
}

void NeneApp::PopulateCommandList()
{
    auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;

    // Reuse the memory associated with command recording.
    // We can only reset when the associated command lists have finished execution on the GPU.
    ThrowIfFailed(cmdListAlloc->Reset());

    // A command list can be reset after it has been added to the command queue via ExecuteCommandList.
    // Reusing the command list reuses memory.
    ThrowIfFailed(m_commandList->Reset(cmdListAlloc.Get(), mPSOs["opaque"].Get()));

    m_commandList->RSSetViewports(1, &m_viewport);
    m_commandList->RSSetScissorRects(1, &m_scissorRect);

    // Indicate a state transition on the resource usage.
    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
        D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    // Clear the back buffer and depth buffer.
    m_commandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
    m_commandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    // Specify the buffers we are going to render to.
    m_commandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

    ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvDescriptorHeap.Get() };
    m_commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

    m_commandList->SetGraphicsRootSignature(mRootSignature.Get());

    auto passCB = mCurrFrameResource->PassCB->Resource();
    m_commandList->SetGraphicsRootConstantBufferView(2, passCB->GetGPUVirtualAddress());

    DrawRenderItems(m_commandList.Get(), mOpaqueRitems);

    // Indicate a state transition on the resource usage.
    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
        D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    // Done recording commands.
    ThrowIfFailed(m_commandList->Close());
}

void NeneApp::SetDelegates()
{}

void NeneApp::InitCamera()
{
    m_camera.SetPosition(0.0f, 1.0f, -5.0f);
    m_camera.LookAt(m_camera.GetPosition3f(), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT3(0.0f, 1.0f, 0.0f));
    m_camera.UpdateViewMatrix();
}

void NeneApp::BuildDescriptorHeaps()
{
    //
    // Create the SRV heap.
    //
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.NumDescriptors = 128;
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(m_device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));

    mCbvSrvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void NeneApp::LoadTextures()
{
    LoadTexture("assets/textures/texture_error.dds");
    LoadTexture("assets/textures/luna_textures/WoodCrate01.dds");
}

bool NeneApp::LoadTexture(const std::string& filename)
{
    std::string texName = filename.substr(filename.find_last_of("/\\") + 1);
    texName = texName.substr(0, texName.find_last_of('.'));
    
    std::wstring wFilename = AnsiToWString(filename);
    auto tex = std::make_unique<Texture>();
    tex->Name = texName;
    tex->Filename = wFilename;

    HRESULT hr = DirectX::CreateDDSTextureFromFile12(m_device.Get(), m_commandList.Get(), wFilename.c_str(),
        tex->Resource, tex->UploadHeap);
    if (FAILED(hr))
    {
        std::cout << "Failed to load texture: '" << filename << "', HRESULT: 0x" << std::hex << hr << std::endl;
        return false;
    }

    // TODO: logging std::cout << "Loaded texture resource: '" << filename << "' (total textures: " << mTextures.size() << ")" << std::endl;

    CD3DX12_CPU_DESCRIPTOR_HANDLE hDesc(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
    hDesc.Offset((INT)mTextures.size(), mCbvSrvDescriptorSize); // Смещение для следующего свободного слота

    // Basic SRV descriptor for 2D texture (d3dx12.h)
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = tex->Resource->GetDesc().Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = tex->Resource->GetDesc().MipLevels;
    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

    // Create SRV
    m_device->CreateShaderResourceView(tex->Resource.Get(), &srvDesc, hDesc);

    tex->GpuHandle = mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
    tex->GpuHandle.ptr += mTextures.size() * mCbvSrvDescriptorSize;

    mTextures[tex->Name] = std::move(tex);
    return true;
}

void NeneApp::LoadObjModel(const std::string& filename, Matrix Transform)
{
    Assimp::Importer importer;
    const aiScene* pScene = importer.ReadFile(filename,
        aiProcess_Triangulate      |
        aiProcess_JoinIdenticalVertices  |
        aiProcess_FlipUVs                |
        aiProcess_CalcTangentSpace       |
        aiProcess_GenNormals);

    if (!pScene || pScene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !pScene->mRootNode)
    {
        std::cout << "Assimp error: " << importer.GetErrorString() << std::endl;
        return;
    }
    std::cout << "Loaded model: " << filename << ", Meshes: " << pScene->mNumMeshes << ", Materials: " << pScene->mNumMaterials << std::endl;
    
    std::string modelName = filename.substr(filename.find_last_of("/\\") + 1);
    modelName = modelName.substr(0, modelName.find_last_of('.'));

    std::string geoName = modelName + "_Geo";
    auto geo = std::make_shared<MeshGeometry>();
    geo->Name = geoName;

    // One index and vertex buffer for all models
    UINT indexOffset = 0;
    UINT vertexOffset = 0;

    // temp sctructure for material index
    std::vector<std::pair<std::string, UINT>> meshMaterialIndices;

    std::uint32_t vertexCount = 0;
    std::uint32_t indexCount = 0;
    {
        aiMesh* tmpMesh = nullptr;
        for (size_t i = 0; i < pScene->mNumMeshes; i++) {
            tmpMesh = pScene->mMeshes[i];
            vertexCount += tmpMesh->mNumVertices;
            indexCount += tmpMesh->mNumFaces * 3;
        }
    }
    std::vector<Vertex> vertices(vertexCount);
    std::vector<std::uint32_t> indices(indexCount);
    std::unordered_map<std::string, SubmeshGeometry> drawArgs;

    for (UINT iMesh = 0; iMesh < pScene->mNumMeshes; ++iMesh)
    {
        aiMesh* mesh = pScene->mMeshes[iMesh];
        aiString meshName;
        if (mesh->mName.length > 0) meshName = mesh->mName;
        else meshName = aiString(std::to_string(iMesh).c_str());  // Default name
        
        // Vertices
        for (UINT i = 0; i < mesh->mNumVertices; ++i)
        {
            Vertex v;
            v.Pos = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };
            if (mesh->HasNormals())                 v.Normal = { mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z };
            if (mesh->HasTangentsAndBitangents())   v.TangentU = { mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z };
            if (mesh->HasTextureCoords(0))          v.TexC = { mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y };
            vertices[vertexOffset + i] = v;
        }

        UINT startIndex = indexOffset;
        // Indices
        for (UINT j = 0; j < mesh->mNumFaces; ++j)
        {
            aiFace face = mesh->mFaces[j];
            for (UINT k = 0; k < face.mNumIndices; ++k)
                indices[indexOffset++] = face.mIndices[k];
        }

        // Submesh info
        SubmeshGeometry submesh;
        submesh.IndexCount = mesh->mNumFaces * 3;
        submesh.StartIndexLocation = startIndex;
        submesh.BaseVertexLocation = vertexOffset;
        submesh.Bounds = ComputeBounds(vertices);  // TODO: Check bounding box correction
        submesh.MaterialIndex = mesh->mMaterialIndex;

        drawArgs[meshName.C_Str()] = submesh;
        meshMaterialIndices.push_back({ meshName.C_Str(), mesh->mMaterialIndex });

        vertexOffset += mesh->mNumVertices;
    }

    // Write vertices and indices to buffers
    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
    const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint32_t);

    ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
    CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

    ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
    CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

    geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(m_device.Get(), m_commandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);
    geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(m_device.Get(), m_commandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

    geo->VertexByteStride = sizeof(Vertex);
    geo->VertexBufferByteSize = vbByteSize;
    geo->IndexFormat = DXGI_FORMAT_R32_UINT;
    geo->IndexBufferByteSize = ibByteSize;
    geo->DrawArgs = drawArgs;


    for (UINT i = 0; i < pScene->mNumMaterials; ++i)
    {
        aiMaterial* mat = pScene->mMaterials[i];
        aiString matName;
        mat->Get(AI_MATKEY_NAME, matName);
        std::cout << "Material " << i << ": " << matName.C_Str() << std::endl;
    }

    // Materials and textures
    for (UINT i = 0; i < pScene->mNumMaterials; ++i)
    {
        aiMaterial* mat = pScene->mMaterials[i];
        aiString matName;
        mat->Get(AI_MATKEY_NAME, matName);
        if (matName.length == 0) matName = aiString(std::to_string(i).c_str());

        auto material = std::make_unique<Material>();
        material->Name = matName.C_Str();
        material->MatCBIndex = (int)mMaterials.size();
        material->NumFramesDirty = gNumFrameResources;

        // Diffuse color
        aiColor3D color;
        mat->Get(AI_MATKEY_COLOR_DIFFUSE, color);
        material->DiffuseAlbedo = XMFLOAT4(color.r, color.g, color.b, 1.0f);

        // Diffuse Texture
        aiString texPath;
        if (mat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS)
        {
            std::string fullPath = filename.substr(0, filename.find_last_of("/\\")) + "/" + texPath.C_Str();
            int texIndexBefore = (int)mTextures.size();
            if (LoadTexture(fullPath))
                material->DiffuseSrvHeapIndex = texIndexBefore;
            else
                material->DiffuseSrvHeapIndex = 0;

            // TODO: Logging: std::cout << "Assigned texture index " << material->DiffuseSrvHeapIndex << " for material '" << material->Name << "'" << std::endl;
        }
        else
        {
            material->DiffuseSrvHeapIndex = 0;  // If no texture - error texture than.
        }

        if (mat->GetTexture(aiTextureType_NORMALS, 0, &texPath) == AI_SUCCESS)
        {
            std::string fullPath = filename.substr(0, filename.find_last_of("/\\")) + "/" + texPath.C_Str();
            int texIndexBefore = (int)mTextures.size();
            if (LoadTexture(fullPath))
                material->NormalSrvHeapIndex = texIndexBefore;
            else
                material->NormalSrvHeapIndex = 0;
        }
        if (mat->GetTexture(aiTextureType_SPECULAR, 0, &texPath) == AI_SUCCESS)
        {
            std::string fullPath = filename.substr(0, filename.find_last_of("/\\")) + "/" + texPath.C_Str();
            int texIndexBefore = (int)mTextures.size();
            if (LoadTexture(fullPath))
                material->SpecularSrvHeapIndex = texIndexBefore;
            else
                material->SpecularSrvHeapIndex = 0;
        }

        // Fresnel and Roughness. TODO: Make it with Assimp
        material->FresnelR0 = XMFLOAT3(0.01f, 0.01f, 0.01f);
        material->Roughness = 0.25f;
        material->MatTransform = MathHelper::Identity4x4();

        mMaterials[material->Name] = std::move(material);
    }
    
    // RenderItem creation
    for (const auto& drawArg : drawArgs)
    {
        auto ritem = std::make_shared<RenderItem>();
        ritem->ObjCBIndex = (UINT)mAllRitems.size();
        ritem->Geo = geo.get();      

        aiMaterial* mat = pScene->mMaterials[drawArg.second.MaterialIndex];
        aiString matName;
        mat->Get(AI_MATKEY_NAME, matName);
        if (mMaterials.at(matName.C_Str()) != nullptr)
        {
            ritem->Mat = mMaterials[matName.C_Str()].get();
        }
        else
        {
            std::cout << "Warning: Material " << matName.C_Str() << " not found for submesh " << drawArg.first << std::endl;
            ritem->Mat = mMaterials["error"].get();
        }
        
        ritem->World = Transform;  // TODO: aiNode transform
        ritem->TexTransform = MathHelper::Identity4x4();
        ritem->NumFramesDirty = gNumFrameResources;
        ritem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        ritem->IndexCount = drawArg.second.IndexCount;
        ritem->StartIndexLocation = drawArg.second.StartIndexLocation;
        ritem->BaseVertexLocation = drawArg.second.BaseVertexLocation;

        mModelRenderItems[drawArg.first] = std::move(ritem);
        mAllRitems.push_back(mModelRenderItems[drawArg.first]);
        mOpaqueRitems.push_back(mAllRitems.back());
    }
    

    std::cout << "Total RenderItems after LoadObjModel: " << mAllRitems.size() << std::endl;
    std::cout << "OpaqueRitems size: " << mOpaqueRitems.size() << std::endl << std::endl;;
    
    mGeometries[geo->Name] = std::move(geo);

    // resize ObjectCB/MaterialCB
    BuildFrameResources();
}


void NeneApp::BuildRootSignature()
{
    CD3DX12_DESCRIPTOR_RANGE texTable;
    texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

    // Root parameter can be a table, root descriptor or root constants.
    CD3DX12_ROOT_PARAMETER slotRootParameter[4];

    // Perfomance TIP: Order from most frequent to least frequent.
    slotRootParameter[0].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);
    slotRootParameter[1].InitAsConstantBufferView(0);
    slotRootParameter[2].InitAsConstantBufferView(1);
    slotRootParameter[3].InitAsConstantBufferView(2);

    auto staticSamplers = GetStaticSamplers();

    // A root signature is an array of root parameters.
    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(4, slotRootParameter,
        (UINT)staticSamplers.size(), staticSamplers.data(),
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    // create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;
    HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
        serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

    if (errorBlob != nullptr)
    {
        ::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    }
    ThrowIfFailed(hr);

    ThrowIfFailed(m_device->CreateRootSignature(
        0,
        serializedRootSig->GetBufferPointer(),
        serializedRootSig->GetBufferSize(),
        IID_PPV_ARGS(mRootSignature.GetAddressOf())));
}

void NeneApp::BuildShadersAndInputLayout()
{
    mShaders["standardVS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", nullptr, "VS", "vs_5_0");
    mShaders["opaquePS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", nullptr, "PS", "ps_5_0");

    mInputLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };
}

void NeneApp::BuildGeometry()
{
    GeometryGenerator geoGen;
    GeometryGenerator::MeshData box = geoGen.CreateBox(1.0f, 1.0f, 1.0f, 3);

    SubmeshGeometry boxSubmesh;
    boxSubmesh.IndexCount = (UINT)box.Indices32.size();
    boxSubmesh.StartIndexLocation = 0;
    boxSubmesh.BaseVertexLocation = 0;


    std::vector<Vertex> vertices(box.Vertices.size());

    for (size_t i = 0; i < box.Vertices.size(); ++i)
    {
        vertices[i].Pos = box.Vertices[i].Position;
        vertices[i].Normal = box.Vertices[i].Normal;
        vertices[i].TexC = box.Vertices[i].TexC;
    }

    std::vector<std::uint16_t> indices = box.GetIndices16();

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
    const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

    auto geo = std::make_unique<MeshGeometry>();
    geo->Name = "boxGeo";

    ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
    CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

    ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
    CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

    geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(m_device.Get(),
        m_commandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

    geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(m_device.Get(),
        m_commandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

    geo->VertexByteStride = sizeof(Vertex);
    geo->VertexBufferByteSize = vbByteSize;
    geo->IndexFormat = DXGI_FORMAT_R16_UINT;
    geo->IndexBufferByteSize = ibByteSize;

    geo->DrawArgs["box"] = boxSubmesh;

    mGeometries[geo->Name] = std::move(geo);
}

void NeneApp::BuildPSOs()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;

    //
    // PSO for opaque objects.
    //
    ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
    opaquePsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
    opaquePsoDesc.pRootSignature = mRootSignature.Get();
    opaquePsoDesc.VS =
    {
        reinterpret_cast<BYTE*>(mShaders["standardVS"]->GetBufferPointer()),
        mShaders["standardVS"]->GetBufferSize()
    };
    opaquePsoDesc.PS =
    {
        reinterpret_cast<BYTE*>(mShaders["opaquePS"]->GetBufferPointer()),
        mShaders["opaquePS"]->GetBufferSize()
    };
    opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    opaquePsoDesc.SampleMask = UINT_MAX;
    opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    opaquePsoDesc.NumRenderTargets = 1;
    opaquePsoDesc.RTVFormats[0] = m_backBufferFormat;
    opaquePsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
    opaquePsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
    opaquePsoDesc.DSVFormat = m_depthStencilFormat;
    ThrowIfFailed(m_device->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&mPSOs["opaque"])));


    //
    // PSO for opaque wireframe objects.
    //

    D3D12_GRAPHICS_PIPELINE_STATE_DESC opaqueWireframePsoDesc = opaquePsoDesc;
    opaqueWireframePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
    ThrowIfFailed(m_device->CreateGraphicsPipelineState(&opaqueWireframePsoDesc, IID_PPV_ARGS(&mPSOs["opaque_wireframe"])));
}

void NeneApp::BuildTextureSRVs()
{
    if (mSrvDescriptorHeap == nullptr || mTextures.empty())
    {
        std::cout << "No SRV heap or textures to build SRVs" << std::endl;
        return;
    }

    CD3DX12_CPU_DESCRIPTOR_HANDLE hDesc(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
    UINT texIndex = 0;
    for (const auto& texPair : mTextures)
    {
        auto& tex = texPair.second;
        if (tex->Resource == nullptr)
        {
            std::cout << "Skipping NULL resource for texture: " << tex->Name << std::endl;
            continue;
        }

        // Basic SRV descriptor for 2D texture (d3dx12.h)
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = tex->Resource->GetDesc().Format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.MipLevels = tex->Resource->GetDesc().MipLevels;
        srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

        // Create SRV
        m_device->CreateShaderResourceView(tex->Resource.Get(), &srvDesc, hDesc);

        // GPU handle for debugging (optional)
        tex->GpuHandle = mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
        tex->GpuHandle.ptr += texIndex * mCbvSrvDescriptorSize;

        hDesc.Offset(1, mCbvSrvDescriptorSize);
        texIndex++;

        // TODO: logging: std::cout << "Created SRV for texture: '" << tex->Name << "' at heap index " << (texIndex - 1) << std::endl;
    }

    std::cout << "Built SRVs for " << mTextures.size() << " textures" << std::endl;
}

void NeneApp::BuildFrameResources()
{
    for (int i = 0; i < gNumFrameResources; ++i)
    {
        mFrameResources.push_back(std::make_unique<FrameResource>(m_device.Get(),
            1, (UINT)mAllRitems.size(), (UINT)mMaterials.size()));
    }
}

void NeneApp::BuildMaterials()
{
    auto mat = std::make_unique<Material>();
    mat->Name = "error";
    mat->MatCBIndex = 0;
    mat->DiffuseSrvHeapIndex = 0;
    mat->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    mat->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
    mat->Roughness = 0.2f;

    mMaterials["error"] = std::move(mat);

    mat = std::make_unique<Material>();
    mat->Name = "woodCrate";
    mat->MatCBIndex = 1;
    mat->DiffuseSrvHeapIndex = 1;
    mat->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    mat->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
    mat->Roughness = 0.2f;

    mMaterials["woodCrate"] = std::move(mat);
}

void NeneApp::BuildRenderItems()
{
    auto boxRitem = std::make_shared<RenderItem>();
    boxRitem->ObjCBIndex = 0;
    boxRitem->Mat = mMaterials["woodCrate"].get();
    boxRitem->Geo = mGeometries["boxGeo"].get();
    boxRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    boxRitem->IndexCount = boxRitem->Geo->DrawArgs["box"].IndexCount;
    boxRitem->StartIndexLocation = boxRitem->Geo->DrawArgs["box"].StartIndexLocation;
    boxRitem->BaseVertexLocation = boxRitem->Geo->DrawArgs["box"].BaseVertexLocation;
    mAllRitems.push_back(std::move(boxRitem));

    // All the render items are opaque.
    for (auto& e : mAllRitems)
        mOpaqueRitems.push_back(e);
    std::cout << "Total RenderItems after BuildRenderItems: " << mAllRitems.size() << std::endl;
    std::cout << "OpaqueRitems size: " << mOpaqueRitems.size() << std::endl;
}

void NeneApp::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<std::shared_ptr<RenderItem>>& ritems)
{
    UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
    UINT matCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(MaterialConstants));

    auto objectCB = mCurrFrameResource->ObjectCB->Resource();
    auto matCB = mCurrFrameResource->MaterialCB->Resource();

    // For each render item...
    for (size_t i = 0; i < ritems.size(); ++i)
    {
        auto ri = ritems[i];

        cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
        cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
        cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

        CD3DX12_GPU_DESCRIPTOR_HANDLE tex(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
        tex.Offset(ri->Mat->DiffuseSrvHeapIndex, mCbvSrvDescriptorSize);

        D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize;
        D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = matCB->GetGPUVirtualAddress() + ri->Mat->MatCBIndex * matCBByteSize;

        cmdList->SetGraphicsRootDescriptorTable(0, tex);
        cmdList->SetGraphicsRootConstantBufferView(1, objCBAddress);
        cmdList->SetGraphicsRootConstantBufferView(3, matCBAddress);

        cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
    }
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> NeneApp::GetStaticSamplers()
{
    // Applications usually only need a handful of samplers.  So just define them all up front
    // and keep them available as part of the root signature.  

    const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
        0, // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

    const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
        1, // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

    const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
        2, // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

    const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
        3, // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

    const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
        4, // shaderRegister
        D3D12_FILTER_ANISOTROPIC, // filter
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
        0.0f,                             // mipLODBias
        8);                               // maxAnisotropy

    const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
        5, // shaderRegister
        D3D12_FILTER_ANISOTROPIC, // filter
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
        0.0f,                              // mipLODBias
        8);                                // maxAnisotropy

    return {
        pointWrap, pointClamp,
        linearWrap, linearClamp,
        anisotropicWrap, anisotropicClamp };
}