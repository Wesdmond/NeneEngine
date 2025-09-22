#include "ParticleSystem.h"
#include <d3dcompiler.h>

ParticleSystem::ParticleSystem(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, ID3D12DescriptorHeap* srvHeap, UINT descriptorOffset, int maxParticles, SimpleMath::Vector3 origin)
    : maxParticles(maxParticles), numParticles(0), origin(origin), accumulatedTime(0.0f), force(0.0f, -9.8f, 0.0f),
    mDevice(device), mInitialCmdList(cmdList), mSrvHeap(srvHeap), mDescriptorOffset(descriptorOffset), mCurrentBuffer(nullptr)
{
    mDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    particleList = new Particle[maxParticles];
    InitializeSystem();
    BuildShadersAndPSOs();
    BuildBuffers();
    CreateDescriptors();
}

ParticleSystem::~ParticleSystem()
{
    delete[] particleList;
}

void ParticleSystem::InitializeSystem()
{
    numParticles = 0;
    accumulatedTime = 0.0f;
    for (int i = 0; i < maxParticles; ++i) {
        InitializeParticle(i);
    }
}

void ParticleSystem::KillSystem()
{
    numParticles = 0;
    accumulatedTime = 0.0f;
    for (int i = 0; i < maxParticles; ++i) {
        particleList[i].energy = 0.0f;
    }
}

void ParticleSystem::InitializeParticle(int index)
{
    Particle& p = particleList[index];
    p.pos = origin;
    p.prevPos = origin;
    p.velocity = SimpleMath::Vector3(0.0f, 0.0f, 0.0f);
    p.acceleration = SimpleMath::Vector3(0.0f, 0.0f, 0.0f);
    p.energy = 0.0f;
    p.size = 1.0f;
    p.sizeDelta = 0.0f;
    p.weight = 1.0f;
    p.weightDelta = 0.0f;
    p.color[0] = 1.0f; p.color[1] = 1.0f; p.color[2] = 1.0f; p.color[3] = 1.0f;
    p.colorDelta[0] = 0.0f; p.colorDelta[1] = 0.0f; p.colorDelta[2] = 0.0f; p.colorDelta[3] = -0.5f;
}

int ParticleSystem::Emit(int numParticlesToEmit)
{
    int emitted = 0;
    for (int i = 0; i < maxParticles && emitted < numParticlesToEmit; ++i) {
        if (particleList[i].energy <= 0.0f) {
            InitializeParticle(i);
            particleList[i].energy = 5.0f; // Время жизни 5 секунд
            particleList[i].velocity = SimpleMath::Vector3((rand() % 100 - 50) / 50.0f, (rand() % 100 + 50) / 50.0f, (rand() % 100 - 50) / 50.0f); // Случайная скорость вверх
            ++emitted;
            ++numParticles;
        }
    }
    return emitted;
}

void ParticleSystem::Update(float elapsedTime)
{
    accumulatedTime += elapsedTime;
    if (accumulatedTime > 0.05f) { // Эмиссия каждые 0.05 секунды
        Emit(5); // Эмитируем 5 частиц
        accumulatedTime = 0.0f;
    }

    // CPU обновление частиц (опционально, но поскольку GPU делает основную работу, здесь только эмиссия)
}

void ParticleSystem::Simulate(ID3D12GraphicsCommandList* cmdList, float deltaTime)
{
    cmdList->SetComputeRootSignature(mComputeRootSig.Get());
    cmdList->SetPipelineState(mComputePSO.Get());

    struct Constants {
        float dt;
        float pad[3];
        SimpleMath::Vector3 force;
        float pad2;
    };
    Constants consts = { deltaTime, {0, 0, 0}, force, 0 };
    ComPtr<ID3D12Resource> constBuffer;
    ThrowIfFailed(mDevice->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(sizeof(Constants)),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&constBuffer)));

    void* data;
    constBuffer->Map(0, nullptr, &data);
    memcpy(data, &consts, sizeof(Constants));
    constBuffer->Unmap(0, nullptr);

    cmdList->SetComputeRootConstantBufferView(0, constBuffer->GetGPUVirtualAddress());
    cmdList->SetComputeRootDescriptorTable(1, mConsumeSRV);
    cmdList->SetComputeRootDescriptorTable(2, mAppendUAV);

    UINT groupCount = (maxParticles + 255) / 256;
    cmdList->Dispatch(groupCount, 1, 1);

    cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mCurrentBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));
    SwapBuffers();
}

