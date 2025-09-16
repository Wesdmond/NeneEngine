#include "GBuffer.h"
#include "Common/d3dUtil.h"

// G-Buffer textures (3 RT)
DXGI_FORMAT gbufferFormats[] = { DXGI_FORMAT_R8G8B8A8_UNORM,      // Albedo
                                 DXGI_FORMAT_R32G32B32A32_FLOAT,  // Normal
                                 DXGI_FORMAT_R32_FLOAT };         // Spec/Roughness

void GBuffer::Initialize(ID3D12Device* device, UINT width, UINT height,
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHeapStart,
    D3D12_CPU_DESCRIPTOR_HANDLE srvHeapStart, D3D12_GPU_DESCRIPTOR_HANDLE srvGpuHeapStart,
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle)
{
    m_width = width;
    m_height = height;
    m_rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    m_srvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    m_srvHeapStart = srvHeapStart;
    m_srvGpuHeapStart = srvGpuHeapStart;
    m_dsvHandle = dsvHandle;
    m_rtvHeapStart = rtvHeapStart;

    m_gbufferTextures.resize(3);
    m_rtvHandles.resize(3);
    m_gbufferSRVs.resize(3);

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvHeapStart);

    D3D12_CLEAR_VALUE optClear;
    optClear.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    optClear.DepthStencil.Depth = 1.0f;
    optClear.DepthStencil.Stencil = 0;
    for (int i = 0; i < 3; ++i) {
        optClear.Format = gbufferFormats[i];
        ThrowIfFailed(device->CreateCommittedResource(  
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Tex2D(gbufferFormats[i], width, height, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            nullptr,
            IID_PPV_ARGS(&m_gbufferTextures[i])));

        // RTV в m_gbufferRtvHeap
        device->CreateRenderTargetView(m_gbufferTextures[i].Get(), nullptr, rtvHandle);
        m_rtvHandles[i] = rtvHandle;
        rtvHandle.Offset(m_rtvDescriptorSize);

        // SRV (offset i от srvHeapStart, который уже после текстур)
        CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(srvHeapStart, i, m_srvDescriptorSize);
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = gbufferFormats[i];
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
        device->CreateShaderResourceView(m_gbufferTextures[i].Get(), &srvDesc, srvHandle);
        m_gbufferSRVs[i] = CD3DX12_GPU_DESCRIPTOR_HANDLE(srvGpuHeapStart, i, m_srvDescriptorSize);
    }
}

void GBuffer::BindForGeometryPass(ID3D12GraphicsCommandList* cmdList)
{
    // Clear RTs
    for (auto& rtv : m_rtvHandles) {
        cmdList->ClearRenderTargetView(rtv, DirectX::Colors::Black, 0, nullptr);  // Или подходящие цвета
    }
    cmdList->ClearDepthStencilView(m_dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    // OM Set Render Targets
    cmdList->OMSetRenderTargets(3, m_rtvHandles.data(), true, &m_dsvHandle);
}

void GBuffer::BindForLightingPass(ID3D12GraphicsCommandList* cmdList)
{
    // RT -> PixelShaderResource (barriers)
    for (auto& tex : m_gbufferTextures) {
        cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(tex.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
    }
}

void GBuffer::Unbind(ID3D12GraphicsCommandList* cmdList)
{
    // back to RT state
    for (auto& tex : m_gbufferTextures) {
        cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(tex.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET));
    }
}

void GBuffer::Resize(ID3D12Device* device, UINT width, UINT height)
{
    m_width = width;
    m_height = height;
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeapStart);

    for (int i = 0; i < 3; ++i) {
        ThrowIfFailed(device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Tex2D(gbufferFormats[i], width, height, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            nullptr,
            IID_PPV_ARGS(&m_gbufferTextures[i])));

        // RTV в m_gbufferRtvHeap
        device->CreateRenderTargetView(m_gbufferTextures[i].Get(), nullptr, rtvHandle);
        m_rtvHandles[i] = rtvHandle;
        rtvHandle.Offset(m_rtvDescriptorSize);

        // SRV (offset i от srvHeapStart, который уже после текстур)
        CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(m_srvHeapStart, i, m_srvDescriptorSize);
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = gbufferFormats[i];
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
        device->CreateShaderResourceView(m_gbufferTextures[i].Get(), &srvDesc, srvHandle);
        m_gbufferSRVs[i] = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_srvGpuHeapStart, i, m_srvDescriptorSize);
    }
}
