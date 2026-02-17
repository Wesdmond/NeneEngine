#pragma once

#include <d3d12.h>
#include "d3dx12.h"
#include <wrl.h>
#include <array>
using Microsoft::WRL::ComPtr;

class GBuffer
{
public:
    static constexpr int NumTextures = 4; // �������, �������, �������, roughness

    // G-Buffer ��������
    ComPtr<ID3D12Resource> Albedo = nullptr;
    ComPtr<ID3D12Resource> Normal = nullptr;
    ComPtr<ID3D12Resource> WorldPos = nullptr;
    ComPtr<ID3D12Resource> Roughness = nullptr;

    // RTV �����������
    D3D12_CPU_DESCRIPTOR_HANDLE AlbedoRTV;
    D3D12_CPU_DESCRIPTOR_HANDLE NormalRTV;
    D3D12_CPU_DESCRIPTOR_HANDLE WorldPosRTV;
    D3D12_CPU_DESCRIPTOR_HANDLE RoughnessRTV;

    // SRV �����������
    D3D12_CPU_DESCRIPTOR_HANDLE AlbedoSRV;
    D3D12_CPU_DESCRIPTOR_HANDLE NormalSRV;
    D3D12_CPU_DESCRIPTOR_HANDLE WorldPosSRV;
    D3D12_CPU_DESCRIPTOR_HANDLE RoughnessSRV;

    UINT SrvHeapStartIndex = 0;

    // ������������� ���� ��������
    void Initialize(ID3D12Device* device, UINT width, UINT height,
        D3D12_CPU_DESCRIPTOR_HANDLE* rtvHandles,
        D3D12_CPU_DESCRIPTOR_HANDLE* srvHandles);
    void TransitionToRenderTarget(ID3D12GraphicsCommandList* cmdList);
    void TransitionToShaderResource(ID3D12GraphicsCommandList* cmdList);
    void SetRenderTargets(ID3D12GraphicsCommandList* cmdList, D3D12_CPU_DESCRIPTOR_HANDLE dsv);
    void ClearRenderTargets(ID3D12GraphicsCommandList* cmdList, const float clearColor[4]);


    std::array<D3D12_CPU_DESCRIPTOR_HANDLE, GBuffer::NumTextures> GetRTVHandles() const;
    D3D12_GPU_DESCRIPTOR_HANDLE GetSRVTable(ID3D12DescriptorHeap* heap, UINT descriptorSize) const;

private:
    void CreateRenderTarget(ID3D12Device* device, UINT width, UINT height,
        DXGI_FORMAT format,
        ComPtr<ID3D12Resource>& outResource,
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle,
        D3D12_CPU_DESCRIPTOR_HANDLE srvHandle);
};