void ParticleSystem::Render(ID3D12GraphicsCommandList* cmdList, const SimpleMath::Matrix& view, const SimpleMath::Matrix& proj)
{
    cmdList->SetGraphicsRootSignature(mRenderRootSig.Get());
    cmdList->SetPipelineState(mRenderPSO.Get());

    // Установка viewProj
    struct RenderConstants {
        SimpleMath::Matrix viewProj;
    };
    RenderConstants renderConsts = { view * proj };
    ComPtr<ID3D12Resource> renderConstBuffer;
    ThrowIfFailed(mDevice->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(sizeof(RenderConstants)),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&renderConstBuffer)));

    void* data;
    renderConstBuffer->Map(0, nullptr, &data);
    memcpy(data, &renderConsts, sizeof(RenderConstants));
    renderConstBuffer->Unmap(0, nullptr);

    cmdList->SetGraphicsRootConstantBufferView(0, renderConstBuffer->GetGPUVirtualAddress());
    cmdList->SetGraphicsRootDescriptorTable(1, mConsumeSRV);

    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
    cmdList->IASetVertexBuffers(0, 1, &VertexBufferView());
    cmdList->DrawInstanced(numParticles, 1, 0, 0);
}

void ParticleSystem::BuildBuffers()
{
    UINT bufferSize = maxParticles * sizeof(Particle);

    D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    ThrowIfFailed(mDevice->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
        nullptr,
        IID_PPV_ARGS(&mParticleBuffer1)));

    ThrowIfFailed(mDevice->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
        nullptr,
        IID_PPV_ARGS(&mParticleBuffer2)));

    mCurrentBuffer = mParticleBuffer1.Get();

    ThrowIfFailed(mDevice->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(bufferSize),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&mUploadBuffer)));

    void* data;
    mUploadBuffer->Map(0, nullptr, &data);
    memcpy(data, particleList, bufferSize);
    mUploadBuffer->Unmap(0, nullptr);

    D3D12_SUBRESOURCE_DATA subData = {};
    subData.pData = particleList;
    subData.RowPitch = bufferSize;
    subData.SlicePitch = bufferSize;

    mInitialCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mParticleBuffer1.Get(), D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_STATE_COPY_DEST));
    UpdateSubresources(mInitialCmdList, mParticleBuffer1.Get(), mUploadBuffer.Get(), 0, 0, 1, &subData);
    mInitialCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mParticleBuffer1.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));
}

void ParticleSystem::CreateDescriptors()
{
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Buffer.StructureByteStride = sizeof(Particle);
    srvDesc.Buffer.NumElements = maxParticles;

    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.StructureByteStride = sizeof(Particle);
    uavDesc.Buffer.NumElements = maxParticles;

    CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(mSrvHeap->GetCPUDescriptorHandleForHeapStart(), mDescriptorOffset, mDescriptorSize);

    mDevice->CreateShaderResourceView(mParticleBuffer1.Get(), &srvDesc, cpuHandle);
    mBuffer1SRV = CD3DX12_GPU_DESCRIPTOR_HANDLE(mSrvHeap->GetGPUDescriptorHandleForHeapStart(), mDescriptorOffset, mDescriptorSize);
    cpuHandle.Offset(1, mDescriptorSize);

    mDevice->CreateUnorderedAccessView(mParticleBuffer1.Get(), nullptr, &uavDesc, cpuHandle);
    mBuffer1UAV = CD3DX12_GPU_DESCRIPTOR_HANDLE(mBuffer1SRV, 1, mDescriptorSize);
    cpuHandle.Offset(1, mDescriptorSize);

    mDevice->CreateShaderResourceView(mParticleBuffer2.Get(), &srvDesc, cpuHandle);
    mBuffer2SRV = CD3DX12_GPU_DESCRIPTOR_HANDLE(mBuffer1UAV, 1, mDescriptorSize);
    cpuHandle.Offset(1, mDescriptorSize);

    mDevice->CreateUnorderedAccessView(mParticleBuffer2.Get(), nullptr, &uavDesc, cpuHandle);
    mBuffer2UAV = CD3DX12_GPU_DESCRIPTOR_HANDLE(mBuffer2SRV, 1, mDescriptorSize);

    mConsumeSRV = mBuffer1SRV;
    mAppendUAV = mBuffer2UAV;
}

