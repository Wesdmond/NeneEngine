// ParticleSystem.cpp
#include "ParticleSystem.h"
#include "Common/d3dUtil.h" // Assume you have this for CompileShader, etc.

ParticleSystem::ParticleSystem(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, ID3D12DescriptorHeap* srvHeap, UINT descriptorOffset, UINT maxParticles, XMFLOAT3 origin)
    : md3dDevice(device), mCommandList(cmdList), mSrvDescriptorHeap(srvHeap), mDescriptorOffset(descriptorOffset), maxParticles(maxParticles), mOrigin(origin), mParticleCount(maxParticles)
{
    BuildResources(cmdList);
    BuildDescriptors();
    BuildRootSignatures();
    BuildShadersAndPSOs();

    // Load sprite texture if needed, assuming you have a function like LoadTexture
    // mSpriteTexture = ...; // Integrate with your texture loading
}

void ParticleSystem::BuildResources(ID3D12GraphicsCommandList* cmdList)
{
    const UINT stride = sizeof(Particle);
    const UINT64 bytes = UINT64(mParticleCount) * stride;

    d3dUtil::CreateDefaultBuffer(md3dDevice, cmdList, nullptr, bytes, mParticlesA); // Adjust as needed
    d3dUtil::CreateDefaultBuffer(md3dDevice, cmdList, nullptr, bytes, mParticlesB);

    mParticlesInit = std::make_unique<UploadBuffer<Particle>>(md3dDevice, mParticleCount, false);
    for (UINT i = 0; i < mParticleCount; i++)
    {
        Particle p{};
        p.pos = XMFLOAT3(MathHelper::RandF(-50, 50), MathHelper::RandF(30, 40), MathHelper::RandF(-50, 50));
        p.vel = XMFLOAT3(MathHelper::RandF(0.5f, 0.5f), MathHelper::RandF(5.5f, 5.5f), MathHelper::RandF(0.5f, 0.5f));
        p.life = p.lifetime = MathHelper::RandF(2.0f, 5.0f);
        p.size = MathHelper::RandF(0.15f, 0.55f);
        p.rot = 0.0f;
        p.alive = 1;
        p.color = XMFLOAT4(1, 1, 1, 1);
        mParticlesInit->CopyData(i, p);
    }

    // Copy to A
    auto toCopyDest = CD3DX12_RESOURCE_BARRIER::Transition(mParticlesA.Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COPY_DEST);
    cmdList->ResourceBarrier(1, &toCopyDest);
    cmdList->CopyResource(mParticlesA.Get(), mParticlesInit->Resource());
    auto toSRV = CD3DX12_RESOURCE_BARRIER::Transition(mParticlesA.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    cmdList->ResourceBarrier(1, &toSRV);

    mSimCB = std::make_unique<UploadBuffer<SimCB>>(md3dDevice, 1, true);
}

void ParticleSystem::BuildDescriptors()
{
    D3D12_DESCRIPTOR_HEAP_DESC desc{};
    desc.NumDescriptors = 5;
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    md3dDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&mParticleHeap));

    const UINT inc = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    auto cpu = mParticleHeap->GetCPUDescriptorHandleForHeapStart();

    D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
    srv.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv.Buffer.FirstElement = 0;
    srv.Buffer.NumElements = mParticleCount;
    srv.Buffer.StructureByteStride = sizeof(Particle);
    srv.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

    D3D12_UNORDERED_ACCESS_VIEW_DESC uav{};
    uav.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    uav.Buffer.FirstElement = 0;
    uav.Buffer.NumElements = mParticleCount;
    uav.Buffer.StructureByteStride = sizeof(Particle);
    uav.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

    md3dDevice->CreateShaderResourceView(mParticlesA.Get(), &srv, cpu); cpu.ptr += inc;
    md3dDevice->CreateUnorderedAccessView(mParticlesA.Get(), nullptr, &uav, cpu); cpu.ptr += inc;
    md3dDevice->CreateShaderResourceView(mParticlesB.Get(), &srv, cpu); cpu.ptr += inc;
    md3dDevice->CreateUnorderedAccessView(mParticlesB.Get(), nullptr, &uav, cpu); cpu.ptr += inc;

    // Sprite SRV - assume checkerboard or your texture
    D3D12_SHADER_RESOURCE_VIEW_DESC spriteSrv = {}; // Fill with your texture desc
    md3dDevice->CreateShaderResourceView(mSpriteTexture.Get(), &spriteSrv, cpu);
}

