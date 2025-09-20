#pragma once
#include "DX12App.h"
#include "Common/GeometryGenerator.h"
#include "Common/Camera.h"
#include "UIManager.h"
#include "Inputs/InputDevice.h"
#include <SimpleMath.h>
#include "FrameResource.h"
#include "GBuffer.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;

// Lightweight structure stores parameters to draw a shape.
struct RenderItem
{
	RenderItem() = default;

    // World matrix of the shape that describes the object's local space
    // relative to the world space, which defines the position, orientation,
    // and scale of the object in the world.
    SimpleMath::Matrix World        = SimpleMath::Matrix::Identity;
    SimpleMath::Matrix TexTransform = SimpleMath::Matrix::Identity;


	// Dirty flag indicating the object data has changed and we need to update the constant buffer.
	// Because we have an object cbuffer for each FrameResource, we have to apply the
	// update to each FrameResource.  Thus, when we modify obect data we should set 
	// NumFramesDirty = gNumFrameResources so that each frame resource gets the update.
	int NumFramesDirty = gNumFrameResources;

	// Index into GPU constant buffer corresponding to the ObjectCB for this render item.
	UINT ObjCBIndex = -1;

	Material* Mat = nullptr;
	MeshGeometry* Geo = nullptr;
	MeshGeometry* GeoLow = nullptr;
	MeshGeometry* GeoHigh = nullptr;
    MeshGeometry* CurrentGeo = nullptr;

    // Choose PSO
    std::string PSOType = "Opaque";
    // Primitive topology.
    D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    // DrawIndexedInstanced parameters.
    UINT IndexCount = 0;
    UINT StartIndexLocation = 0;
    int BaseVertexLocation = 0;

    // For LOD
    std::string SubmeshName;
    UINT SelectedIndexCount = 0;
    UINT SelectedStartIndexLocation = 0;
    int SelectedBaseVertexLocation = 0;

    bool UseLOD = true;
    float LODThreshold = 50.0f;
    std::string Name;
    bool Visible = false;
};

// Octree for frustum culling (hierarchical spatial partitioning)
struct OctreeNode {
    DirectX::BoundingBox Bounds;  // AABB for this octant
    std::vector<std::shared_ptr<RenderItem>> Items;  // Leaf items only
    std::unique_ptr<OctreeNode> Children[8];  // 8 octants
    bool IsLeaf = true;

    OctreeNode(const DirectX::BoundingBox& box) : Bounds(box) {}
};

struct LightItem {
    LightItem() = default;
    SimpleMath::Matrix World;

    // Dirty flag indicating the object data has changed and we need to update the constant buffer.
    // Because we have an object cbuffer for each FrameResource, we have to apply the
    // update to each FrameResource.  Thus, when we modify obect data we should set 
    // NumFramesDirty = gNumFrameResources so that each frame resource gets the update.
    int NumFramesDirtyLight = gNumFrameResources;

    std::string Name = "NoName Light";

    // Index into GPU constant buffer corresponding to the ObjectCB for this render item.
    UINT LightCBIndex = -1;

    MeshGeometry* Geo = nullptr;

    // light settings
    Light light;
    UINT lightType;
    bool IsDrawingDebugGeometry = false;

    // Choose PSO
    std::string PSOType = "Opaque";
    // Primitive topology.
    D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    // DrawIndexedInstanced parameters.
    UINT IndexCount = 0;
    UINT StartIndexLocation = 0;
    int BaseVertexLocation = 0;

    bool Visible = false;
};

const int MAX_DYNAMIC_LIGHTS = 100;
struct DynamicLight {
    std::shared_ptr<LightItem> lightRI;
    SimpleMath::Vector3 velocity;
    float linearDamping;

