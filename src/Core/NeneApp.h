#pragma once
#include "DX12App.h"
#include "Common/GeometryGenerator.h"
#include "Common/Camera.h"

class NeneApp : public DX12App
{
public:
    NeneApp(HINSTANCE mhAppInst, HWND mhMainWnd);
    bool Initialize() override;
    void Update(const GameTimer& gt) override;
    void Draw(const GameTimer& gt) override;

private:
    void PopulateCommandList();
    bool LoadAssets();
    void BuildDescriptorHeaps();
    void BuildConstantBuffers();
    void BuildRootSignature();
    void BuildShadersAndInputLayout();
    void BuildGeometry();
    void BuildPSO();

    // App resources
    Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_indexBuffer;
    D3D12_INDEX_BUFFER_VIEW m_indexBufferView;

private:
    Camera m_camera;
};