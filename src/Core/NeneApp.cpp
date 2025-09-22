#include "NeneApp.h"
#include <iostream>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <random>

// Light count constants
const int NUM_DIR_LIGHTS = 1;
const int NUM_POINT_LIGHTS = 1;
const int NUM_SPOT_LIGHTS = 1;

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
    m_gBuffer.Initialize(m_device.Get(), m_clientWidth, m_clientHeight, m_rtvGBufferHeap->GetCPUDescriptorHandleForHeapStart(),
        mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart(),
        m_dsvGBufferHeap->GetCPUDescriptorHandleForHeapStart());

    BuildRootSignature();
    BuildShadersAndInputLayout();
    LoadTextures();
    BuildMaterials();
    BuildLightGeometries();
    BuildBoxGeometry();
    //BuildManyBoxes(10000);
    //BuildDisplacementTestGeometry();
    //BuildPlane(10.f, 10.f, 8, 8, "highMountain", "mountain", CreateTransformMatrix(-11, 0, 0), D3D_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST);
    //BuildPlane(10.f, 10.f, 1, 1, "lowMountain", "mountain", CreateTransformMatrix(0, 0, 0), D3D_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST);
    //BuildPlane(10.f, 10.f, 8, 8, "hBox", "woodCrate", CreateTransformMatrix(-11, 0, -11), D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    //BuildPlane(10.f, 10.f, 2, 2, "lBox", "woodCrate", CreateTransformMatrix(0, 0, -11), D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    LoadObjModel("assets/sponza.obj", (Matrix::CreateScale(0.04f)) * Matrix::CreateTranslation(0.f, -1.f, 0.f));
    BuildRenderItems();
    BuildFrameResources();
    BuildPSOs();

    // Init Post-process
    BuildPostProcessHeaps();
    BuildPostProcessResources();
    BuildPostProcessSignature();
    BuildPostProcessPSO();

    // Initialize Particle System
    UINT particleDescriptorOffset = 4 + (UINT)mTextures.size() + 1; // После GBuffer (4) + текстур + 1 для post-process
    mParticleSystem = std::make_unique<ParticleSystem>(m_device.Get(), m_commandList.Get(), 
        mSrvDescriptorHeap.Get(), particleDescriptorOffset, 4096, SimpleMath::Vector3(0.0f, 2.0f, 0.0f)); // Пример: 4096 частиц, origin в (0,2,0)

    InitCamera();

    ImGui_ImplDX12_InitInfo init_info = {};
    init_info.Device = m_device.Get();
    init_info.CommandQueue = m_commandQueue.Get();
    init_info.NumFramesInFlight = gNumFrameResources;
    init_info.RTVFormat = m_backBufferFormat;
    init_info.DSVFormat = m_depthStencilFormat;
    init_info.SrvDescriptorHeap = m_imguiSrvHeap.Get();
    init_info.LegacySingleSrvCpuDescriptor = m_imguiSrvHeap->GetCPUDescriptorHandleForHeapStart();
    init_info.LegacySingleSrvGpuDescriptor = m_imguiSrvHeap->GetGPUDescriptorHandleForHeapStart();
    m_uiManager.InitImGui(&init_info);

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
    if (m_dsvGBufferHeap)
        m_gBuffer.Resize(m_device.Get(), m_clientWidth, m_clientHeight);
    if (mSrvDescriptorHeap)
        BuildPostProcessResources();
    // Invalidate ImGui's device objects (releases old RTVs/views tied to back buffers)
    ImGui_ImplDX12_InvalidateDeviceObjects();
    // Recreate ImGui's device objects immediately (binds to new back buffers)
    ImGui_ImplDX12_CreateDeviceObjects();

    m_camera.SetLens(0.25f * MathHelper::Pi, AspectRatio(), 0.1f, 1000.0f);

    // The window resized, so update the aspect ratio and recompute the projection matrix.
    XMMATRIX P = XMMatrixPerspectiveFovLH(m_camera.GetFovY(), m_camera.GetAspect(), m_camera.GetNearZ(), m_camera.GetFarZ());
    mProj = P;
}

std::random_device rd; // obtain a random number from hardware
std::mt19937 gen(rd());

SimpleMath::Color GetRandomColor() {
    std::uniform_real_distribution<float> colVal(0.0f, 1.0f);
    return Color(colVal(gen), colVal(gen), colVal(gen));
}

float GetRandomForce() {
    std::uniform_real_distribution<float> force(4.0f, 10.0f);
    float ans = force(gen);
    std::cout << "Random float number = " << ans << std::endl;
    return ans;

}

SimpleMath::Vector3 GetRandomSize() {
    std::uniform_real_distribution<float> force(6.5f, 12.0f);
    return Vector3(force(gen));

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
    if (m_inputDevice->IsKeyDown(Keys::G)) {
        ShootLight(m_camera.GetPosition(), GetRandomSize(), m_camera.GetLook() * GetRandomForce(), GetRandomColor(), 0.5f);
        m_inputDevice->RemovePressedKey(Keys::G);
    }

    // Emit particles on 'P' key
    if (m_inputDevice->IsKeyDown(Keys::P)) {
        mParticleSystem->Emit(50); // Эмитируем 50 частиц
        m_inputDevice->RemovePressedKey(Keys::P);
    }

    m_inputDevice->MouseOffset = Vector2(0.0f, 0.0f);
    m_mouseDelta = Vector2::Zero;
}

void NeneApp::UpdateCamera(const GameTimer& gt)
{
    m_camera.UpdateViewMatrix();
    mView = m_camera.GetView();
    mProj = m_camera.GetProj();

    // TODO: Add debug: std::cout << "Camera Pos: " << m_camera.GetPosition3f().x << ", " << m_camera.GetPosition3f().y << ", " << m_camera.GetPosition3f().z << std::endl;
}

void NeneApp::AnimateMaterials(const GameTimer& gt)
{
}

void NeneApp::UpdateDynamicLights(const GameTimer& gt)
{
    for (auto& dynamicLight : m_DynamicLights)
    {
        if (!dynamicLight->isMoving)
            continue;
        if (dynamicLight->velocity.Length() <= 0.1f) {
            dynamicLight->velocity = Vector3::Zero;
            dynamicLight->isMoving = false;
            dynamicLight->lightRI->IsDrawingDebugGeometry = false;
            continue;
        }
        dynamicLight->lightRI->light.Position += dynamicLight->velocity * gt.DeltaTime();
        Vector3 oppositeDirection = dynamicLight->velocity;
        oppositeDirection.Normalize(oppositeDirection * -1);
        dynamicLight->velocity -= oppositeDirection * dynamicLight->linearDamping * gt.DeltaTime();
        dynamicLight->lightRI->NumFramesDirtyLight = gNumFrameResources;
    }
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
            /*SubmeshGeometry* mesh;
            if (!e->SubmeshName.empty()) {
                mesh = &(e->CurrentGeo->DrawArgs[e->SubmeshName]);
            }
            else {
                if (e->CurrentGeo)
                    mesh = &(e->CurrentGeo->DrawArgs.begin()->second);
                else
                    mesh = &(e->Geo->DrawArgs.begin()->second);
            }
            if (!mesh->isInWorld) {
                BoundingBox temp = mesh->Bounds;
                temp.Transform(temp, e->World);
                mesh->Bounds = temp;
                mesh->isInWorld = true;
            }*/

            ObjectConstants objConstants;
            objConstants.World = e->World.Transpose();
            objConstants.TexTransform = e->TexTransform.Transpose();

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
            matConstants.TessellationFactor = mat->TessellationFactor;
            matConstants.DisplacementScale = mat->DisplacementScale;
            matConstants.DisplacementBias = mat->DisplacementBias;

            currMaterialCB->CopyData(mat->MatCBIndex, matConstants);

            // Next FrameResource need to be updated too.
            mat->NumFramesDirty--;
        }
    }
}

void NeneApp::UpdateMainPassCB(const GameTimer& gt)
{
    //XMMATRIX view = XMLoadFloat4x4(&mView);
    //XMMATRIX proj = XMLoadFloat4x4(&mProj);

    //XMMATRIX viewProj = XMMatrixMultiply(view, proj);
    //XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
    //XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
    //XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

    //XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
    //XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(invView));
    //XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
    //XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
    //XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));
    //XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
    //XMStoreFloat3(&mMainPassCB.EyePosW, m_camera.GetPosition());

    mMainPassCB.View = mView.Transpose();
    mMainPassCB.InvView = mView.Invert().Transpose();
    mMainPassCB.Proj = mProj.Transpose();
    mMainPassCB.InvProj = mProj.Invert().Transpose();
    mMainPassCB.ViewProj = (mView * mProj).Transpose();
    mMainPassCB.InvViewProj = mMainPassCB.ViewProj.Invert();
    mMainPassCB.EyePosW = m_camera.GetPosition();
    mMainPassCB.RenderTargetSize = Vector2((float)m_clientWidth, (float)m_clientHeight);
    mMainPassCB.InvRenderTargetSize = Vector2(1.0f / m_clientWidth, 1.0f / m_clientHeight);
    mMainPassCB.NearZ = m_camera.GetNearZ();
    mMainPassCB.FarZ = m_camera.GetFarZ();
    mMainPassCB.TotalTime = gt.TotalTime();
    mMainPassCB.DeltaTime = gt.DeltaTime();
    mMainPassCB.AmbientLight = Color(0.05f, 0.05f, 0.05f, 1.0f);

    auto currPassCB = mCurrFrameResource->PassCB.get();
    currPassCB->CopyData(0, mMainPassCB);
}

void NeneApp::UpdateLightCB(const GameTimer& gt) {
    auto currLightCB = mCurrFrameResource->LightCB.get();
    for (auto& e : mLightRitems)
    {
        // Only update the cbuffer data if the constants have changed.  
        // This needs to be tracked per frame resource.
        if (e->NumFramesDirtyLight > 0)
        {
            if (e->LightCBIndex == UINT(-1))
            {
                std::cout << "ERROR: Invalid LightCBIndex for light!" << std::endl;
                throw std::runtime_error("ERROR: Invalid LightCBIndex for light!");
            }

            e->World = SimpleMath::Matrix::CreateScale(e->light.FalloffEnd) * SimpleMath::Matrix::CreateTranslation(e->light.Position);

            LightData lightConstants;
            lightConstants.light = e->light;
            lightConstants.lightType = e->lightType;
            lightConstants.WorldLight = e->World.Transpose();


            currLightCB->CopyData(e->LightCBIndex, lightConstants);
            // Next FrameResource need to be updated too.
            e->NumFramesDirtyLight--;
        }
    }
}