    bool isMoving = true;
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

#pragma region geometry
    void BuildBoxLightGeometry();
    void BuildSphereLightGeometry();
    void BuildQuadLightGeometry();
    void BuildCylinderLightGeometry();
    void BuildLightGeometries();
    void BuildBoxGeometry();
    void BuildManyBoxes(UINT count = 1000);
    void BuildDisplacementTestGeometry();
    void ShootLight(SimpleMath::Vector3 position, SimpleMath::Vector3 size, SimpleMath::Vector3 velocity, SimpleMath::Color color, float linearDamping);
    void BuildPlane(float width, float height, UINT x, UINT y, const std::string& meshName, const std::string& matName, const SimpleMath::Matrix& transform, D3D12_PRIMITIVE_TOPOLOGY type);
    void BuildRenderItems();
#pragma endregion

    void BuildPSOs();
	void BuildTextureSRVs();
    void BuildFrameResources();
    void BuildMaterials();
    void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<std::shared_ptr<RenderItem>>& ritems);
    void DrawForward();
    void DrawDeffered();
    void DrawUI();

    std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();

    void UpdateInputs(const GameTimer& gt);
    void UpdateCamera(const GameTimer& gt);
    void AnimateMaterials(const GameTimer& gt);
    void UpdateDynamicLights(const GameTimer& gt);
    void UpdateObjectCBs(const GameTimer& gt);
    void UpdateMaterialCBs(const GameTimer& gt);
    void UpdateMainPassCB(const GameTimer& gt);
    void UpdateLightCB(const GameTimer& gt);

    void UpdateVisibleRenderItems();
    DirectX::BoundingBox ComputeBounds(const std::vector<Vertex>& verts);
private:
    Camera m_camera;
    UIManager m_uiManager;
    std::shared_ptr<InputDevice> m_inputDevice;

    // GBuffer
    GBuffer m_gBuffer;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvGBufferHeap;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_dsvGBufferHeap;
    ComPtr<ID3D12RootSignature> m_defferedRootSignature;

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
    std::vector<std::shared_ptr<RenderItem>> mTessRitems;
    std::vector<std::shared_ptr<RenderItem>> mNormalRitems;
    std::vector<std::shared_ptr<RenderItem>> mBasicRitems;
    std::vector<std::shared_ptr<LightItem>> mLightRitems;
    std::vector<std::shared_ptr<DynamicLight>> m_DynamicLights;

    PassConstants mMainPassCB;;

    SimpleMath::Matrix mView = SimpleMath::Matrix::Identity;
    SimpleMath::Matrix mProj = SimpleMath::Matrix::Identity;

    UINT mPassCbvOffset = 0;

#pragma endregion

#pragma region Frustrum Culling
    std::vector<std::shared_ptr<RenderItem>> mVisibleRitems;
    float mLODDistanceThreshold = 30.0f;
    bool mUseFrustumCulling = true;
    int mSelectedRItemIndex = 0; // Selected RenderItem index in ImGui

    // Octree
    std::unique_ptr<OctreeNode> mOctreeRoot = nullptr;
    DirectX::BoundingBox mSceneBounds;
    int mOctreeMaxDepth = 4;
    bool mRebuildOctree = true;
    std::unordered_set<std::shared_ptr<RenderItem>> addedItems;

    void BuildOctree(const std::vector<std::shared_ptr<RenderItem>>& items, OctreeNode* node, int depth);
    void CollectVisibleItems(OctreeNode* node, const DirectX::BoundingFrustum& frustum, std::vector<std::shared_ptr<RenderItem>>& visibleItems, DirectX::XMVECTOR eyePos);
    DirectX::BoundingBox ComputeRenderItemBounds(const std::shared_ptr<RenderItem>& ri);
    void RebuildOctreeIfNeeded();
#pragma endregion
    bool mIsWireframe = false;
    float m_cameraSpeed = 20;
    float m_mouseSensitivity = 0.01f;

    // Inputs
    DirectX::SimpleMath::Vector2 m_mousePos     = DirectX::SimpleMath::Vector2::Zero;
    DirectX::SimpleMath::Vector2 m_mouseDelta   = DirectX::SimpleMath::Vector2::Zero;
    float m_mouseWheelDelta;
};