void ParticleSystem::BuildRootSignatures()
{
    // CS Root Sig
    CD3DX12_DESCRIPTOR_RANGE t0(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    CD3DX12_DESCRIPTOR_RANGE u0(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
    CD3DX12_ROOT_PARAMETER pCS[3];
    pCS[0].InitAsConstantBufferView(0);
    pCS[1].InitAsDescriptorTable(1, &t0);
    pCS[2].InitAsDescriptorTable(1, &u0);
    CD3DX12_ROOT_SIGNATURE_DESC rsCS(3, pCS, 0, nullptr);
    ComPtr<ID3DBlob> sigCS;
    D3D12SerializeRootSignature(&rsCS, D3D_ROOT_SIGNATURE_VERSION_1, &sigCS, nullptr);
    md3dDevice->CreateRootSignature(0, sigCS->GetBufferPointer(), sigCS->GetBufferSize(), IID_PPV_ARGS(&mParticleCS_RS));

    // Gfx Root Sig
    CD3DX12_DESCRIPTOR_RANGE t0Gfx(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    CD3DX12_DESCRIPTOR_RANGE t1Gfx(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
    CD3DX12_ROOT_PARAMETER pGfx[3];
    pGfx[0].InitAsConstantBufferView(0);
    pGfx[1].InitAsDescriptorTable(1, &t0Gfx, D3D12_SHADER_VISIBILITY_PIXEL);
    pGfx[2].InitAsDescriptorTable(1, &t1Gfx, D3D12_SHADER_VISIBILITY_PIXEL);
    auto samplers = GetStaticSamplers(); // From your app
    CD3DX12_ROOT_SIGNATURE_DESC rsGfx(3, pGfx, (UINT)samplers.size(), samplers.data(), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
    ComPtr<ID3DBlob> sigGfx;
    D3D12SerializeRootSignature(&rsGfx, D3D_ROOT_SIGNATURE_VERSION_1, &sigGfx, nullptr);
    md3dDevice->CreateRootSignature(0, sigGfx->GetBufferPointer(), sigGfx->GetBufferSize(), IID_PPV_ARGS(&mParticleGfx_RS));
}

void ParticleSystem::BuildShadersAndPSOs()
{
    mShaders["particleCS"] = d3dUtil::CompileShader(L"Shaders\\ParticlesCS.hlsl", nullptr, "CS", "cs_5.1");
    mShaders["particleVS"] = d3dUtil::CompileShader(L"Shaders\\ParticlesBillboard.hlsl", nullptr, "VS", "vs_5.1");
    mShaders["particlePS"] = d3dUtil::CompileShader(L"Shaders\\ParticlesBillboard.hlsl", nullptr, "PS", "ps_5.1");

    D3D12_COMPUTE_PIPELINE_STATE_DESC csPso = {};
    csPso.pRootSignature = mParticleCS_RS.Get();
    csPso.CS = {
        reinterpret_cast<BYTE*>(mShaders["particleCS"]->GetBufferPointer()),
        mShaders["particleCS"]->GetBufferSize()
    };
    md3dDevice->CreateComputePipelineState(&csPso, IID_PPV_ARGS(&mParticleCS_PSO));

    D3D12_GRAPHICS_PIPELINE_STATE_DESC gfxPso = {};
    gfxPso.pRootSignature = mParticleGfx_RS.Get();
    gfxPso.VS = {
        reinterpret_cast<BYTE*>(mShaders["particleVS"]->GetBufferPointer()),
        mShaders["particleVS"]->GetBufferSize()
    };
    gfxPso.PS = {
        reinterpret_cast<BYTE*>(mShaders["particlePS"]->GetBufferPointer()),
        mShaders["particlePS"]->GetBufferSize()
    };
    gfxPso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    gfxPso.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    gfxPso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    gfxPso.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    gfxPso.BlendState.RenderTarget[0].BlendEnable = TRUE;
    gfxPso.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
    gfxPso.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
    gfxPso.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    gfxPso.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    gfxPso.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
    gfxPso.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    gfxPso.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    gfxPso.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    gfxPso.SampleMask = UINT_MAX;
    gfxPso.SampleDesc.Count = 1;
    gfxPso.NumRenderTargets = 1;
    gfxPso.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM; // Adjust to your format
    gfxPso.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    md3dDevice->CreateGraphicsPipelineState(&gfxPso, IID_PPV_ARGS(&mParticleGfx_PSO));
}

void ParticleSystem::Update(const GameTimer& gt)
{
    Simulate(gt);
}

void ParticleSystem::Simulate(const GameTimer& gt)
{
    bool useA = true; // Simplify or use frame index as in original

    ID3D12DescriptorHeap* heaps[] = { mParticleHeap.Get() };
    mCommandList->SetDescriptorHeaps(1, heaps);

    mCommandList->SetComputeRootSignature(mParticleCS_RS.Get());
    mCommandList->SetPipelineState(mParticleCS_PSO.Get());

    SimCB scb{ gt.DeltaTime(), force };
    mSimCB->CopyData(0, scb);
    mCommandList->SetComputeRootConstantBufferView(0, mSimCB->Resource()->GetGPUVirtualAddress());

    auto outRes = useA ? mParticlesB.Get() : mParticlesA.Get();
    auto barrierToUAV = CD3DX12_RESOURCE_BARRIER::Transition(outRes, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    mCommandList->ResourceBarrier(1, &barrierToUAV);

    auto gpuHandle = mParticleHeap->GetGPUDescriptorHandleForHeapStart();
    UINT inc = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    mCommandList->SetComputeRootDescriptorTable(1, CD3DX12_GPU_DESCRIPTOR_HANDLE(gpuHandle, useA ? KSRV_A : KSRV_B));
    mCommandList->SetComputeRootDescriptorTable(2, CD3DX12_GPU_DESCRIPTOR_HANDLE(gpuHandle, useA ? KUAV_B : KUAV_A));

    UINT groups = (mParticleCount + 255) / 256;
    mCommandList->Dispatch(groups, 1, 1);

    auto uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(outRes);
    mCommandList->ResourceBarrier(1, &uavBarrier);

    auto barrierToSRV = CD3DX12_RESOURCE_BARRIER::Transition(outRes, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    mCommandList->ResourceBarrier(1, &barrierToSRV);
}

void ParticleSystem::Draw(ID3D12GraphicsCommandList* cmdList, D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle, D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle, const PassConstants& passCB)
{
    mCommandList = cmdList; // Update if needed

    bool useA = true;
    ID3D12DescriptorHeap* heaps[] = { mParticleHeap.Get() };
    mCommandList->SetDescriptorHeaps(1, heaps);

    mCommandList->SetGraphicsRootSignature(mParticleGfx_RS.Get());
    mCommandList->SetPipelineState(mParticleGfx_PSO.Get());

    mCommandList->IASetVertexBuffers(0, 0, nullptr);
    mCommandList->IASetIndexBuffer(nullptr);
    mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    // Set Pass CB if needed, assume you pass it or have it

    auto gpuHandle = mParticleHeap->GetGPUDescriptorHandleForHeapStart();
    UINT inc = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    mCommandList->SetGraphicsRootDescriptorTable(1, CD3DX12_GPU_DESCRIPTOR_HANDLE(gpuHandle, useA ? KSRV_B : KSRV_A));
    mCommandList->SetGraphicsRootDescriptorTable(2, CD3DX12_GPU_DESCRIPTOR_HANDLE(gpuHandle, KSRV_Sprite));

    mCommandList->OMSetRenderTargets(1, &rtvHandle, false, &dsvHandle);
    mCommandList->DrawInstanced(4, mParticleCount, 0, 0);
}