void ParticleSystem::BuildShadersAndPSOs()
{
    ComPtr<ID3DBlob> computeShader;
    ComPtr<ID3DBlob> vertexShader;
    ComPtr<ID3DBlob> geomShader;
    ComPtr<ID3DBlob> pixelShader;
    ComPtr<ID3DBlob> errorBlob;

    // Компиляция compute шейдера
    HRESULT hr = D3DCompileFromFile(L"Shaders\\Particle\\ParticleUpdateCS.hlsl", nullptr, nullptr, "CSMain", "cs_5_0", 0, 0, &computeShader, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) {
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
        }
        ThrowIfFailed(hr);
    }

    // Компиляция рендер шейдеров из объединенного файла
    hr = D3DCompileFromFile(L"Shaders\\Particle\\ParticleRender.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", 0, 0, &vertexShader, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) {
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
        }
        ThrowIfFailed(hr);
    }

    hr = D3DCompileFromFile(L"Shaders\\Particle\\ParticleRender.hlsl", nullptr, nullptr, "GSMain", "gs_5_0", 0, 0, &geomShader, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) {
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
        }
        ThrowIfFailed(hr);
    }

    hr = D3DCompileFromFile(L"Shaders\\Particle\\ParticleRender.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", 0, 0, &pixelShader, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) {
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
        }
        ThrowIfFailed(hr);
    }

    // Compute Root Signature
    CD3DX12_ROOT_PARAMETER params[3];
    params[0].InitAsConstantBufferView(0); // Константы
    params[1].InitAsUnorderedAccessView(0); // ConsumeStructuredBuffer
    params[2].InitAsUnorderedAccessView(1); // AppendStructuredBuffer

    CD3DX12_ROOT_SIGNATURE_DESC computeDesc(3, params);
    ComPtr<ID3DBlob> sig;
    ThrowIfFailed(D3D12SerializeRootSignature(&computeDesc, D3D_ROOT_SIGNATURE_VERSION_1, &sig, nullptr));
    ThrowIfFailed(mDevice->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), IID_PPV_ARGS(&mComputeRootSig)));

    // Compute PSO
    D3D12_COMPUTE_PIPELINE_STATE_DESC computePso = {};
    computePso.pRootSignature = mComputeRootSig.Get();
    computePso.CS = { computeShader->GetBufferPointer(), computeShader->GetBufferSize() };
    ThrowIfFailed(mDevice->CreateComputePipelineState(&computePso, IID_PPV_ARGS(&mComputePSO)));

    // Render Root Signature
    CD3DX12_ROOT_PARAMETER renderParams[2];
    renderParams[0].InitAsConstantBufferView(0); // ViewProj
    renderParams[1].InitAsShaderResourceView(0); // StructuredBuffer

    CD3DX12_ROOT_SIGNATURE_DESC renderDesc(2, renderParams);
    ComPtr<ID3DBlob> renderSig;
    ThrowIfFailed(D3D12SerializeRootSignature(&renderDesc, D3D_ROOT_SIGNATURE_VERSION_1, &renderSig, nullptr));
    ThrowIfFailed(mDevice->CreateRootSignature(0, renderSig->GetBufferPointer(), renderSig->GetBufferSize(), IID_PPV_ARGS(&mRenderRootSig)));

    // Render PSO
    D3D12_GRAPHICS_PIPELINE_STATE_DESC renderPso = {};
    renderPso.pRootSignature = mRenderRootSig.Get();
    renderPso.VS = { vertexShader->GetBufferPointer(), vertexShader->GetBufferSize() };
    renderPso.GS = { geomShader->GetBufferPointer(), geomShader->GetBufferSize() };
    renderPso.PS = { pixelShader->GetBufferPointer(), pixelShader->GetBufferSize() };
    renderPso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
    renderPso.NumRenderTargets = 1;
    renderPso.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    //renderPso.BlendState.RenderTarget[0].BlendEnable = TRUE;
    //renderPso.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
    //renderPso.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
    //renderPso.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    renderPso.DepthStencilState.DepthEnable = FALSE;
    renderPso.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    renderPso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    renderPso.SampleDesc.Count = 1;
    renderPso.SampleMask = UINT_MAX;
    ThrowIfFailed(mDevice->CreateGraphicsPipelineState(&renderPso, IID_PPV_ARGS(&mRenderPSO)));
}

void ParticleSystem::SwapBuffers()
{
    if (mCurrentBuffer == mParticleBuffer1.Get()) {
        mCurrentBuffer = mParticleBuffer2.Get();
        mConsumeSRV = mBuffer2SRV;
        mAppendUAV = mBuffer1UAV;
    }
    else {
        mCurrentBuffer = mParticleBuffer1.Get();
        mConsumeSRV = mBuffer1SRV;
        mAppendUAV = mBuffer2UAV;
    }
}

D3D12_VERTEX_BUFFER_VIEW ParticleSystem::VertexBufferView() const
{
    D3D12_VERTEX_BUFFER_VIEW vbv;
    vbv.BufferLocation = mCurrentBuffer->GetGPUVirtualAddress();
    vbv.StrideInBytes = sizeof(Particle);
    vbv.SizeInBytes = maxParticles * sizeof(Particle);
    return vbv;
}