void NeneApp::UpdateVisibleRenderItems()
{
    mVisibleRitems.clear();
    addedItems.clear();
    Matrix viewMat = mView;
    Matrix projMat = mProj;

    BoundingFrustum frustum, localSpaceFrustum;
    BoundingFrustum::CreateFromMatrix(frustum, projMat);

    Vector3 eyePos = mMainPassCB.EyePosW;

    RebuildOctreeIfNeeded();

    if (mOctreeRoot && mUseFrustumCulling) {
        CollectVisibleItems(mOctreeRoot.get(), frustum, mVisibleRitems, eyePos);
    }
    else {
        // Fallback to linear if no octree
        for (auto& ri : mAllRitems)
        {
            Matrix worldMat = ri->World;
            BoundingBox bound;

            if (!ri->SubmeshName.empty()) {
                bound = ri->Geo->DrawArgs.at(ri->SubmeshName).Bounds;
            }
            else
                bound = ri->Geo->DrawArgs.begin()->second.Bounds;


            bool visible = true;
            if (mUseFrustumCulling)
            {
                frustum.Transform(localSpaceFrustum, mView.Invert() * worldMat.Invert());
                visible = localSpaceFrustum.Contains(bound) != DISJOINT;
            }
            ri->Visible = visible;
            if (!visible)
                continue;

            Vector3 center = bound.Center;
            float dist = Vector3::Distance(center, eyePos);

            ri->CurrentGeo = ri->Geo;
            if (ri->UseLOD)
            {
                if (dist > ri->LODThreshold)
                    ri->CurrentGeo = ri->GeoLow ? ri->GeoLow : ri->Geo;
                else if (dist < ri->LODThreshold / 2)
                    ri->CurrentGeo = ri->GeoHigh ? ri->GeoHigh : ri->Geo;
                else
                    ri->CurrentGeo = ri->Geo;
            }
            else
                ri->CurrentGeo = ri->Geo;

            SubmeshGeometry drawArg;
            if (!ri->SubmeshName.empty())
                drawArg = ri->CurrentGeo->DrawArgs.at(ri->SubmeshName);
            else
                drawArg = ri->CurrentGeo->DrawArgs.begin()->second;
            ri->SelectedIndexCount = drawArg.IndexCount;
            ri->SelectedStartIndexLocation = drawArg.StartIndexLocation;
            ri->SelectedBaseVertexLocation = drawArg.BaseVertexLocation;

            mVisibleRitems.push_back(ri);
        }
    }

    mTessRitems.clear();
    mOpaqueRitems.clear();

    // Filtering models from LoadObjModel
    for (auto& ri : mVisibleRitems)
    {
        Material* mat = ri->Mat;
        if (mat->HasDisplacementMap)
            mTessRitems.push_back(ri);
        else
            mOpaqueRitems.push_back(ri);
    }
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

    DirectX::XMFLOAT3 center = {
        (_min.x + _max.x) * 0.5f,
        (_min.y + _max.y) * 0.5f,
        (_min.z + _max.z) * 0.5f
    };

    DirectX::XMFLOAT3 extents = {
        (_max.x - _min.x) * 0.5f,
        (_max.y - _min.y) * 0.5f,
        (_max.z - _min.z) * 0.5f
    };

    return DirectX::BoundingBox(center, extents);
}

DirectX::BoundingBox NeneApp::ComputeRenderItemBounds(const std::shared_ptr<RenderItem>& ri) {
    DirectX::BoundingBox bound;

    if (!ri->SubmeshName.empty()) {
        bound = ri->Geo->DrawArgs.at(ri->SubmeshName).Bounds;
    }
    else
        bound = ri->Geo->DrawArgs.begin()->second.Bounds;
    bound.Transform(bound, ri->World);
    return bound;
}

void NeneApp::BuildOctree(const std::vector<std::shared_ptr<RenderItem>>& items, OctreeNode* node, int depth) {
    if (items.empty() || depth >= mOctreeMaxDepth) {
        // Leaf: Store items whose bounds intersect this node's AABB
        for (auto& item : items) {
            if (node->Bounds.Contains(ComputeRenderItemBounds(item)) != DISJOINT) {
                node->Items.push_back(item);
            }
        }
        node->IsLeaf = true;
        return;
    }

    // Subdivide into 8 children
    DirectX::XMFLOAT3 center = node->Bounds.Center;
    DirectX::XMFLOAT3 extents = node->Bounds.Extents;
    float halfExtX = extents.x * 0.5f;
    float halfExtY = extents.y * 0.5f;
    float halfExtZ = extents.z * 0.5f;

    // Define 8 child AABBs (standard octant split)
    DirectX::XMFLOAT3 childCenters[8] = {
        {center.x - halfExtX, center.y - halfExtY, center.z - halfExtZ},  // 0: (-x,-y,-z)
        {center.x + halfExtX, center.y - halfExtY, center.z - halfExtZ},  // 1: (+x,-y,-z)
        {center.x - halfExtX, center.y + halfExtY, center.z - halfExtZ},  // 2: (-x,+y,-z)
        {center.x + halfExtX, center.y + halfExtY, center.z - halfExtZ},  // 3: (+x,+y,-z)
        {center.x - halfExtX, center.y - halfExtY, center.z + halfExtZ},  // 4: (-x,-y,+z)
        {center.x + halfExtX, center.y - halfExtY, center.z + halfExtZ},  // 5: (+x,-y,+z)
        {center.x - halfExtX, center.y + halfExtY, center.z + halfExtZ},  // 6: (-x,+y,+z)
        {center.x + halfExtX, center.y + halfExtY, center.z + halfExtZ}   // 7: (+x,+y,+z)
    };

    for (int i = 0; i < 8; ++i) {
        DirectX::BoundingBox childBox(childCenters[i], { halfExtX, halfExtY, halfExtZ });
        node->Children[i] = std::make_unique<OctreeNode>(childBox);
        node->IsLeaf = false;
    }

    // Distribute items to children
    std::vector<std::vector<std::shared_ptr<RenderItem>>> childItems(8);
    for (auto& item : items) {
        bool assigned = false;
        for (int i = 0; i < 8; ++i) {
            if (node->Children[i]->Bounds.Contains(ComputeRenderItemBounds(item)) != DISJOINT) {
                childItems[i].push_back(item); // Добавляем во все пересекающиеся children
                assigned = true;
            }
        }
        if (!assigned) {
            // Fallback to leaf if no child
            node->Items.push_back(item);
        }
    }

    // Recurse
    for (int i = 0; i < 8; ++i) {
        if (!childItems[i].empty()) {
            BuildOctree(childItems[i], node->Children[i].get(), depth + 1);
        }
    }
}

void NeneApp::CollectVisibleItems(OctreeNode* node, const DirectX::BoundingFrustum& frustum, std::vector<std::shared_ptr<RenderItem>>& visibleItems, DirectX::XMVECTOR eyePos) {
    BoundingBox temp = node->Bounds;
    temp.Transform(temp, mView);
    if (!(frustum.Contains(temp) != DISJOINT)) return;  // Fully outside: prune


    if (node->IsLeaf) {

        for (auto& ri : node->Items) {
            if (addedItems.find(ri) != addedItems.end()) {
                continue;
            }
            DirectX::BoundingBox itemBounds = ComputeRenderItemBounds(ri);
            float dist = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&itemBounds.Center), eyePos)));
            if (ri->UseLOD) {
                if (dist > ri->LODThreshold) {
                    ri->CurrentGeo = ri->GeoLow ? ri->GeoLow : ri->Geo;
                }
                else if (dist < ri->LODThreshold / 2) {
                    ri->CurrentGeo = ri->GeoHigh ? ri->GeoHigh : ri->Geo;
                }
                else {
                    ri->CurrentGeo = ri->Geo;
                }
            }
            else {
                ri->CurrentGeo = ri->Geo;
            }
            SubmeshGeometry drawArg;
            if (!ri->SubmeshName.empty())
                drawArg = ri->CurrentGeo->DrawArgs.at(ri->SubmeshName);
            else
                drawArg = ri->CurrentGeo->DrawArgs.begin()->second;
            ri->SelectedIndexCount = drawArg.IndexCount;
            ri->SelectedStartIndexLocation = drawArg.StartIndexLocation;
            ri->SelectedBaseVertexLocation = drawArg.BaseVertexLocation;
            ri->Visible = true;
            visibleItems.push_back(ri);
            addedItems.insert(ri);
        }
        return;
    }

    for (int i = 0; i < 8; ++i) {
        if (node->Children[i]) {
            CollectVisibleItems(node->Children[i].get(), frustum, visibleItems, eyePos);
        }
    }
    //std::cout << "Collected " << visibleItems.size() << " unique visible items in this pass." << std::endl;
}

void NeneApp::RebuildOctreeIfNeeded() {
    if (!mRebuildOctree) return;

    // Compute overall scene bounds
    bool first = true;
    for (auto& ri : mAllRitems) {
        DirectX::BoundingBox itemBounds = ComputeRenderItemBounds(ri);

        if (first) {
            mSceneBounds = itemBounds;
            first = false;
        }
        else {
            mSceneBounds.CreateMerged(mSceneBounds, mSceneBounds, itemBounds);
        }
    }

    mOctreeRoot = std::make_unique<OctreeNode>(mSceneBounds);
    BuildOctree(mAllRitems, mOctreeRoot.get(), 0);
    mRebuildOctree = false;
    std::cout << "Octree rebuilt: " << mAllRitems.size() << " items partitioned." << std::endl;
}

void NeneApp::Update(const GameTimer& gt)
{
    UpdateInputs(gt);
    UpdateCamera(gt);
    UpdateDynamicLights(gt);

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

    // temp solution
    mTessRitems.clear();
    mOpaqueRitems.clear();
    for (auto& ri : mAllRitems)
    {
        Material* mat = ri->Mat;
        if (mat->HasDisplacementMap)
            mTessRitems.push_back(ri);
        else
            mOpaqueRitems.push_back(ri);
    }

    AnimateMaterials(gt);
    UpdateObjectCBs(gt);
    UpdateMaterialCBs(gt);
    UpdateMainPassCB(gt);
    UpdateLightCB(gt);
    //UpdateLightBuffers(gt);
    UpdateVisibleRenderItems();

    // Update particle system (CPU part)
    mParticleSystem->Update(gt.DeltaTime());
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
    ThrowIfFailed(m_commandList->Reset(cmdListAlloc.Get(), mPSOs["deferredGeo"].Get()));

    ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvDescriptorHeap.Get() };
    m_commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

    // Simulate particles (GPU update)
    mParticleSystem->Simulate(m_commandList.Get(), mMainPassCB.DeltaTime);

    DrawDeffered();;

    //DrawForward();
    DrawDeffered();

// UI 
    DrawUI();


    // Indicate a state transition on the resource usage.
    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
        D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    // Done recording commands.
    ThrowIfFailed(m_commandList->Close());
}

// Function to bind to some delegates (in InputDevice, for example)
void NeneApp::SetDelegates()
{

}

void NeneApp::InitCamera()
{
    m_camera.SetPosition(0.0f, 1.0f, -5.0f);
    m_camera.LookAt(m_camera.GetPosition3f(), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT3(0.0f, 1.0f, 0.0f));
    m_camera.UpdateViewMatrix();
}

void NeneApp::BuildDescriptorHeaps()
{
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
    rtvHeapDesc.NumDescriptors = 3;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    rtvHeapDesc.NodeMask = 0;
    ThrowIfFailed(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvGBufferHeap)));

    // Create the SRV heap.
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.NumDescriptors = 128;
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(m_device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));

    mCbvSrvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // Create the SRV heap for ImGui (single descriptor for font texture)
    D3D12_DESCRIPTOR_HEAP_DESC imguiHeapDesc = {};
    imguiHeapDesc.NumDescriptors = 1;
    imguiHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    imguiHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    imguiHeapDesc.NodeMask = 0;
    ThrowIfFailed(m_device->CreateDescriptorHeap(&imguiHeapDesc, IID_PPV_ARGS(&m_imguiSrvHeap)));

    // For GBuffer Depth Buffer
    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
    dsvHeapDesc.NumDescriptors = 1;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    dsvHeapDesc.NodeMask = 0;
    ThrowIfFailed(m_device->CreateDescriptorHeap(
        &dsvHeapDesc, IID_PPV_ARGS(m_dsvGBufferHeap.GetAddressOf())));
}

