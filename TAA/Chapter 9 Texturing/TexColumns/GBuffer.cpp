#include "GBuffer.h"

void GBuffer::Initialize(ID3D12Device* device, UINT width, UINT height,
    D3D12_CPU_DESCRIPTOR_HANDLE* rtvHandles,
    D3D12_CPU_DESCRIPTOR_HANDLE* srvHandles)
{
    CreateRenderTarget(device, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, Albedo, rtvHandles[0], srvHandles[0]);
    AlbedoRTV = rtvHandles[0];
    AlbedoSRV = srvHandles[0];
    CreateRenderTarget(device, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, Normal, rtvHandles[1], srvHandles[1]);
    NormalRTV = rtvHandles[1];
    NormalSRV = srvHandles[1];
    CreateRenderTarget(device, width, height, DXGI_FORMAT_R32G32B32A32_FLOAT, WorldPos, rtvHandles[2], srvHandles[2]);
    WorldPosRTV = rtvHandles[2];
    WorldPosSRV = srvHandles[2];
    CreateRenderTarget(device, width, height, DXGI_FORMAT_R8_UNORM, Roughness, rtvHandles[3], srvHandles[3]);
    RoughnessRTV = rtvHandles[3];
    RoughnessSRV = srvHandles[3];
}

void GBuffer::CreateRenderTarget(ID3D12Device* device, UINT width, UINT height,
    DXGI_FORMAT format,
    ComPtr<ID3D12Resource>& outResource,
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle,
    D3D12_CPU_DESCRIPTOR_HANDLE srvHandle)
{
    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = 1;
    texDesc.Format = format;
    texDesc.SampleDesc.Count = 1;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = format;
    clearValue.Color[0] = 0.0f;
    clearValue.Color[1] = 0.0f;
    clearValue.Color[2] = 0.0f;
    clearValue.Color[3] = 0.0f;

    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);

    device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &texDesc,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        &clearValue,
        IID_PPV_ARGS(&outResource));

    // RTV
    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.Format = format;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    rtvDesc.Texture2D.MipSlice = 0;

    device->CreateRenderTargetView(outResource.Get(), &rtvDesc, rtvHandle);

    // SRV
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels = 1;

    device->CreateShaderResourceView(outResource.Get(), &srvDesc, srvHandle);
}

void GBuffer::TransitionToRenderTarget(ID3D12GraphicsCommandList* cmdList)
{
    D3D12_RESOURCE_BARRIER barriers[] = {
        CD3DX12_RESOURCE_BARRIER::Transition(Albedo.Get(),    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET),
        CD3DX12_RESOURCE_BARRIER::Transition(Normal.Get(),    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET),
        CD3DX12_RESOURCE_BARRIER::Transition(WorldPos.Get(),  D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET),
        CD3DX12_RESOURCE_BARRIER::Transition(Roughness.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET)
    };

    cmdList->ResourceBarrier(_countof(barriers), barriers);
}
void GBuffer::TransitionToShaderResource(ID3D12GraphicsCommandList* cmdList)
{
    D3D12_RESOURCE_BARRIER barriers[] = {
        CD3DX12_RESOURCE_BARRIER::Transition(Albedo.Get(),    D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
        CD3DX12_RESOURCE_BARRIER::Transition(Normal.Get(),    D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
        CD3DX12_RESOURCE_BARRIER::Transition(WorldPos.Get(),  D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
        CD3DX12_RESOURCE_BARRIER::Transition(Roughness.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
    };

    cmdList->ResourceBarrier(_countof(barriers), barriers);
}
void GBuffer::SetRenderTargets(ID3D12GraphicsCommandList* cmdList, D3D12_CPU_DESCRIPTOR_HANDLE dsv)
{
    D3D12_CPU_DESCRIPTOR_HANDLE rtvs[] = {
        AlbedoRTV,
        NormalRTV,
        WorldPosRTV,
        RoughnessRTV,
    };

    cmdList->OMSetRenderTargets(_countof(rtvs), rtvs, false, &dsv);
}
std::array<D3D12_CPU_DESCRIPTOR_HANDLE, GBuffer::NumTextures> GBuffer::GetRTVHandles() const
{
    return { AlbedoRTV, NormalRTV, WorldPosRTV, RoughnessRTV };
}

void GBuffer::ClearRenderTargets(ID3D12GraphicsCommandList* cmdList, const float clearColor[4])
{
    cmdList->ClearRenderTargetView(AlbedoRTV, clearColor, 0, nullptr);
    cmdList->ClearRenderTargetView(NormalRTV, clearColor, 0, nullptr);
    cmdList->ClearRenderTargetView(WorldPosRTV, clearColor, 0, nullptr);
    cmdList->ClearRenderTargetView(RoughnessRTV, clearColor, 0, nullptr);
}

D3D12_GPU_DESCRIPTOR_HANDLE GBuffer::GetSRVTable(ID3D12DescriptorHeap* heap, UINT descriptorSize) const
{
    return CD3DX12_GPU_DESCRIPTOR_HANDLE(
        heap->GetGPUDescriptorHandleForHeapStart(),
        SrvHeapStartIndex,
        descriptorSize
    );
}
