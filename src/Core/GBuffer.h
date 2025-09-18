#pragma once

#pragma once
#include "Common/d3dUtil.h"
#include <vector>


class GBuffer
{
public:
    void Initialize(ID3D12Device* device, UINT width, UINT height,
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHeapStart,
        D3D12_CPU_DESCRIPTOR_HANDLE srvHeapStart, D3D12_GPU_DESCRIPTOR_HANDLE srvGpuHeapStart,
        D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle);
    void BindForGeometryPass(ID3D12GraphicsCommandList* cmdList);
    void BindForLightingPass(ID3D12GraphicsCommandList* cmdList);
    void Unbind(ID3D12GraphicsCommandList* cmdList);
    void Resize(ID3D12Device* device, UINT width, UINT height);

    std::vector<D3D12_GPU_DESCRIPTOR_HANDLE> GetSRVs() const { return m_gbufferSRVs; }
    D3D12_CPU_DESCRIPTOR_HANDLE GetDepthSRV() const { return m_depthSRV; }

    UINT Width() const { return m_width; }
    UINT Height() const { return m_height; }

private:
    D3D12_CPU_DESCRIPTOR_HANDLE m_srvHeapStart;  // Начало SRV heap для G-Buffer
    D3D12_GPU_DESCRIPTOR_HANDLE m_srvGpuHeapStart;
    D3D12_CPU_DESCRIPTOR_HANDLE m_rtvHeapStart;

    // RTV for G-Buffer:
    // 0-Albedo (RGBA8UN)
    // 1-Normal (RGBA32F)
    // 3-Spec (R32F for roughness + GBA for FresnelR0)
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_gbufferTextures;
    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> m_rtvHandles;
    std::vector<D3D12_GPU_DESCRIPTOR_HANDLE> m_gbufferSRVs;  // SRV для lighting pass

    // Depth
    Microsoft::WRL::ComPtr<ID3D12Resource> m_depthBuffer;
    D3D12_CPU_DESCRIPTOR_HANDLE m_dsvHandle;
    D3D12_CPU_DESCRIPTOR_HANDLE m_depthSRV;

    UINT m_width = 0;
    UINT m_height = 0;
    UINT m_rtvDescriptorSize = 0;
    UINT m_srvDescriptorSize = 0;
};