// Function to preload some textures, if needed
void NeneApp::LoadTextures()
{

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
    hDesc.Offset((INT)mTextures.size() + 4, mCbvSrvDescriptorSize);

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
        aiProcess_GenSmoothNormals);

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
        std::vector<Vertex> subVertices(vertices.begin() + vertexOffset, vertices.begin() + vertexOffset + mesh->mNumVertices);
        submesh.Bounds = ComputeBounds(subVertices);  // TODO: Check bounding box correction
        submesh.MaterialIndex = mesh->mMaterialIndex;

        drawArgs[meshName.C_Str()] = submesh;

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
            int texIndexBefore = (int)mTextures.size() + 4;
            if (LoadTexture(fullPath))
                material->DiffuseSrvHeapIndex = texIndexBefore;
            else
                material->DiffuseSrvHeapIndex = 4;

            // TODO: Logging: std::cout << "Assigned texture index " << material->DiffuseSrvHeapIndex << " for material '" << material->Name << "'" << std::endl;
        } else
        {
            material->DiffuseSrvHeapIndex = 4;  // If no texture - error texture than.
        }

        if (mat->GetTexture(aiTextureType_NORMALS, 0, &texPath) == AI_SUCCESS)
        {
            std::string fullPath = filename.substr(0, filename.find_last_of("/\\")) + "/" + texPath.C_Str();
            int texIndexBefore = (int)mTextures.size() + 4;
            
            if (LoadTexture(fullPath))
            {
                material->NormalSrvHeapIndex = texIndexBefore;
                material->HasNormalMap = true;
            }
            else
                material->NormalSrvHeapIndex = 4;
        } else
        {
            material->NormalSrvHeapIndex = 4;
            // TODO: Logging: std::cout << "No normal map for material " << material->Name << std::endl;
        }

        if (mat->GetTexture(aiTextureType_DISPLACEMENT, 0, &texPath) == AI_SUCCESS)
        {
            std::string fullPath = filename.substr(0, filename.find_last_of("/\\")) + "/" + texPath.C_Str();
            int texIndexBefore = (int)mTextures.size() + 4;
            if (LoadTexture(fullPath))
            {
                material->DisplacementSrvHeapIndex = texIndexBefore;
                material->HasDisplacementMap = true;
            }
            else
                material->DisplacementSrvHeapIndex = 4;
        } else
        {
            material->DisplacementSrvHeapIndex = 4;
            // TODO: logging: std::cout << "No displacement  map for material " << material->Name << std::endl;
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
        
        ritem->Name = drawArg.first + " (OBJ Submesh)";
        ritem->SubmeshName = drawArg.first;
        ritem->UseLOD = false; // disable for obj by default
        ritem->LODThreshold = mLODDistanceThreshold;
        ritem->World = Transform;  // TODO: aiNode transform
        ritem->TexTransform = MathHelper::Identity4x4();
        ritem->NumFramesDirty = gNumFrameResources;
        ritem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        ritem->IndexCount = drawArg.second.IndexCount;
        ritem->StartIndexLocation = drawArg.second.StartIndexLocation;
        ritem->BaseVertexLocation = drawArg.second.BaseVertexLocation;

        ritem->CurrentGeo = ritem->Geo;
        ritem->SelectedIndexCount = ritem->IndexCount;
        ritem->SelectedStartIndexLocation = ritem->StartIndexLocation;
        ritem->SelectedBaseVertexLocation = ritem->BaseVertexLocation;

        mModelRenderItems[drawArg.first] = std::move(ritem);
        mAllRitems.push_back(mModelRenderItems[drawArg.first]);
        mOpaqueRitems.push_back(mAllRitems.back());
    }
    

    std::cout << "Total RenderItems after LoadObjModel: " << mAllRitems.size() << std::endl;
    std::cout << "OpaqueRitems size: " << mOpaqueRitems.size() << std::endl << std::endl;;
    
    mGeometries[geo->Name] = std::move(geo);
}


void NeneApp::BuildRootSignature()
{
    CD3DX12_DESCRIPTOR_RANGE texTables[3];
    texTables[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); // Diffuse
    texTables[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1); // Normal
    texTables[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2); // Displacement

    // Root parameter can be a table, root descriptor or root constants.
    CD3DX12_ROOT_PARAMETER slotRootParameter[6];

    // Perfomance TIP: Order from most frequent to least frequent.
    slotRootParameter[0].InitAsDescriptorTable(1, &texTables[0], D3D12_SHADER_VISIBILITY_PIXEL); // Diffuse
    slotRootParameter[1].InitAsDescriptorTable(1, &texTables[1], D3D12_SHADER_VISIBILITY_PIXEL); // Normal
    slotRootParameter[2].InitAsDescriptorTable(1, &texTables[2], D3D12_SHADER_VISIBILITY_ALL); // Displacement
    slotRootParameter[3].InitAsConstantBufferView(0);
    slotRootParameter[4].InitAsConstantBufferView(1);
    slotRootParameter[5].InitAsConstantBufferView(2);

    auto staticSamplers = GetStaticSamplers();

    // A root signature is an array of root parameters.
    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(6, slotRootParameter,
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

    CD3DX12_DESCRIPTOR_RANGE gbufferTable;
    gbufferTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4, 0); // 3 G-Buffer + 1 Depth

    // Root parameter can be a table, root descriptor or root constants.
    CD3DX12_ROOT_PARAMETER deffered_slotRootParameter[3];

    // Perfomance TIP: Order from most frequent to least frequent.
    deffered_slotRootParameter[0].InitAsConstantBufferView(0);
    deffered_slotRootParameter[1].InitAsConstantBufferView(1);
    deffered_slotRootParameter[2].InitAsDescriptorTable(1, &gbufferTable, D3D12_SHADER_VISIBILITY_ALL);

    // A root signature is an array of root parameters.
    CD3DX12_ROOT_SIGNATURE_DESC deffered_rootSigDesc(3, deffered_slotRootParameter,
        (UINT)staticSamplers.size(), staticSamplers.data(),
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    hr = D3D12SerializeRootSignature(&deffered_rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
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
        IID_PPV_ARGS(m_defferedRootSignature.GetAddressOf())));
}

void NeneApp::BuildShadersAndInputLayout()
{
    // TODO: Fix Tesselator shader to work with light again
    //mShaders["DefaultTessVS"] = d3dUtil::CompileShader(L"Shaders\\DefaultTessellation.hlsl", nullptr, "VSMain", "vs_5_1");
    //mShaders["DefaultTessHS"] = d3dUtil::CompileShader(L"Shaders\\DefaultTessellation.hlsl", nullptr, "HSMain", "hs_5_1");
    //mShaders["DefaultTessDS"] = d3dUtil::CompileShader(L"Shaders\\DefaultTessellation.hlsl", nullptr, "DSMain", "ds_5_1");
    //mShaders["DefaultTessPS"] = d3dUtil::CompileShader(L"Shaders\\DefaultTessellation.hlsl", nullptr, "PSMain", "ps_5_1");

    mShaders["deferredGeoVS"] = d3dUtil::CompileShader(L"Shaders\\GeometryPass.hlsl", nullptr, "VS", "vs_5_1");
    mShaders["deferredGeoPS"] = d3dUtil::CompileShader(L"Shaders\\GeometryPass.hlsl", nullptr, "PS", "ps_5_1");
    mShaders["deferredLightVS"] = d3dUtil::CompileShader(L"Shaders\\DeferredLight.hlsl", nullptr, "VS", "vs_5_1");
    mShaders["deferredLightPS"] = d3dUtil::CompileShader(L"Shaders\\DeferredLight.hlsl", nullptr, "PS", "ps_5_1");
    mShaders["deferredLightPSDebug"] = d3dUtil::CompileShader(L"Shaders\\DeferredLight.hlsl", nullptr, "PS_debug", "ps_5_1");

    mInputLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };
}

void NeneApp::BuildBoxLightGeometry()
{
    GeometryGenerator geoGen;
    GeometryGenerator::MeshData box = geoGen.CreateBox(1.0f, 1.0f, 1.0f, 0);

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
    boxSubmesh.Bounds = ComputeBounds(vertices);

    std::vector<std::uint16_t> indices = box.GetIndices16();

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
    const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

    auto geo = std::make_unique<MeshGeometry>();
    geo->Name = "lightBox";

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

    geo->DrawArgs["lightBox"] = boxSubmesh;

    mGeometries[geo->Name] = std::move(geo);
}

void NeneApp::BuildSphereLightGeometry()
{
    GeometryGenerator geoGen;
    GeometryGenerator::MeshData box = geoGen.CreateSphere(1.0f, 8, 5);

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
    boxSubmesh.Bounds = ComputeBounds(vertices);

    std::vector<std::uint16_t> indices = box.GetIndices16();

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
    const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

    auto geo = std::make_unique<MeshGeometry>();
    geo->Name = "lightSphere";

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

    geo->DrawArgs["lightSphere"] = boxSubmesh;

    mGeometries[geo->Name] = std::move(geo);
}

void NeneApp::BuildQuadLightGeometry()
{
    GeometryGenerator geoGen;
    GeometryGenerator::MeshData box = geoGen.CreateQuad(-1.0f, 1.0f, 2.0f, 2.0f, 0.0f);

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
    boxSubmesh.Bounds = ComputeBounds(vertices);

    std::vector<std::uint16_t> indices = box.GetIndices16();

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
    const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

    auto geo = std::make_unique<MeshGeometry>();
    geo->Name = "lightQuad";

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

    geo->DrawArgs["lightQuad"] = boxSubmesh;

    mGeometries[geo->Name] = std::move(geo);
}

void NeneApp::BuildCylinderLightGeometry()
{
    GeometryGenerator geoGen;
    GeometryGenerator::MeshData box = geoGen.CreateCylinder(1.0f, 0.0f, 2.0f, 32, 1);

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
    boxSubmesh.Bounds = ComputeBounds(vertices);

    std::vector<std::uint16_t> indices = box.GetIndices16();

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
    const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

    auto geo = std::make_unique<MeshGeometry>();
    geo->Name = "lightCylinder";

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

    geo->DrawArgs["lightCylinder"] = boxSubmesh;

    mGeometries[geo->Name] = std::move(geo);
}

void NeneApp::BuildLightGeometries()
{
    BuildBoxLightGeometry();
    BuildSphereLightGeometry();
    BuildQuadLightGeometry();
    BuildCylinderLightGeometry();
}

void NeneApp::BuildBoxGeometry()
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
    boxSubmesh.Bounds = ComputeBounds(vertices);

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

