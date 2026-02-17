#pragma once

#include <wrl.h>
#include <d3d12.h>
#include <unordered_map>
#include <vector>
#include <string>
#include <array>

#include "../../Common/d3dUtil.h"
#include "../../Common/MathHelper.h"
#include "FrameResource.h"

using Microsoft::WRL::ComPtr;

struct RenderItem
{
    RenderItem() = default;
    RenderItem(const RenderItem& rhs) = delete;

    DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

    int NumFramesDirty = 3;
    UINT ObjCBIndex = -1;

    Material* Mat = nullptr;
    MeshGeometry* Geo = nullptr;

    D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    UINT IndexCount = 0;
    UINT StartIndexLocation = 0;
    int BaseVertexLocation = 0;

    std::string Name;

    DirectX::BoundingBox aabb;

};




class RenderingSystem
{
public:
    RenderingSystem(ID3D12Device* device, UINT cbvSrvDescriptorSize);
    ~RenderingSystem();

    void Initialize(ID3D12GraphicsCommandList* cmdList);
    void OnResize(float aspectRatio);
    void Draw(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems, FrameResource* frame, const PassConstants& passConstants);

    // ���������� ��������
    void BuildRootSignature();
    void BuildDescriptorHeaps(const std::unordered_map<std::string, std::unique_ptr<Texture>>& textures);
    void BuildShadersAndInputLayout();
    void BuildPSOs(DXGI_FORMAT backBufferFormat, DXGI_FORMAT depthStencilFormat, bool msaaState, UINT msaaQuality);

    void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);

    // ������ � GPU-��������
    ID3D12RootSignature* GetRootSignature() const;
    ID3D12DescriptorHeap* GetSrvHeap() const;
    UINT GetCbvSrvDescriptorSize() const;

    const std::unordered_map<std::string, ComPtr<ID3DBlob>>& GetShaders() const;
    const std::unordered_map<std::string, ComPtr<ID3D12PipelineState>>& GetPSOs() const;
    std::unordered_map<std::string, std::unique_ptr<Material>>* mMaterials;

private:
    std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();

    ID3D12Device* mDevice = nullptr;

    ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
    ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;

    std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;
    
    std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;

    std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

    UINT mCbvSrvDescriptorSize = 0;
};
