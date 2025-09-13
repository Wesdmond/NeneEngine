#pragma once
#include "DX12App.h"
#include "Common/GeometryGenerator.h"
#include "Common/Camera.h"
#include "UIManager.h"
#include "Inputs/InputDevice.h"
#include <SimpleMath.h>
#include "FrameResource.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;

// Lightweight structure stores parameters to draw a shape.  This will
// vary from app-to-app.
struct RenderItem
{
	RenderItem() = default;

    // World matrix of the shape that describes the object's local space
    // relative to the world space, which defines the position, orientation,
    // and scale of the object in the world.
    XMFLOAT4X4 World = MathHelper::Identity4x4();

	XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

	// Dirty flag indicating the object data has changed and we need to update the constant buffer.
	// Because we have an object cbuffer for each FrameResource, we have to apply the
	// update to each FrameResource.  Thus, when we modify obect data we should set 
	// NumFramesDirty = gNumFrameResources so that each frame resource gets the update.
	int NumFramesDirty = gNumFrameResources;

	// Index into GPU constant buffer corresponding to the ObjectCB for this render item.
	UINT ObjCBIndex = -1;

	Material* Mat = nullptr;
	MeshGeometry* Geo = nullptr;

    // Primitive topology.
    D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    // DrawIndexedInstanced parameters.
    UINT IndexCount = 0;
    UINT StartIndexLocation = 0;
    int BaseVertexLocation = 0;
};

class NeneApp : public DX12App
{
public:
    NeneApp(HINSTANCE mhAppInst, HWND mhMainWnd, std::shared_ptr<InputDevice> inputDevice);
    ~NeneApp();
    bool Initialize() override;
    virtual void OnResize()override;
    void Update(const GameTimer& gt) override;
    void Draw(const GameTimer& gt) override;

private:
    void PopulateCommandList();
    void SetDelegates();
    void InitCamera();

    void LoadTextures();
    bool LoadTexture(const std::string& filename);
    void LoadObjModel(const std::string& filename, SimpleMath::Matrix Transform);
    void BuildRootSignature();
    void BuildDescriptorHeaps();
    void BuildShadersAndInputLayout();
    void BuildBoxGeometry();
    void BuildDisplacementTestGeometry();
    void BuildMountainGeometry();
    void BuildPSOs();
	void BuildTextureSRVs();
    void BuildFrameResources();
    void BuildMaterials();
    void BuildRenderItems();
    void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<std::shared_ptr<RenderItem>>& ritems);

    std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();

    void UpdateInputs(const GameTimer& gt);
    void UpdateCamera(const GameTimer& gt);
    void AnimateMaterials(const GameTimer& gt);
    void UpdateObjectCBs(const GameTimer& gt);
    void UpdateMaterialCBs(const GameTimer& gt);
    void UpdateMainPassCB(const GameTimer& gt);

    DirectX::BoundingBox ComputeBounds(const std::vector<Vertex>& verts);
private:
    Camera m_camera;
    UIManager m_uiManager;
    std::shared_ptr<InputDevice> m_inputDevice;

#pragma region App Resources
    std::vector<std::unique_ptr<FrameResource>> mFrameResources;
    FrameResource* mCurrFrameResource = nullptr;
    int mCurrFrameResourceIndex = 0;

    UINT mCbvSrvDescriptorSize = 0;

    ComPtr<ID3D12RootSignature> mRootSignature = nullptr;

	ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;
    ComPtr<ID3D12DescriptorHeap> m_imguiSrvHeap = nullptr;

	std::unordered_map<std::string, std::shared_ptr<MeshGeometry>> mGeometries;
	std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;
	std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;
	std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;
    std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;

    std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;


    // List of all the render items.
    std::vector<std::shared_ptr<RenderItem>> mAllRitems;

	std::unordered_map<std::string, std::shared_ptr<RenderItem>> mModelRenderItems;
	
    // Render items divided by PSO.
    std::vector<std::shared_ptr<RenderItem>> mOpaqueRitems;
    std::vector<std::shared_ptr<RenderItem>> m_tessMesh;
    std::vector<std::shared_ptr<RenderItem>> mNormalRitems;
    std::vector<std::shared_ptr<RenderItem>> mBasicRitems;

    PassConstants mMainPassCB;;

    XMFLOAT4X4 mView = MathHelper::Identity4x4();
    XMFLOAT4X4 mProj = MathHelper::Identity4x4();

    UINT mPassCbvOffset = 0;

#pragma endregion
    bool mIsWireframe = true;
    float m_cameraSpeed = 10;
    float m_mouseSensitivity = 0.005f;

    // Inputs
    DirectX::SimpleMath::Vector2 m_mousePos;
    DirectX::SimpleMath::Vector2 m_mouseDelta;
    float m_mouseWheelDelta;
};