void NeneApp::BuildManyBoxes(UINT count)
{
    // Ensure box geometry exists
    if (mGeometries.find("boxGeo") == mGeometries.end())
    {
        BuildBoxGeometry();
    }

    // Create low and high LOD geometries for boxes
    GeometryGenerator geoGen;
    GeometryGenerator::MeshData boxLow = geoGen.CreateBox(1.0f, 1.0f, 1.0f, 0); // Low detail (no subdivisions)
    GeometryGenerator::MeshData boxHigh = geoGen.CreateBox(1.0f, 1.0f, 1.0f, 5); // High detail (more subdivisions)

    // Create low LOD geometry
    SubmeshGeometry boxLowSubmesh;
    boxLowSubmesh.IndexCount = (UINT)boxLow.Indices32.size();
    boxLowSubmesh.StartIndexLocation = 0;
    boxLowSubmesh.BaseVertexLocation = 0;
    std::vector<Vertex> verticesLow(boxLow.Vertices.size());
    for (size_t i = 0; i < boxLow.Vertices.size(); ++i)
    {
        verticesLow[i].Pos = boxLow.Vertices[i].Position;
        verticesLow[i].Normal = boxLow.Vertices[i].Normal;
        verticesLow[i].TexC = boxLow.Vertices[i].TexC;
    }
    boxLowSubmesh.Bounds = ComputeBounds(verticesLow);
    std::vector<std::uint16_t> indicesLow = boxLow.GetIndices16();
    const UINT vbByteSizeLow = (UINT)verticesLow.size() * sizeof(Vertex);
    const UINT ibByteSizeLow = (UINT)indicesLow.size() * sizeof(std::uint16_t);
    auto geoLow = std::make_unique<MeshGeometry>();
    geoLow->Name = "boxGeoLow";
    ThrowIfFailed(D3DCreateBlob(vbByteSizeLow, &geoLow->VertexBufferCPU));
    CopyMemory(geoLow->VertexBufferCPU->GetBufferPointer(), verticesLow.data(), vbByteSizeLow);
    ThrowIfFailed(D3DCreateBlob(ibByteSizeLow, &geoLow->IndexBufferCPU));
    CopyMemory(geoLow->IndexBufferCPU->GetBufferPointer(), indicesLow.data(), ibByteSizeLow);
    geoLow->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(m_device.Get(), m_commandList.Get(), verticesLow.data(), vbByteSizeLow, geoLow->VertexBufferUploader);
    geoLow->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(m_device.Get(), m_commandList.Get(), indicesLow.data(), ibByteSizeLow, geoLow->IndexBufferUploader);
    geoLow->VertexByteStride = sizeof(Vertex);
    geoLow->VertexBufferByteSize = vbByteSizeLow;
    geoLow->IndexFormat = DXGI_FORMAT_R16_UINT;
    geoLow->IndexBufferByteSize = ibByteSizeLow;
    geoLow->DrawArgs["boxLow"] = boxLowSubmesh;
    mGeometries[geoLow->Name] = std::move(geoLow);

    // Create high LOD geometry
    SubmeshGeometry boxHighSubmesh;
    boxHighSubmesh.IndexCount = (UINT)boxHigh.Indices32.size();
    boxHighSubmesh.StartIndexLocation = 0;
    boxHighSubmesh.BaseVertexLocation = 0;
    std::vector<Vertex> verticesHigh(boxHigh.Vertices.size());
    for (size_t i = 0; i < boxHigh.Vertices.size(); ++i)
    {
        verticesHigh[i].Pos = boxHigh.Vertices[i].Position;
        verticesHigh[i].Normal = boxHigh.Vertices[i].Normal;
        verticesHigh[i].TexC = boxHigh.Vertices[i].TexC;
    }
    boxHighSubmesh.Bounds = ComputeBounds(verticesHigh);
    std::vector<std::uint16_t> indicesHigh = boxHigh.GetIndices16();
    const UINT vbByteSizeHigh = (UINT)verticesHigh.size() * sizeof(Vertex);
    const UINT ibByteSizeHigh = (UINT)indicesHigh.size() * sizeof(std::uint16_t);
    auto geoHigh = std::make_unique<MeshGeometry>();
    geoHigh->Name = "boxGeoHigh";
    ThrowIfFailed(D3DCreateBlob(vbByteSizeHigh, &geoHigh->VertexBufferCPU));
    CopyMemory(geoHigh->VertexBufferCPU->GetBufferPointer(), verticesHigh.data(), vbByteSizeHigh);
    ThrowIfFailed(D3DCreateBlob(ibByteSizeHigh, &geoHigh->IndexBufferCPU));
    CopyMemory(geoHigh->IndexBufferCPU->GetBufferPointer(), indicesHigh.data(), ibByteSizeHigh);
    geoHigh->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(m_device.Get(), m_commandList.Get(), verticesHigh.data(), vbByteSizeHigh, geoHigh->VertexBufferUploader);
    geoHigh->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(m_device.Get(), m_commandList.Get(), indicesHigh.data(), ibByteSizeHigh, geoHigh->IndexBufferUploader);
    geoHigh->VertexByteStride = sizeof(Vertex);
    geoHigh->VertexBufferByteSize = vbByteSizeHigh;
    geoHigh->IndexFormat = DXGI_FORMAT_R16_UINT;
    geoHigh->IndexBufferByteSize = ibByteSizeHigh;
    geoHigh->DrawArgs["boxHigh"] = boxHighSubmesh;
    mGeometries[geoHigh->Name] = std::move(geoHigh);

    // Ensure woodCrate material exists
    if (mMaterials.find("woodCrate") == mMaterials.end())
    {
        auto mat = std::make_unique<Material>();
        mat->Name = "woodCrate";
        mat->MatCBIndex = (int)mMaterials.size();
        mat->DiffuseSrvHeapIndex = (int)mTextures.size();
        LoadTexture("assets/textures/luna_textures/WoodCrate01.dds");
        mat->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
        mat->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
        mat->Roughness = 0.2f;
        mMaterials["woodCrate"] = std::move(mat);
    }

    // Random number generation for placement and scale
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> posDist(-100.0f, 100.0f); // Large area for culling test
    std::uniform_real_distribution<float> scaleDist(0.5f, 2.0f);     // Random scale for variety
    std::uniform_real_distribution<float> rotDist(-XM_PI, XM_PI);    // Random rotation for visual complexity

    // Create 'count' boxes with randomized positions, scales, and rotations
    for (UINT i = 0; i < count; ++i)
    {
        auto boxRitem = std::make_shared<RenderItem>();
        boxRitem->Name = "Box_" + std::to_string(i);
        boxRitem->ObjCBIndex = (int)mAllRitems.size();
        boxRitem->Mat = mMaterials["woodCrate"].get();

        // Random transform
        float scale = scaleDist(gen);
        float rotY = rotDist(gen);
        Matrix world = Matrix::CreateScale(scale) *
            Matrix::CreateRotationY(rotY) *
            Matrix::CreateTranslation(posDist(gen), posDist(gen), posDist(gen));
        boxRitem->World = world;

        // Assign geometries for LOD
        boxRitem->Geo = mGeometries["boxGeo"].get();
        boxRitem->GeoLow = mGeometries["boxGeoLow"].get();
        boxRitem->GeoHigh = mGeometries["boxGeoHigh"].get();
        boxRitem->CurrentGeo = boxRitem->Geo;
        boxRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        boxRitem->IndexCount = boxRitem->Geo->DrawArgs["box"].IndexCount;
        boxRitem->StartIndexLocation = boxRitem->Geo->DrawArgs["box"].StartIndexLocation;
        boxRitem->BaseVertexLocation = boxRitem->Geo->DrawArgs["box"].BaseVertexLocation;
        boxRitem->SelectedIndexCount = boxRitem->IndexCount;
        boxRitem->SelectedStartIndexLocation = boxRitem->StartIndexLocation;
        boxRitem->SelectedBaseVertexLocation = boxRitem->BaseVertexLocation;
        boxRitem->UseLOD = true;
        boxRitem->LODThreshold = 50.0f;
        boxRitem->Visible = true;

        mAllRitems.push_back(std::move(boxRitem));
    }

    std::cout << "Created " << count << " box render items with randomized positions, scales, and LOD." << std::endl;
}

void NeneApp::BuildDisplacementTestGeometry()
{
    GeometryGenerator geoGen;
    GeometryGenerator::MeshData sphere = geoGen.CreateSphere(1.f, 12, 12);

    SubmeshGeometry sphereSubmesh;
    sphereSubmesh.StartIndexLocation = 0;
    sphereSubmesh.BaseVertexLocation = 0;

    std::vector<Vertex> vertices(sphere.Vertices.size());
    for (size_t i = 0; i < sphere.Vertices.size(); ++i)
    {
        vertices[i].Pos = sphere.Vertices[i].Position;
        vertices[i].Normal = sphere.Vertices[i].Normal;
        vertices[i].TexC = sphere.Vertices[i].TexC;
        vertices[i].TangentU = sphere.Vertices[i].TangentU;
    }
    sphereSubmesh.Bounds = BoundingBox(XMFLOAT3(0.f, 0.f, 0.f), XMFLOAT3(0.5f, 0.5f, 0.5f));

    uint32_t sliceCount = 12;
    uint32_t stackCount = 12;
    uint32_t quadCount = sliceCount * (stackCount - 1);
    std::vector<std::uint16_t> indices;
    indices.reserve(quadCount * 4);

    // i=0..stackCount-2 (stacks), j=0..sliceCount-1 (slices)
    for (uint32_t i = 0; i < stackCount - 1; ++i)
    {
        for (uint32_t j = 0; j < sliceCount; ++j)
        {
            uint16_t bottomLeft = 1 + i * sliceCount + j;
            uint16_t bottomRight = 1 + i * sliceCount + (j + 1) % sliceCount;
            uint16_t topRight = 1 + (i + 1) * sliceCount + (j + 1) % sliceCount;
            uint16_t topLeft = 1 + (i + 1) * sliceCount + j;

            // bottom-left, bottom-right, top-right, top-left (CCW, для UV в DS)
            indices.push_back(bottomLeft);
            indices.push_back(bottomRight);
            indices.push_back(topRight);
            indices.push_back(topLeft);
        }
    }

    sphereSubmesh.IndexCount = (UINT)indices.size();

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
    const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

    auto geo = std::make_unique<MeshGeometry>();
    geo->Name = "tessSphereGeo";

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

    geo->DrawArgs["tessSphere"] = sphereSubmesh;

    mGeometries[geo->Name] = std::move(geo);
}

void NeneApp::ShootLight(SimpleMath::Vector3 position, SimpleMath::Vector3 size, SimpleMath::Vector3 velocity, SimpleMath::Color color, float linearDamping)
{
    std::cout << "Shoot Light!" << std::endl;

    auto sphereGeo = mGeometries["lightSphere"].get();
    auto pointRI = std::make_shared<LightItem>();
    pointRI->Name = "Dynamic Point Light " + m_DynamicLights.size();
    pointRI->lightType = LightTypes::POINTLIGHT;
    pointRI->light.Position = position;
    pointRI->light.FalloffStart = 1.0f;
    pointRI->light.FalloffEnd = 10.0f;
    pointRI->light.Strength = { color.R(), color.G(), color.B() };
    pointRI->LightCBIndex = (UINT)mLightRitems.size();
    pointRI->Geo = sphereGeo;
    pointRI->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    pointRI->IndexCount = sphereGeo->DrawArgs["lightSphere"].IndexCount;
    pointRI->StartIndexLocation = sphereGeo->DrawArgs["lightSphere"].StartIndexLocation;
    pointRI->BaseVertexLocation = sphereGeo->DrawArgs["lightSphere"].BaseVertexLocation;
    pointRI->PSOType = "DeferredLightDepthOff";
    pointRI->Visible = true;
    pointRI->NumFramesDirtyLight = gNumFrameResources;
    pointRI->IsDrawingDebugGeometry = true;

    auto dynamicLight = std::make_shared<DynamicLight>();
    dynamicLight->lightRI = std::move(pointRI);
    dynamicLight->velocity = velocity;
    dynamicLight->linearDamping = linearDamping;

    mLightRitems.push_back(dynamicLight->lightRI);
    m_DynamicLights.push_back(dynamicLight);
}

