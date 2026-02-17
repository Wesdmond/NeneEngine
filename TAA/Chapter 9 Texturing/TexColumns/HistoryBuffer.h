#pragma once

#include <d3d12.h>
#include "d3dx12.h"
#include <wrl.h>
#include <array>
using Microsoft::WRL::ComPtr;

class HistoryBuffer
{
public:
    static constexpr int NumTextures = 4; // ������� ����, ������� ����, ��������

    // G-Buffer ��������
    ComPtr<ID3D12Resource> HistoryA = nullptr;
    ComPtr<ID3D12Resource> HistoryB = nullptr;
    ComPtr<ID3D12Resource> Current = nullptr;
    ComPtr<ID3D12Resource> Velocity = nullptr;

    // RTV �����������
    D3D12_CPU_DESCRIPTOR_HANDLE HistoryARTV;
    D3D12_CPU_DESCRIPTOR_HANDLE HistoryBRTV;
    D3D12_CPU_DESCRIPTOR_HANDLE CurrentRTV;
    D3D12_CPU_DESCRIPTOR_HANDLE VelocityRTV;

    // SRV �����������
    D3D12_CPU_DESCRIPTOR_HANDLE HistoryASRV;
    D3D12_CPU_DESCRIPTOR_HANDLE HistoryBSRV;
    D3D12_CPU_DESCRIPTOR_HANDLE CurrentSRV;
    D3D12_CPU_DESCRIPTOR_HANDLE VelocitySRV;

    UINT SrvHeapStartIndex = 0;
    bool HistoryARead = false;

    // ������������� ���� ��������
    void Initialize(ID3D12Device* device, UINT width, UINT height,
        D3D12_CPU_DESCRIPTOR_HANDLE* rtvHandles,
        D3D12_CPU_DESCRIPTOR_HANDLE* srvHandles);
    void TransitionToRenderTarget(ID3D12GraphicsCommandList* cmdList);
    void TransitionToShaderResource(ID3D12GraphicsCommandList* cmdList);
    void SetRenderTargets(ID3D12GraphicsCommandList* cmdList, D3D12_CPU_DESCRIPTOR_HANDLE dsv);
    void ClearRenderTargets(ID3D12GraphicsCommandList* cmdList, const float clearColor[4]);


    std::array<D3D12_CPU_DESCRIPTOR_HANDLE, HistoryBuffer::NumTextures> GetRTVHandles() const;
    D3D12_GPU_DESCRIPTOR_HANDLE GetSRVTable(ID3D12DescriptorHeap* heap, UINT descriptorSize) const;

private:
    void CreateRenderTarget(ID3D12Device* device, UINT width, UINT height,
        DXGI_FORMAT format,
        ComPtr<ID3D12Resource>& outResource,
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle,
        D3D12_CPU_DESCRIPTOR_HANDLE srvHandle);
};

