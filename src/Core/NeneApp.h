#pragma once
#include "DX12App.h"
#include "Common/GeometryGenerator.h"
#include "Common/Camera.h"
#include "Inputs/InputDevice.h"
#include <SimpleMath.h>

class NeneApp : public DX12App
{
public:
    NeneApp(HINSTANCE mhAppInst, HWND mhMainWnd, std::shared_ptr<InputDevice> inputDevice);
    bool Initialize() override;
    void Update(const GameTimer& gt) override;
    void Draw(const GameTimer& gt) override;

private:
    void PopulateCommandList();
    void BuildDescriptorHeaps();
    void BuildConstantBuffers();
    void BuildRootSignature();
    void BuildShadersAndInputLayout();
    void BuildGeometry();
    void BuildPSO();
    void UpdateInputs(const GameTimer& gt);

private:
    Camera m_camera;
    std::shared_ptr<InputDevice> m_inputDevice;

    // App resources
    Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_indexBuffer;
    D3D12_INDEX_BUFFER_VIEW m_indexBufferView;

    // Inputs
    DirectX::SimpleMath::Vector2 m_mousePos;
    DirectX::SimpleMath::Vector2 m_mouseDelta;
    float m_mouseWheelDelta;
};