void NeneApp::BuildPlane(float width, float height, UINT x, UINT y, const std::string& meshName, const std::string& matName, const Matrix& transform, D3D12_PRIMITIVE_TOPOLOGY type)
{
    GeometryGenerator geoGen;
    GeometryGenerator::MeshData plane;
    if (type == D3D_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST)
        plane = geoGen.CreateGridQuad(width, height, x, y);
    else
        plane = geoGen.CreateGrid(width, height, x, y);
    SubmeshGeometry moutain;
    moutain.IndexCount = (UINT)plane.Indices32.size();
    moutain.StartIndexLocation = 0;
    moutain.BaseVertexLocation = 0;

    std::vector<Vertex> vertices(plane.Vertices.size());

    for (size_t i = 0; i < plane.Vertices.size(); ++i)
    {
        vertices[i].Pos = plane.Vertices[i].Position;
        vertices[i].Normal = plane.Vertices[i].Normal;
        vertices[i].TexC = plane.Vertices[i].TexC;
        vertices[i].TangentU = plane.Vertices[i].TangentU;
    }
    moutain.Bounds = ComputeBounds(vertices);

    std::vector<std::uint32_t> indices = plane.Indices32;

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
    const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint32_t);

    auto geo = std::make_unique<MeshGeometry>();
    geo->Name = meshName + "Geo";

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
    geo->IndexFormat = DXGI_FORMAT_R32_UINT;
    geo->IndexBufferByteSize = ibByteSize;

    geo->DrawArgs[meshName] = moutain;

    mGeometries[geo->Name] = std::move(geo);

    auto ri_plane = std::make_shared<RenderItem>();
    ri_plane->Name = meshName;
    ri_plane->ObjCBIndex = mAllRitems.size();
    ri_plane->Mat = mMaterials[matName].get();
    ri_plane->World = transform;
    ri_plane->Geo = mGeometries[meshName + "Geo"].get();
    ri_plane->PrimitiveType = type;
    ri_plane->IndexCount = ri_plane->Geo->DrawArgs[meshName].IndexCount;
    ri_plane->StartIndexLocation = ri_plane->Geo->DrawArgs[meshName].StartIndexLocation;
    ri_plane->BaseVertexLocation = ri_plane->Geo->DrawArgs[meshName].BaseVertexLocation;
    mAllRitems.push_back(std::move(ri_plane));
}

void NeneApp::BuildPSOs()
{
#pragma region OldRealization (Deprecated)
    /*
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

    //
    // PSO for objects with normal map.
    //
    D3D12_GRAPHICS_PIPELINE_STATE_DESC normalPsoDesc = opaquePsoDesc;
    normalPsoDesc.VS = { reinterpret_cast<BYTE*>(mShaders["normalVS"]->GetBufferPointer()), mShaders["normalVS"]->GetBufferSize() };
    normalPsoDesc.PS = { reinterpret_cast<BYTE*>(mShaders["normalPS"]->GetBufferPointer()), mShaders["normalPS"]->GetBufferSize() };
    ThrowIfFailed(m_device->CreateGraphicsPipelineState(&normalPsoDesc, IID_PPV_ARGS(&mPSOs["normal"])));
    
    //
    // PSO for objects with displacement and normal  map.
    //
    D3D12_GRAPHICS_PIPELINE_STATE_DESC tessDesc = normalPsoDesc;
    tessDesc.VS = { reinterpret_cast<BYTE*>(mShaders["DefaultTessVS"]->GetBufferPointer()), mShaders["DefaultTessVS"]->GetBufferSize() };
    tessDesc.HS = { reinterpret_cast<BYTE*>(mShaders["DefaultTessHS"]->GetBufferPointer()), mShaders["DefaultTessHS"]->GetBufferSize() };
    tessDesc.DS = { reinterpret_cast<BYTE*>(mShaders["DefaultTessDS"]->GetBufferPointer()), mShaders["DefaultTessDS"]->GetBufferSize() };
    tessDesc.PS = { reinterpret_cast<BYTE*>(mShaders["DefaultTessPS"]->GetBufferPointer()), mShaders["DefaultTessPS"]->GetBufferSize() };
    tessDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
    ThrowIfFailed(m_device->CreateGraphicsPipelineState(&tessDesc, IID_PPV_ARGS(&mPSOs["tesselation"])));

    D3D12_GRAPHICS_PIPELINE_STATE_DESC tessWireframePsoDesc = tessDesc;
    tessWireframePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
    ThrowIfFailed(m_device->CreateGraphicsPipelineState(&tessWireframePsoDesc, IID_PPV_ARGS(&mPSOs["tesselation_wireframe"])));
    */
#pragma endregion

    // PSO для Geometry Pass
    D3D12_GRAPHICS_PIPELINE_STATE_DESC deferredGeoPsoDesc = {};
    deferredGeoPsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
    deferredGeoPsoDesc.pRootSignature = mRootSignature.Get();
    deferredGeoPsoDesc.VS = { reinterpret_cast<BYTE*>(mShaders["deferredGeoVS"]->GetBufferPointer()), mShaders["deferredGeoVS"]->GetBufferSize() };
    deferredGeoPsoDesc.PS = { reinterpret_cast<BYTE*>(mShaders["deferredGeoPS"]->GetBufferPointer()), mShaders["deferredGeoPS"]->GetBufferSize() };
    deferredGeoPsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    deferredGeoPsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    deferredGeoPsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    deferredGeoPsoDesc.SampleMask = UINT_MAX;
    deferredGeoPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    deferredGeoPsoDesc.NumRenderTargets = 3;
    deferredGeoPsoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;      // Position
    deferredGeoPsoDesc.RTVFormats[1] = DXGI_FORMAT_R32G32B32A32_FLOAT;  // Normal
    deferredGeoPsoDesc.RTVFormats[2] = DXGI_FORMAT_R32_FLOAT;           // Roughness
    deferredGeoPsoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    deferredGeoPsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
    deferredGeoPsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
    ThrowIfFailed(m_device->CreateGraphicsPipelineState(&deferredGeoPsoDesc, IID_PPV_ARGS(&mPSOs["deferredGeo"])));

    // PSO для Lighting Pass
    D3D12_INPUT_ELEMENT_DESC lightInputLayout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };
    // Deferred light PSO: No depth test (for dir quad, full-screen)
    D3D12_GRAPHICS_PIPELINE_STATE_DESC deferredLightOffDesc = {};
    deferredLightOffDesc.InputLayout = { lightInputLayout, _countof(lightInputLayout) };
    deferredLightOffDesc.VS = { reinterpret_cast<BYTE*>(mShaders["deferredLightVS"]->GetBufferPointer()), mShaders["deferredLightVS"]->GetBufferSize() };
    deferredLightOffDesc.PS = { reinterpret_cast<BYTE*>(mShaders["deferredLightPS"]->GetBufferPointer()), mShaders["deferredLightPS"]->GetBufferSize() };
    deferredLightOffDesc.pRootSignature = m_defferedRootSignature.Get();
    deferredLightOffDesc.RTVFormats[0] = m_backBufferFormat;
    deferredLightOffDesc.NumRenderTargets = 1;
    deferredLightOffDesc.SampleDesc.Count = 1;
    deferredLightOffDesc.SampleMask = UINT_MAX;
    deferredLightOffDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    deferredLightOffDesc.DSVFormat = m_depthStencilFormat;
    deferredLightOffDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    deferredLightOffDesc.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT;  // For full-screen

    D3D12_RENDER_TARGET_BLEND_DESC rtBlendDesc = {};
    rtBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;;	// RGB + A
    rtBlendDesc.BlendEnable = TRUE;
    rtBlendDesc.LogicOpEnable = FALSE;
    rtBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
    rtBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    rtBlendDesc.SrcBlend = D3D12_BLEND_ONE;								// src * 1
    rtBlendDesc.DestBlend = D3D12_BLEND_ONE;							// dest * 1
    rtBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
    rtBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;

    D3D12_BLEND_DESC blendDesc = {};
    blendDesc.AlphaToCoverageEnable = FALSE;
    blendDesc.IndependentBlendEnable = FALSE;
    blendDesc.RenderTarget[0] = rtBlendDesc;
    deferredLightOffDesc.BlendState = blendDesc;
    deferredLightOffDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    deferredLightOffDesc.DepthStencilState.DepthEnable = FALSE;  // No depth for full-screen
    deferredLightOffDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    ThrowIfFailed(m_device->CreateGraphicsPipelineState(&deferredLightOffDesc, IID_PPV_ARGS(&mPSOs["DeferredLightDepthOff"])));

    // Deferred light PSO: with depth (NOT USED)
    auto deferredLightOnDesc = deferredLightOffDesc;
    deferredLightOnDesc.DepthStencilState.DepthEnable = TRUE;
    deferredLightOnDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    deferredLightOnDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;  // Don't alter G-depth
    deferredLightOnDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;  // Render inside volume if camera in it
    deferredLightOnDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
    ThrowIfFailed(m_device->CreateGraphicsPipelineState(&deferredLightOnDesc, IID_PPV_ARGS(&mPSOs["DeferredLightDepthOn"])));

    D3D12_GRAPHICS_PIPELINE_STATE_DESC lightQUADPsoDesc = deferredLightOffDesc;
    lightQUADPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
    lightQUADPsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    ThrowIfFailed(m_device->CreateGraphicsPipelineState(&lightQUADPsoDesc, IID_PPV_ARGS(&mPSOs["lightQUADPsoDesc"])));
   
    D3D12_GRAPHICS_PIPELINE_STATE_DESC lightShapesPsoDesc = deferredLightOffDesc;
    lightShapesPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    lightShapesPsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
    D3D12_DEPTH_STENCIL_DESC dsDesc = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    dsDesc.DepthEnable = FALSE;
    dsDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    dsDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    lightShapesPsoDesc.DepthStencilState = dsDesc;
    lightShapesPsoDesc.PS = { reinterpret_cast<BYTE*>(mShaders["deferredLightPSDebug"]->GetBufferPointer()),
                        mShaders["deferredLightPSDebug"]->GetBufferSize() };

    ThrowIfFailed(m_device->CreateGraphicsPipelineState(&lightShapesPsoDesc, IID_PPV_ARGS(&mPSOs["lightingShapes"])));
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
    //hDesc.Offset(4, mCbvSrvDescriptorSize); // For GBuffer size
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
            1, (UINT)mAllRitems.size(), (UINT)mMaterials.size(), (UINT)mLightRitems.size() + MAX_DYNAMIC_LIGHTS));
    }
}

