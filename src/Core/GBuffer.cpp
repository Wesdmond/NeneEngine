#include "GBuffer.h"
#include "Common/d3dUtil.h"
#include <iostream>

// G-Buffer textures (3 RT)
DXGI_FORMAT gbufferFormats[] = { DXGI_FORMAT_R8G8B8A8_UNORM,      // Albedo
                                 DXGI_FORMAT_R32G32B32A32_FLOAT,  // Normal
                                 DXGI_FORMAT_R32_FLOAT };         // Spec/Roughness
DXGI_FORMAT depthFormat = DXGI_FORMAT_D32_FLOAT;                  // Depth (DSV)
DXGI_FORMAT depthSrvFormat = DXGI_FORMAT_R32_FLOAT;               // Depth (SRV)

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

    Resize(device, width, height);
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
    cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
        m_depthBuffer.Get(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
}

void GBuffer::Unbind(ID3D12GraphicsCommandList* cmdList)
{
    // back to RT state
    for (auto& tex : m_gbufferTextures) {
        cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(tex.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET));
    }
    cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
        m_depthBuffer.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE));
}

void GBuffer::Resize(ID3D12Device* device, UINT width, UINT height)
{
    std::cout << "Strating resing GBuffer" << std::endl;
    m_width = width;
    m_height = height;
    m_gbufferTextures.resize(3);
    m_rtvHandles.resize(3);
    m_gbufferSRVs.resize(4);

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeapStart);
    CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(m_srvHeapStart);

    // Create G-Buffer textures and RTVs/SRVs
    D3D12_CLEAR_VALUE optClear;
    for (int i = 0; i < 3; ++i)
    {
        optClear.Format = gbufferFormats[i];
        if (i == 0) { // Albedo: Black
            optClear.Color[0] = optClear.Color[1] = optClear.Color[2] = 0.0f;
            optClear.Color[3] = 1.0f;
        }
        else if (i == 1) { // Normal: (0,0,1,0)
            optClear.Color[0] = optClear.Color[1] = 0.0f;
            optClear.Color[2] = 1.0f;
            optClear.Color[3] = 0.0f;
        }
        else { // Roughness: 0
            optClear.Color[0] = 0.0f;
            optClear.Color[1] = optClear.Color[2] = optClear.Color[3] = 0.0f;
        }

        // Create texture
        ThrowIfFailed(device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Tex2D(gbufferFormats[i], width, height, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            &optClear,
            IID_PPV_ARGS(&m_gbufferTextures[i])));

        // Create RTV
        device->CreateRenderTargetView(m_gbufferTextures[i].Get(), nullptr, rtvHandle);
        m_rtvHandles[i] = rtvHandle;
        rtvHandle.Offset(m_rtvDescriptorSize);

        // Create SRV (offset i from srvHeapStart)
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = gbufferFormats[i];
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
        device->CreateShaderResourceView(m_gbufferTextures[i].Get(), &srvDesc, srvHandle);
        m_gbufferSRVs[i] = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_srvGpuHeapStart, i, m_srvDescriptorSize);
        srvHandle.Offset(m_srvDescriptorSize);
    }
    // Create depth buffer
    optClear.Format = depthFormat;
    optClear.DepthStencil.Depth = 1.0f;
    optClear.DepthStencil.Stencil = 0;
    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Tex2D(depthFormat, width, height, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL),
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &optClear,
        IID_PPV_ARGS(&m_depthBuffer)));

    // Create DSV
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = depthFormat;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Texture2D.MipSlice = 0;
    device->CreateDepthStencilView(m_depthBuffer.Get(), &dsvDesc, m_dsvHandle);

    // Create SRV for depth (offset 3 from srvHeapStart)
    D3D12_SHADER_RESOURCE_VIEW_DESC depthSrvDesc = {};
    depthSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    depthSrvDesc.Format = depthSrvFormat;
    depthSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    depthSrvDesc.Texture2D.MipLevels = 1;
    depthSrvDesc.Texture2D.MostDetailedMip = 0;
    depthSrvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
    device->CreateShaderResourceView(m_depthBuffer.Get(), &depthSrvDesc, srvHandle);
    m_gbufferSRVs[3] = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_srvGpuHeapStart, 3, m_srvDescriptorSize);
}
