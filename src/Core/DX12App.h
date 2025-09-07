#pragma once
#include "Common/d3dUtil.h"
#include "Inputs/InputDevice.h"

#if defined(DEBUG) || defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

// Link necessary d3d12 libraries.
#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "dxgi.lib")

class GameTimer;

class DX12App
{
public:
    DX12App(HINSTANCE mhAppInst, HWND mhMainWnd);
    virtual ~DX12App();

    virtual bool Initialize();
    virtual void Update(const GameTimer& gt) = 0;
    virtual void Draw(const GameTimer& gt) = 0;
    virtual void OnResize();


    float AspectRatio() const;

    bool Get4xMsaaState() const;
    void Set4xMsaaState(bool value);
    bool CheckIsDeviceInit() const;
    void SetWindowSize(int width, int height);

protected:
    bool LoadPipeline();
    virtual void CreateRtvAndDsvDescriptorHeaps();
    void CreateCommandObjects();
    void CreateSwapChain();

    void FlushCommandQueue();

    ID3D12Resource* CurrentBackBuffer() const;
    D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView() const;
    D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView() const;

    void LogAdapters();
    void LogAdapterOutputs(IDXGIAdapter* adapter);
    void LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format);

protected:
    HINSTANCE m_hAppInst = nullptr; // application instance handle
    HWND m_hMainWnd = nullptr; // main window handle

    // Set true to use 4X MSAA (4.1.8).  The default is false.
    bool m4xMsaaState = false; // 4X MSAA enabled
    UINT m4xMsaaQuality = 0; // quality level of 4X MSAA

    //  Pipeline Objects
    static constexpr int SwapChainBufferCount = 2;
    static constexpr UINT FrameCount = 2;
    D3D12_VIEWPORT m_viewport;
    D3D12_RECT m_scissorRect;
    Microsoft::WRL::ComPtr<IDXGISwapChain> m_swapChain;
    Microsoft::WRL::ComPtr<IDXGIFactory4> m_dxgiFactory;
    Microsoft::WRL::ComPtr<ID3D12Device> m_device;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_renderTargets[FrameCount];
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_commandAllocator;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_commandQueue;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_commandList;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState;
    int m_currBackBuffer = 0;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_depthStencilBuffer;
    UINT m_rtvDescriptorSize = 0;
    UINT m_dsvDescriptorSize = 0;
    UINT m_cbvSrvUavDescriptorSize = 0;

    //Synchronization objects.
    UINT m_frameIndex;
    HANDLE m_fenceEvent;
    Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;
    UINT64 m_fenceValue;

    // Derived class should set these in derived constructor to customize starting values.
    std::wstring m_MainWndCaption = L"Nene App";
    D3D_DRIVER_TYPE m_d3dDriverType = D3D_DRIVER_TYPE_HARDWARE;
    DXGI_FORMAT m_backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    DXGI_FORMAT m_depthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    int m_clientWidth = 1920;
    int m_clientHeight = 1080;

    std::shared_ptr<InputDevice> m_inputDevice;
};