// Prebuilded materials
void NeneApp::BuildMaterials()
{
    auto mat = std::make_unique<Material>();
    mat->Name = "error";
    mat->MatCBIndex = (int)mMaterials.size();
    mat->DiffuseSrvHeapIndex = (int)mTextures.size() + 4;
    LoadTexture("assets/textures/texture_error.dds");
    mat->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    mat->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
    mat->Roughness = 0.2f;
    mMaterials["error"] = std::move(mat);


    mat = std::make_unique<Material>();
    mat->Name = "woodCrate";
    mat->MatCBIndex = (int)mMaterials.size();
    mat->DiffuseSrvHeapIndex = (int)mTextures.size() + 4;;
    LoadTexture("assets/textures/luna_textures/WoodCrate01.dds");
    mat->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    mat->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
    mat->Roughness = 0.2f;
    mMaterials["woodCrate"] = std::move(mat);


    mat = std::make_unique<Material>();
    mat->Name = "rock";
    mat->MatCBIndex = (int)mMaterials.size();
    mat->DiffuseSrvHeapIndex = (int)mTextures.size() + 4;
    LoadTexture("assets/textures/for_tesselation/rock.dds");
    mat->NormalSrvHeapIndex = (UINT)mTextures.size();
    mat->HasNormalMap = true;
    LoadTexture("assets/textures/for_tesselation/rock_nmap.dds");
    mat->DisplacementSrvHeapIndex = (UINT)mTextures.size();
    mat->HasDisplacementMap = true;
    LoadTexture("assets/textures/for_tesselation/rock_disp.dds");
    mat->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    mat->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
    mat->Roughness = 0.2f;
    mMaterials["rock"] = std::move(mat);

    mat = std::make_unique<Material>();
    mat->Name = "rock";
    mat->MatCBIndex = (int)mMaterials.size() + 4;
    mat->DiffuseSrvHeapIndex = (int)mTextures.size();
    LoadTexture("assets/textures/for_tesselation/mountain_diff.dds");
    mat->NormalSrvHeapIndex = (UINT)mTextures.size();
    mat->HasNormalMap = true;
    LoadTexture("assets/textures/for_tesselation/mountain_nmap.dds");
    mat->DisplacementSrvHeapIndex = (UINT)mTextures.size();
    mat->HasDisplacementMap = true;
    LoadTexture("assets/textures/for_tesselation/mountain_disp.dds");
    mMaterials["mountain"] = std::move(mat);
}

// Helper function to create a rotation matrix aligning (0,0,1) to the given direction
XMMATRIX CreateRotationMatrixFromDirection(const XMFLOAT3& direction)
{
    XMVECTOR dir = XMLoadFloat3(&direction);
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMVECTOR defaultDir = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);

    // Avoid degenerate case when direction is parallel to up vector
    XMVECTOR dot = XMVector3Dot(dir, up);
    if (XMVectorGetX(XMVectorAbs(dot)) > 0.999f)
        up = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);

    XMMATRIX rotation = XMMatrixLookToLH(XMVectorZero(), dir, up);
    return XMMatrixTranspose(rotation); // HLSL expects row-major matrices
}

void NeneApp::BuildRenderItems()
{
    auto boxRitem = std::make_shared<RenderItem>();
    boxRitem->Name = "Box";
    boxRitem->ObjCBIndex = (UINT)mAllRitems.size() + mLightRitems.size();
    boxRitem->Mat = mMaterials["woodCrate"].get();
    boxRitem->World = Matrix::CreateTranslation(0.0f, 1.0f, 0.0f);
    boxRitem->Geo = mGeometries["boxGeo"].get();
    boxRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    boxRitem->IndexCount = boxRitem->Geo->DrawArgs["box"].IndexCount;
    boxRitem->StartIndexLocation = boxRitem->Geo->DrawArgs["box"].StartIndexLocation;
    boxRitem->BaseVertexLocation = boxRitem->Geo->DrawArgs["box"].BaseVertexLocation;
    mAllRitems.push_back(std::move(boxRitem));

    /*
    auto sphereRitem = std::make_shared<RenderItem>();
    sphereRitem->Name = "TessSphere";
    sphereRitem->ObjCBIndex = (int)mAllRitems.size();
    sphereRitem->Mat = mMaterials["rock"].get();
    sphereRitem->World = Matrix::CreateTranslation(4.0f, 4.0f, 0.0f);
    sphereRitem->Geo = mGeometries["tessSphereGeo"].get();
    sphereRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST;
    sphereRitem->IndexCount = sphereRitem->Geo->DrawArgs["tessSphere"].IndexCount;
    sphereRitem->StartIndexLocation = sphereRitem->Geo->DrawArgs["tessSphere"].StartIndexLocation;
    sphereRitem->BaseVertexLocation = sphereRitem->Geo->DrawArgs["tessSphere"].BaseVertexLocation;
    mAllRitems.push_back(std::move(sphereRitem));*/

    mLightRitems.clear();
    auto quadGeo = mGeometries["lightQuad"].get();
    auto sphereGeo = mGeometries["lightSphere"].get();
    auto cylGeo = mGeometries["lightCylinder"].get();

    // Ambient light RI
    auto ambRI = std::make_shared<LightItem>();
    ambRI->Name = "Ambient Light";
    ambRI->LightCBIndex = (UINT)mLightRitems.size();
    ambRI->Geo = quadGeo;
    ambRI->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    ambRI->IndexCount = quadGeo->DrawArgs["lightQuad"].IndexCount;
    ambRI->StartIndexLocation = quadGeo->DrawArgs["lightQuad"].StartIndexLocation;
    ambRI->BaseVertexLocation = quadGeo->DrawArgs["lightQuad"].BaseVertexLocation;
    ambRI->PSOType = "lightQUADPsoDesc";
    ambRI->Visible = true;
    ambRI->NumFramesDirtyLight = gNumFrameResources;
    ambRI->lightType = LightTypes::AMBIENT;
    ambRI->light.Strength = { 0.05f, 0.05f, 0.05f };
    mLightRitems.push_back(ambRI);

    // Dir light RI
    auto dirRI = std::make_shared<LightItem>();
    dirRI->Name = "Directional Light";
    dirRI->LightCBIndex = (UINT)mLightRitems.size();
    dirRI->Geo = quadGeo;
    dirRI->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    dirRI->IndexCount = quadGeo->DrawArgs["lightQuad"].IndexCount;
    dirRI->StartIndexLocation = quadGeo->DrawArgs["lightQuad"].StartIndexLocation;
    dirRI->BaseVertexLocation = quadGeo->DrawArgs["lightQuad"].BaseVertexLocation;
    dirRI->PSOType = "lightQUADPsoDesc";
    dirRI->Visible = true;
    dirRI->NumFramesDirtyLight = gNumFrameResources;
    dirRI->lightType = LightTypes::DIRECTIONAL;
    dirRI->light.Direction = { -1.0f, -1.0f, 1.0f };
    dirRI->light.Strength = { 0.1f, 0.1f, 0.1f };
    mLightRitems.push_back(dirRI);

    // Point light RI (scale to falloff, pos static)
    auto pointRI = std::make_shared<LightItem>();
    pointRI->Name = "Point Light";
    pointRI->lightType = LightTypes::POINTLIGHT;
    pointRI->light.Position = { 0.0f, 1.0f, 0.0f };
    pointRI->light.FalloffStart = 1.0f;
    pointRI->light.FalloffEnd = 5.0f;
    pointRI->light.Strength = { 1.0f, 0.0f, 0.0f };
    pointRI->LightCBIndex = (UINT)mLightRitems.size();
    pointRI->Geo = sphereGeo;
    pointRI->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    pointRI->IndexCount = sphereGeo->DrawArgs["lightSphere"].IndexCount;
    pointRI->StartIndexLocation = sphereGeo->DrawArgs["lightSphere"].StartIndexLocation;
    pointRI->BaseVertexLocation = sphereGeo->DrawArgs["lightSphere"].BaseVertexLocation;
    pointRI->PSOType = "DeferredLightDepthOff";
    pointRI->Visible = true;
    pointRI->NumFramesDirtyLight = gNumFrameResources;
    mLightRitems.push_back(pointRI);

    // Spot light RI (scale/rotate to match dir/falloff)
    auto spotRI = std::make_shared<LightItem>();
    Matrix spotR = Matrix::CreateRotationX(XM_PI);  // Point down (-Y)
    Matrix spotS = Matrix::CreateScale(5.0f, 25.0f, 5.0f);  // Radius 5, height 25
    Matrix spotT = Matrix::CreateTranslation(10.0f, 10.0f, -5.0f);
    spotRI->Name = "SpotLight";
    spotRI->lightType = LightTypes::SPOTLIGHT;
    spotRI->light.Position = { 10.0f, 10.0f, -5.0f };
    spotRI->light.FalloffStart = 1.0f;
    spotRI->light.FalloffEnd = 20.0f;
    spotRI->light.Direction = { 0.0f, -1.0f, 0.0f };
    spotRI->light.SpotPower = 10.0f;
    spotRI->light.Strength = { 1.0f, 1.0f, 1.0f };
    spotRI->LightCBIndex = (UINT)mLightRitems.size();
    spotRI->Geo = cylGeo;
    spotRI->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    spotRI->IndexCount = cylGeo->DrawArgs["lightCylinder"].IndexCount;
    spotRI->StartIndexLocation = cylGeo->DrawArgs["lightCylinder"].StartIndexLocation;
    spotRI->BaseVertexLocation = cylGeo->DrawArgs["lightCylinder"].BaseVertexLocation;
    spotRI->PSOType = "DeferredLightDepthOff";
    spotRI->Visible = true;
    spotRI->NumFramesDirtyLight = gNumFrameResources;
    mLightRitems.push_back(spotRI);
    
    std::cout << "Total RenderItems after BuildRenderItems: " << mAllRitems.size() << std::endl;
    std::cout << "Light render items size: " << mLightRitems.size() << std::endl;
}

void NeneApp::BuildPostProcessHeaps()
{

}

void NeneApp::BuildPostProcessResources()
{
    // Create render target for post-processing
    D3D12_RESOURCE_DESC renderTargetDesc{};
    ZeroMemory(&renderTargetDesc, sizeof(D3D12_RESOURCE_DESC));
    renderTargetDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    renderTargetDesc.Alignment = 0;
    renderTargetDesc.Width = m_clientWidth;
    renderTargetDesc.Height = m_clientHeight;
    renderTargetDesc.DepthOrArraySize = 1;
    renderTargetDesc.MipLevels = 1;
    renderTargetDesc.Format = m_backBufferFormat;
    renderTargetDesc.SampleDesc.Count = 1;
    renderTargetDesc.SampleDesc.Quality = 0;
    renderTargetDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    renderTargetDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    D3D12_CLEAR_VALUE clearValue{};
    clearValue.Format = m_backBufferFormat;
    memcpy(clearValue.Color, Colors::Black, sizeof(float) * 4);

    ThrowIfFailed(m_device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &renderTargetDesc,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        &clearValue,
        IID_PPV_ARGS(&mPostProcessRenderTarget)));

    // Create RTV heap
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
    rtvHeapDesc.NumDescriptors = 1;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    rtvHeapDesc.NodeMask = 0;

    ThrowIfFailed(m_device->CreateDescriptorHeap(
        &rtvHeapDesc, IID_PPV_ARGS(&mPostProcessRTVHeap)));

    // Create RTV
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(
        mPostProcessRTVHeap->GetCPUDescriptorHandleForHeapStart());
    m_device->CreateRenderTargetView(
        mPostProcessRenderTarget.Get(), nullptr, rtvHandle);

    // После создания mPostProcessRenderTarget
    CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
    UINT srvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // Offset после GBuffer (4) + текстур
    UINT postProcessSrvOffset = 4 + (UINT)mTextures.size();
    srvHandle.Offset(postProcessSrvOffset, srvDescriptorSize);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = m_backBufferFormat;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.MostDetailedMip = 0;
    m_device->CreateShaderResourceView(
        mPostProcessRenderTarget.Get(), &srvDesc, srvHandle);

    // Сохраните GPU handle для binding в шейдерах (добавьте в класс член, напр. D3D12_GPU_DESCRIPTOR_HANDLE mPostProcessSrvGpuHandle)
    mPostProcessSrvGpuHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart(), postProcessSrvOffset, srvDescriptorSize);
}

