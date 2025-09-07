#include "NeneApp.h"

using Microsoft::WRL::ComPtr;
using namespace std;
using namespace DirectX;

struct Vertex
{
    DirectX::XMFLOAT3 Pos;
    DirectX::XMFLOAT4 Color;

    Vertex(XMFLOAT3 _Pos, XMFLOAT4 _Color) : Pos(_Pos), Color(_Color) {}
};

NeneApp::NeneApp(HINSTANCE mhAppInst, HWND mhMainWnd) : DX12App(mhAppInst, mhMainWnd) {}

bool NeneApp::Initialize()
{
    if (!DX12App::Initialize())
        return false;

    // Reset the command list to prep for initialization commands.
    ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), nullptr));

    BuildDescriptorHeaps();
    BuildConstantBuffers();
    BuildRootSignature();
    BuildShadersAndInputLayout();
    BuildGeometry();
    BuildPSO();

    // Execute the initialization commands.
    ThrowIfFailed(m_commandList->Close());
    ID3D12CommandList* cmdsLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
    
    return true;
}

void NeneApp::Update(const GameTimer& gt)
{
    
}

void NeneApp::Draw(const GameTimer& gt)
{
    PopulateCommandList();
    
    // Add the command list to the queue for execution.
    ID3D12CommandList* cmdLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

    // Swap the back and front buffers
    ThrowIfFailed(m_swapChain->Present(0, 0)); // TODO: add DXGI_PRESENT_ALLOW_TEARING flag if want to disable fps lock.

    m_currBackBuffer = (m_currBackBuffer + 1) % SwapChainBufferCount;

    // Wait until frame commands are complete. This waiting is inefficient and is
    // done for simplicity. Later we will show how to organize our rendering code
    // so we do not have to wait per frame.
    FlushCommandQueue();
}

void NeneApp::PopulateCommandList()
{
    // Command list allocators can only be reset when the associated 
    // command lists have finished execution on the GPU; apps should use 
    // fences to determine GPU execution progress.
    ThrowIfFailed(m_commandAllocator->Reset());

    // However, when ExecuteCommandList() is called on a particular command 
    // list, that command list can then be reset at any time and must be before 
    // re-recording.
    ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), m_pipelineState.Get()));

    // Set necessary state.
    m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
    m_commandList->RSSetViewports(1, &m_viewport);
    m_commandList->RSSetScissorRects(1, &m_scissorRect);

    // Indicate that the back buffer will be used as a render target.
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    m_commandList->ResourceBarrier(1, &barrier);

    m_commandList->OMSetRenderTargets(1, &CurrentBackBufferView(), FALSE, nullptr);

    // Record commands.
    m_commandList->ClearRenderTargetView(CurrentBackBufferView(), DirectX::Colors::LightSteelBlue, 0, nullptr);

    // Indicate that the back buffer will now be used to present.
    barrier = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    m_commandList->ResourceBarrier(1, &barrier);

    ThrowIfFailed(m_commandList->Close());
}

void NeneApp::BuildDescriptorHeaps()
{
}

void NeneApp::BuildConstantBuffers()
{
}

void NeneApp::BuildRootSignature()
{
}

void NeneApp::BuildShadersAndInputLayout()
{
}

void NeneApp::BuildGeometry()
{
}

void NeneApp::BuildPSO()
{
}
