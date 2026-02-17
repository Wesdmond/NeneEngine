#include "HistoryBuffer.h"

void HistoryBuffer::Initialize(ID3D12Device* device, UINT width, UINT height,
    D3D12_CPU_DESCRIPTOR_HANDLE* rtvHandles,
    D3D12_CPU_DESCRIPTOR_HANDLE* srvHandles)
{
    CreateRenderTarget(device, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, HistoryA, rtvHandles[0], srvHandles[0]);
    HistoryARTV = rtvHandles[0];
    HistoryASRV = srvHandles[0];
    CreateRenderTarget(device, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, HistoryB, rtvHandles[1], srvHandles[1]);
    HistoryBRTV = rtvHandles[1];
    HistoryBSRV = srvHandles[1];
    CreateRenderTarget(device, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, Current, rtvHandles[2], srvHandles[2]);
    CurrentRTV = rtvHandles[2];
    CurrentSRV = srvHandles[2];
    CreateRenderTarget(device, width, height, DXGI_FORMAT_R32G32_FLOAT, Velocity, rtvHandles[3], srvHandles[3]);
    VelocityRTV = rtvHandles[3];
    VelocitySRV = srvHandles[3];
}

void HistoryBuffer::CreateRenderTarget(ID3D12Device* device, UINT width, UINT height,
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

void HistoryBuffer::TransitionToRenderTarget(ID3D12GraphicsCommandList* cmdList)
{
    D3D12_RESOURCE_BARRIER barriers[] = {
        CD3DX12_RESOURCE_BARRIER::Transition(HistoryA.Get(),    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET),
        CD3DX12_RESOURCE_BARRIER::Transition(HistoryB.Get(),    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET),
        CD3DX12_RESOURCE_BARRIER::Transition(Current.Get(),    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET),
        CD3DX12_RESOURCE_BARRIER::Transition(Velocity.Get(),  D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET),
    };

    cmdList->ResourceBarrier(_countof(barriers), barriers);
}
void HistoryBuffer::TransitionToShaderResource(ID3D12GraphicsCommandList* cmdList)
{
    D3D12_RESOURCE_BARRIER barriers[] = {
        CD3DX12_RESOURCE_BARRIER::Transition(HistoryA.Get(),    D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
        CD3DX12_RESOURCE_BARRIER::Transition(HistoryB.Get(),    D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
        CD3DX12_RESOURCE_BARRIER::Transition(Current.Get(),    D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
        CD3DX12_RESOURCE_BARRIER::Transition(Velocity.Get(),  D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
    };

    cmdList->ResourceBarrier(_countof(barriers), barriers);
}
void HistoryBuffer::SetRenderTargets(ID3D12GraphicsCommandList* cmdList, D3D12_CPU_DESCRIPTOR_HANDLE dsv)
{
    D3D12_CPU_DESCRIPTOR_HANDLE rtvs[] = {
        HistoryARTV,
        HistoryBRTV,
        CurrentRTV,
        VelocityRTV,
    };

    cmdList->OMSetRenderTargets(_countof(rtvs), rtvs, false, &dsv);
}
std::array<D3D12_CPU_DESCRIPTOR_HANDLE, HistoryBuffer::NumTextures> HistoryBuffer::GetRTVHandles() const
{
    return { HistoryARTV, HistoryBRTV, CurrentRTV, VelocityRTV };
}

void HistoryBuffer::ClearRenderTargets(ID3D12GraphicsCommandList* cmdList, const float clearColor[4])
{
    cmdList->ClearRenderTargetView(HistoryARTV, clearColor, 0, nullptr);
    cmdList->ClearRenderTargetView(HistoryBRTV, clearColor, 0, nullptr);
    cmdList->ClearRenderTargetView(CurrentRTV, clearColor, 0, nullptr);
    cmdList->ClearRenderTargetView(VelocityRTV, clearColor, 0, nullptr);
}

D3D12_GPU_DESCRIPTOR_HANDLE HistoryBuffer::GetSRVTable(ID3D12DescriptorHeap* heap, UINT descriptorSize) const
{
    return CD3DX12_GPU_DESCRIPTOR_HANDLE(
        heap->GetGPUDescriptorHandleForHeapStart(),
        SrvHeapStartIndex,
        descriptorSize
    );
}