void NeneApp::BuildPostProcessSignature()
{
    CD3DX12_DESCRIPTOR_RANGE gbufferTable;
    gbufferTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4, 0); // 3 G-Buffer + 1 Depth
    CD3DX12_DESCRIPTOR_RANGE afterLightTable;
    afterLightTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4); // 3 G-Buffer + 1 Depth

    CD3DX12_ROOT_PARAMETER slotRootParameter[3];
    slotRootParameter[0].InitAsDescriptorTable(1, &gbufferTable, D3D12_SHADER_VISIBILITY_PIXEL);
    slotRootParameter[1].InitAsConstantBufferView(0);
    slotRootParameter[2].InitAsDescriptorTable(1, &afterLightTable, D3D12_SHADER_VISIBILITY_PIXEL);

    // TODO: Make on GetStaticSamplers method
    CD3DX12_STATIC_SAMPLER_DESC sampler(
        0,                                  // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_POINT,     // filter
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,   // addressU
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,   // addressV
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP);  // addressW

    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(3, slotRootParameter,
        1, &sampler,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;
    HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
        serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

    if (errorBlob != nullptr)
    {
        OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    }
    ThrowIfFailed(hr);

    ThrowIfFailed(m_device->CreateRootSignature(
        0,
        serializedRootSig->GetBufferPointer(),
        serializedRootSig->GetBufferSize(),
        IID_PPV_ARGS(&mPostProcessRootSignature)));
}

void NeneApp::BuildPostProcessPSO()
{
    // Compile shaders
    OutputDebugStringA("Compiling Shaders\\BasicPostProcess.hlsl VS\n");
    mPostProcessShaders["postVS"] = d3dUtil::CompileShader(L"Shaders\\BasicPostProcess.hlsl", nullptr, "VS", "vs_5_1");
    OutputDebugStringA("Compiling Shaders\\BasicPostProcess.hlsl PS\n");
    mPostProcessShaders["postPS"] = d3dUtil::CompileShader(L"Shaders\\BasicPostProcess.hlsl", nullptr, "PS", "ps_5_1");

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
    ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
    psoDesc.InputLayout = { nullptr, 0 };
    psoDesc.pRootSignature = mPostProcessRootSignature.Get();
    psoDesc.VS =
    {
        reinterpret_cast<BYTE*>(mPostProcessShaders["postVS"]->GetBufferPointer()),
        mPostProcessShaders["postVS"]->GetBufferSize()
    };
    psoDesc.PS =
    {
        reinterpret_cast<BYTE*>(mPostProcessShaders["postPS"]->GetBufferPointer()),
        mPostProcessShaders["postPS"]->GetBufferSize()
    };
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState.DepthEnable = false;
    psoDesc.DepthStencilState.StencilEnable = false;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = m_backBufferFormat;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleDesc.Quality = 0;

    ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs["PostProcess"])));
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

        cmdList->IASetVertexBuffers(0, 1, &ri->CurrentGeo->VertexBufferView());
        cmdList->IASetIndexBuffer(&ri->CurrentGeo->IndexBufferView());
        cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

        CD3DX12_GPU_DESCRIPTOR_HANDLE diffuseTex(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
        CD3DX12_GPU_DESCRIPTOR_HANDLE normalTex(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
        CD3DX12_GPU_DESCRIPTOR_HANDLE displacementTex(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
        diffuseTex.Offset(ri->Mat->DiffuseSrvHeapIndex, mCbvSrvDescriptorSize);
        normalTex.Offset(ri->Mat->NormalSrvHeapIndex, mCbvSrvDescriptorSize);
        displacementTex.Offset(ri->Mat->DisplacementSrvHeapIndex, mCbvSrvDescriptorSize);
        
        D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize;
        D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = matCB->GetGPUVirtualAddress() + ri->Mat->MatCBIndex * matCBByteSize;

        cmdList->SetGraphicsRootDescriptorTable(0, diffuseTex);
        if (ri->Mat->HasNormalMap)
            cmdList->SetGraphicsRootDescriptorTable(1, normalTex);
        if (ri->Mat->HasDisplacementMap)
            cmdList->SetGraphicsRootDescriptorTable(2, displacementTex);
        cmdList->SetGraphicsRootConstantBufferView(3, objCBAddress);
        cmdList->SetGraphicsRootConstantBufferView(5, matCBAddress);

        cmdList->DrawIndexedInstanced(ri->SelectedIndexCount, 1, ri->SelectedStartIndexLocation, ri->SelectedBaseVertexLocation, 0);
    }
}

void NeneApp::DrawForward()
{
    // Basic objects rendering
    if (!mIsWireframe) {
        m_commandList->SetPipelineState(mPSOs["opaque"].Get());
    }
    else {
        m_commandList->SetPipelineState(mPSOs["opaque_wireframe"].Get());
    }
    DrawRenderItems(m_commandList.Get(), mOpaqueRitems);

    // Objects with displacement map rendering
    if (!mIsWireframe) {
        m_commandList->SetPipelineState(mPSOs["tesselation"].Get());
    }
    else {
        m_commandList->SetPipelineState(mPSOs["tesselation_wireframe"].Get());
    }
    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST);
    DrawRenderItems(m_commandList.Get(), mTessRitems);
}

void NeneApp::DrawDeffered()
{

    // Geometry Pass
    m_commandList->RSSetViewports(1, &m_viewport);
    m_commandList->RSSetScissorRects(1, &m_scissorRect);

    ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvDescriptorHeap.Get() };
    m_commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

    m_gBuffer.BindForGeometryPass(m_commandList.Get());


    m_commandList->SetPipelineState(mPSOs["deferredGeo"].Get());
    m_commandList->SetGraphicsRootSignature(mRootSignature.Get());

    m_commandList->SetGraphicsRootConstantBufferView(4, mCurrFrameResource->PassCB->Resource()->GetGPUVirtualAddress());
    DrawRenderItems(m_commandList.Get(), mOpaqueRitems);

    // Lighting Pass
    m_gBuffer.BindForLightingPass(m_commandList.Get());
    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
        mPostProcessRenderTarget.Get(),
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_RENDER_TARGET));
    auto ppRTV = mPostProcessRTVHeap->GetCPUDescriptorHandleForHeapStart();
    CD3DX12_CPU_DESCRIPTOR_HANDLE ppRTVHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(
        mPostProcessRTVHeap->GetCPUDescriptorHandleForHeapStart(),
        0,
        m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV));
    m_commandList->OMSetRenderTargets(1, &ppRTV, true, &DepthStencilView());
    m_commandList->ClearRenderTargetView(ppRTVHandle, DirectX::Colors::Black, 0, nullptr);

    m_commandList->SetPipelineState(mPSOs["DeferredLightDepthOff"].Get());
    m_commandList->SetGraphicsRootSignature(m_defferedRootSignature.Get());
    auto srvHandles = m_gBuffer.GetSRVs();
    m_commandList->SetGraphicsRootDescriptorTable(2, srvHandles[0]);
    m_commandList->SetGraphicsRootConstantBufferView(0, mCurrFrameResource->PassCB->Resource()->GetGPUVirtualAddress());


    UINT lightCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(LightData));
    auto lightCB = mCurrFrameResource->LightCB->Resource();
    m_commandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    for (const auto& lightItem : mLightRitems)
    {
        D3D12_GPU_VIRTUAL_ADDRESS lightCBAddress = lightCB->GetGPUVirtualAddress() + lightItem->LightCBIndex * lightCBByteSize;
        m_commandList->SetGraphicsRootConstantBufferView(1, lightCBAddress);

        // Set PSO by type
        m_commandList->SetPipelineState(mPSOs[lightItem->PSOType].Get());

        m_commandList->IASetVertexBuffers(0, 1, &lightItem->Geo->VertexBufferView());
        m_commandList->IASetIndexBuffer(&lightItem->Geo->IndexBufferView());
        m_commandList->DrawIndexedInstanced(lightItem->IndexCount, 1, lightItem->StartIndexLocation, lightItem->BaseVertexLocation, 0);
    }
    // Debug rendering
    m_commandList->SetPipelineState(mPSOs["lightingShapes"].Get());
    m_commandList->SetGraphicsRootSignature(m_defferedRootSignature.Get());
    m_commandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    for (const auto& lightItem : mLightRitems)
    {
        if (lightItem->lightType != LightTypes::AMBIENT && lightItem->lightType != LightTypes::DIRECTIONAL && lightItem->IsDrawingDebugGeometry)
        {
            D3D12_GPU_VIRTUAL_ADDRESS lightCBAddress = lightCB->GetGPUVirtualAddress() + lightItem->LightCBIndex * lightCBByteSize;
            m_commandList->SetGraphicsRootConstantBufferView(1, lightCBAddress);

            m_commandList->IASetVertexBuffers(0, 1, &lightItem->Geo->VertexBufferView());
            m_commandList->IASetIndexBuffer(&lightItem->Geo->IndexBufferView());
            m_commandList->DrawIndexedInstanced(lightItem->IndexCount, 1, lightItem->StartIndexLocation, lightItem->BaseVertexLocation, 0);
        }
    }

    // Render particles after lights (additive blend)
    mParticleSystem->Render(m_commandList.Get(), mView, mProj);

    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
        mPostProcessRenderTarget.Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
    // Indicate a state transition on the resource usage.
    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
        D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
    m_commandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, nullptr);


    // Post-Process
    m_commandList->SetPipelineState(mPSOs["PostProcess"].Get());
    m_commandList->SetGraphicsRootSignature(mPostProcessRootSignature.Get());
    m_commandList->SetGraphicsRootDescriptorTable(0, srvHandles[0]);
    m_commandList->SetGraphicsRootConstantBufferView(1, mCurrFrameResource->PassCB->Resource()->GetGPUVirtualAddress());
    m_commandList->SetGraphicsRootDescriptorTable(2, mPostProcessSrvGpuHandle);

    m_commandList->IASetVertexBuffers(0, 0, nullptr);
    m_commandList->IASetIndexBuffer(nullptr);
    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_commandList->DrawInstanced(3, 1, 0, 0);


    m_gBuffer.Unbind(m_commandList.Get());
}

