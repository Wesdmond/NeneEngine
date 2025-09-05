#pragma once
#include "Renderer.h"
#include <d3d12.h>
#include <wrl.h>
#include <dxgi1_6.h>
using Microsoft::WRL::ComPtr;

class DirectX12Renderer : public Renderer
{
	DECLARE_SINGLETON(DirectX12Renderer)
public:
	void Initialize() override;
	void Render() override;
	void Shutdown() override;

private:
    static const UINT FrameCount = 2;

#pragma region Pipeline Objects
    D3D12_VIEWPORT m_viewport;
    D3D12_RECT m_scissorRect;
    ComPtr<IDXGISwapChain4> m_swapChain;
    ComPtr<ID3D12Device> m_device;
    ComPtr<ID3D12Resource> m_renderTargets[FrameCount];
    ComPtr<ID3D12CommandAllocator> m_commandAllocator;
    ComPtr<ID3D12CommandQueue> m_commandQueue;
    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    ComPtr<ID3D12PipelineState> m_pipelineState;
    ComPtr<ID3D12GraphicsCommandList> m_commandList;
    UINT m_rtvDescriptorSize;
#pragma endregion

#pragma region App resources
    ComPtr<ID3D12Resource> m_vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
#pragma endregion

#pragma region Synchronization objects.
    UINT m_frameIndex;
    HANDLE m_fenceEvent;
    ComPtr<ID3D12Fence> m_fence;
    UINT64 m_fenceValue;
#pragma endregion

    void LoadPipeline();
    void LoadAssets();
    void PopulateCommandList();
    void WaitForPreviousFrame();
};