void NeneApp::DrawUI()
{
    m_commandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, nullptr); // Disable depth-stencil for ImGui

    ID3D12DescriptorHeap* ImGui_descriptorHeaps[] = { m_imguiSrvHeap.Get() };
    m_commandList->SetDescriptorHeaps(_countof(ImGui_descriptorHeaps), ImGui_descriptorHeaps);

    m_uiManager.BeginFrame();

    ImGui::Begin("Render Settings");
    ImGui::Checkbox("Wireframe Mode", &mIsWireframe);

    ImGui::Text("Frustum Culling Settings");
    ImGui::Checkbox("Frustum Culling", &mUseFrustumCulling);
    ImGui::SliderFloat("LOD Distance Threshold", &mLODDistanceThreshold, 5.0f, 200.0f);
    ImGui::Text("Visible objects: %d/%d ", (int)mVisibleRitems.size(), (int)mAllRitems.size());

    ImGui::Text("Camera Settings");
    ImGui::SliderFloat("Camera speed", &m_cameraSpeed, 1.0f, 100.0f);
    ImGui::End();


    ImGui::Begin("Render Item");
    if (ImGui::CollapsingHeader("Render Items debug table", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::BeginTable("Render Items", 4, ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Name"); ImGui::TableSetupColumn("Material"); ImGui::TableSetupColumn("Visible"); ImGui::TableSetupColumn("Distance");
            ImGui::TableHeadersRow();
            for (auto& ri : mAllRitems) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::Text("%s", ri->Name.c_str());
                ImGui::TableSetColumnIndex(1); ImGui::Text("%s", ri->Mat->Name.c_str());
                ImGui::TableSetColumnIndex(2); ImGui::Text("%s", ri->Visible ? "true" : "false");
            }
            ImGui::EndTable();
        }
    }

    std::vector<const char*> itemNames;
    for (const auto& ri : mAllRitems)
        itemNames.push_back(ri->Name.empty() ? ("Object " + std::to_string(ri->ObjCBIndex)).c_str() : ri->Name.c_str());

    ImGui::Combo("Select Render Item", &mSelectedRItemIndex, itemNames.data(), (int)itemNames.size());

    if (mSelectedRItemIndex >= 0 && mSelectedRItemIndex < (int)mAllRitems.size())
    {
        auto& ri = mAllRitems[mSelectedRItemIndex];
        ImGui::Text("Editing: %s", ri->Name.c_str());

        // Getting Pos, Rot and Scale of RenderItem object
        Matrix world = XMLoadFloat4x4(&ri->World);
        Vector3 scale, translation;
        Quaternion q;
        world.Decompose(scale, q, translation);
        float pos[3] = { translation.x, translation.y, translation.z };
        float rot[3] = { 0.0f, 0.0f, 0.0f };
        float scl[3] = { scale.x, scale.y, scale.z };

        // quaternion to euler
        float pitch = asinf(-2.0f * (q.x * q.z - q.w * q.y)) * (180.0f / XM_PI);
        float yaw = atan2f(2.0f * (q.y * q.z + q.w * q.x), q.w * q.w - q.x * q.x - q.y * q.y + q.z * q.z) * (180.0f / XM_PI);
        float roll = atan2f(2.0f * (q.x * q.y + q.w * q.z), q.w * q.w + q.x * q.x - q.y * q.y - q.z * q.z) * (180.0f / XM_PI);
        rot[0] = pitch; rot[1] = yaw; rot[2] = roll;

        bool transformChanged = false;
        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
        {
            transformChanged |= ImGui::SliderFloat3("Position", pos, -100.0f, 100.0f, "%.2f");
            transformChanged |= ImGui::SliderFloat3("Rotation (degrees)", rot, -180.0f, 180.0f, "%.2f");
            transformChanged |= ImGui::SliderFloat3("Scale", scl, 0.1f, 10.0f, "%.2f");
        }

        if (transformChanged)
        {
            DirectX::XMMATRIX newWorld = XMMatrixScaling(scl[0], scl[1], scl[2]) *
                XMMatrixRotationRollPitchYaw(rot[0] * (XM_PI / 180.0f), rot[1] * (XM_PI / 180.0f), rot[2] * (XM_PI / 180.0f)) *
                XMMatrixTranslation(pos[0], pos[1], pos[2]);
            XMStoreFloat4x4(&ri->World, newWorld);
            ri->NumFramesDirty = gNumFrameResources;
        }

        bool lodChanged = false;
        if (ImGui::CollapsingHeader("LOD Settings", ImGuiTreeNodeFlags_DefaultOpen))
        {
            lodChanged |= ImGui::Checkbox("Enable LOD", &ri->UseLOD);
            lodChanged |= ImGui::SliderFloat("LOD Threshold", &ri->LODThreshold, 5.0f, 200.0f, "%.1f");

            XMVECTOR eyePos = XMLoadFloat3(&mMainPassCB.EyePosW);
            XMMATRIX worldMat = XMLoadFloat4x4(&ri->World);
            BoundingBox totalBounds = ri->Geo->DrawArgs.begin()->second.Bounds;
            totalBounds.Transform(totalBounds, worldMat);
            XMVECTOR center = XMLoadFloat3(&totalBounds.Center);
            float dist = XMVectorGetX(XMVector3Length(XMVectorSubtract(center, eyePos)));
            std::string currentLODStr = "Basic";
            if (ri->UseLOD) {
                if (dist > ri->LODThreshold) {
                    currentLODStr = (ri->GeoLow ? "Low" : "Basic");
                }
                else {
                    currentLODStr = (ri->GeoHigh ? "High" : "Basic");
                }
            }
            ImGui::Text("Preview LOD (dist=%.1f): %s", dist, currentLODStr.c_str());

            std::vector<const char*> geoNames;
            std::unordered_map<const char*, std::shared_ptr<MeshGeometry>> geoMap;
            for (const auto& ge : mGeometries) {
                geoNames.push_back(ge.first.c_str());
                geoMap[ge.first.c_str()] = ge.second;
            }

            int lowGeoId = 0;
            bool foundLow = false;
            for (int i = 0; i < (int)geoNames.size(); ++i) {
                if (ri->GeoLow == geoMap[geoNames[i]].get()) {
                    lowGeoId = i;
                    foundLow = true;
                    break;
                }
            }
            if (!foundLow) {
                for (int i = 0; i < (int)geoNames.size(); ++i) {
                    if (ri->Geo == geoMap[geoNames[i]].get()) {
                        lowGeoId = i;
                        break;
                    }
                }
            }

            // for High
            int highGeoId = 0;
            bool foundHigh = false;
            for (int i = 0; i < (int)geoNames.size(); ++i) {
                if (ri->GeoHigh == geoMap[geoNames[i]].get()) {
                    highGeoId = i;
                    foundHigh = true;
                    break;
                }
            }
            if (!foundHigh) {
                for (int i = 0; i < (int)geoNames.size(); ++i) {
                    if (ri->Geo == geoMap[geoNames[i]].get()) {
                        highGeoId = i;
                        break;
                    }
                }
            }

            // Combo Low LOD
            if (ImGui::Combo("Geo for Low LOD", &lowGeoId, geoNames.data(), (int)geoNames.size())) {
                lodChanged = true;
                ri->GeoLow = geoMap[geoNames[lowGeoId]].get();
                if (ri->GeoLow && ri->GeoLow->DrawArgs.size() > 0) {
                    ri->SelectedIndexCount = ri->GeoLow->DrawArgs.begin()->second.IndexCount;
                    ri->SelectedStartIndexLocation = ri->GeoLow->DrawArgs.begin()->second.StartIndexLocation;
                    ri->SelectedBaseVertexLocation = ri->GeoLow->DrawArgs.begin()->second.BaseVertexLocation;
                }
            }

            // Combo High LOD
            if (ImGui::Combo("Geo for High LOD", &highGeoId, geoNames.data(), (int)geoNames.size())) {
                lodChanged = true;
                ri->GeoHigh = geoMap[geoNames[highGeoId]].get();
                if (ri->GeoHigh && ri->GeoHigh->DrawArgs.size() > 0) {
                    ri->SelectedIndexCount = ri->GeoHigh->DrawArgs.begin()->second.IndexCount;
                    ri->SelectedStartIndexLocation = ri->GeoHigh->DrawArgs.begin()->second.StartIndexLocation;
                    ri->SelectedBaseVertexLocation = ri->GeoHigh->DrawArgs.begin()->second.BaseVertexLocation;
                }
            }

            if (lodChanged) {
                ri->NumFramesDirty = gNumFrameResources;
            }
        }

        if (ri->Mat && ImGui::CollapsingHeader("Material Settings", ImGuiTreeNodeFlags_DefaultOpen))
        {
            Material* mat = ri->Mat;
            bool matChanged = false;

            matChanged |= ImGui::ColorEdit4("Diffuse Albedo", &mat->DiffuseAlbedo.x);
            matChanged |= ImGui::SliderFloat("Roughness", &mat->Roughness, 0.0f, 1.0f, "%.3f");
            matChanged |= ImGui::SliderFloat3("Fresnel R0", &mat->FresnelR0.x, 0.0f, 1.0f, "%.3f");

            if (mat->HasDisplacementMap)
            {
                matChanged |= ImGui::SliderFloat("Tessellation Factor", &mat->TessellationFactor, 1.0f, 64.0f, "%.1f");
                matChanged |= ImGui::SliderFloat("Displacement Scale", &mat->DisplacementScale, 0.0f, 10.0f, "%.2f");
                matChanged |= ImGui::SliderFloat("Displacement Bias", &mat->DisplacementBias, -5.0f, 5.0f, "%.2f");
            }

            if (matChanged)
            {
                mat->NumFramesDirty = gNumFrameResources;
            }
        }

        if (ImGui::Button("Reset Transform"))
        {
            ri->World = MathHelper::Identity4x4();
            ri->NumFramesDirty = gNumFrameResources;
        }
    }
    else
    {
        ImGui::Text("No Render Item selected");
    }
    ImGui::End();

    if (ImGui::Begin("Lights Control"))
    {
        static int selectedLightIndex = 0;
        if (mLightRitems.empty())
        {
            ImGui::Text("No lights available");
        }
        else
        {
            
            std::vector<const char*> lightNames;
            for (const auto& ri : mLightRitems)
                lightNames.push_back(ri->Name.empty() ? ("Dinamic Light " + std::to_string(ri->LightCBIndex)).c_str() : ri->Name.c_str());
            ImGui::Combo("Select Light", &selectedLightIndex, lightNames.data(), (int)lightNames.size());

            auto* selectedLight = mLightRitems[selectedLightIndex].get();
            if (selectedLight)
            {
                if (selectedLight->lightType != AMBIENT && selectedLight->lightType != DIRECTIONAL) {
                    ImGui::Checkbox("Draw Debug Geometry", &selectedLight->IsDrawingDebugGeometry);
                }

                bool posChanged = false;
                if (selectedLight->lightType != AMBIENT && selectedLight->lightType != DIRECTIONAL) {
                    ImGui::Text("Position:");
                    posChanged |= ImGui::SliderFloat3("Position", &selectedLight->light.Position.x, -10.0f, 10.0f, "%.2f");
                }

                bool colorChanged = false;
                colorChanged |= ImGui::ColorEdit3("Color: ", &selectedLight->light.Strength.x);

                bool directionChanged = false;
                if (selectedLight->lightType == DIRECTIONAL || selectedLight->lightType == SPOTLIGHT) {
                    ImGui::Text("Direction:");
                    directionChanged |= ImGui::SliderFloat3("Direction", &selectedLight->light.Direction.x, -1.0f, 1.0f, "%.1f");
                }


                if (colorChanged || posChanged || directionChanged)
                {
                    selectedLight->NumFramesDirtyLight = gNumFrameResources;
                }

                if (ImGui::Button("Reset Light"))
                {
                    selectedLight->light.Position = XMFLOAT3(0.0f, 0.0f, 0.0f);
                    selectedLight->light.Strength = XMFLOAT3(0.2f, 0.2f, 0.2f);
                    selectedLight->light.Direction = XMFLOAT3(-1.0f, -1.0f, 1.0f);
                    selectedLight->NumFramesDirtyLight = gNumFrameResources;
                }
            }
        }
    }
    ImGui::End();

    if (ImGui::Begin("Particle System Control"))
    {
        ImGui::SliderFloat3("Force", &mParticleSystem->force.x, -20.0f, 20.0f, "%.1f");
        ImGui::Text("Active Particles: %d / %d", mParticleSystem->numParticles, mParticleSystem->maxParticles);
    }
    ImGui::End();

    m_uiManager.Render(m_commandList.Get());
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