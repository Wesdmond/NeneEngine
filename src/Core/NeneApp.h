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

struct Particle {
    SimpleMath::Vector3 pos;
    SimpleMath::Vector3 startPos;
    SimpleMath::Vector3 vel;
    float life;
    float lifetime;
    float size;
    float rot;
    int alive;
    SimpleMath::Color color;
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

    void BuildRootSignature();
    void BuildDescriptorHeaps();
    void BuildShadersAndInputLayout();
    void BuildPSOs();
    void BuildTextureSRVs();
    void BuildFrameResources();
    void BuildMaterials();
    void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<std::shared_ptr<RenderItem>>& ritems);
    void DrawForward();
    void DrawDeffered();
    void DrawUI();
    void DrawParticles();
    void DrawPostProcess();
    void DrawSkyBox();

    void LoadTextures();
    bool LoadTexture(const std::string& filename);
    bool LoadTexture(const std::string& texName, const std::string& filename, D3D12_SRV_DIMENSION textureType = D3D12_SRV_DIMENSION_TEXTURE2D);
    void LoadObjModel(const std::string& filename, SimpleMath::Matrix Transform);

    // geometry
    void BuildBoxLightGeometry();
    void BuildSphereLightGeometry();
    void BuildQuadLightGeometry();
    void BuildCylinderLightGeometry();
    void BuildLightGeometries();
    void BuildBoxGeometry();
    void BuildPBRTestObjects();
    void BuildSkyBox();
    void BuildManyBoxes(UINT count = 1000);
    void BuildDisplacementTestGeometry();
    void ShootLight(SimpleMath::Vector3 position, SimpleMath::Vector3 size, SimpleMath::Vector3 velocity, SimpleMath::Color color, float linearDamping);
    void BuildPlane(float width, float height, UINT x, UINT y, const std::string& meshName, const std::string& matName, const SimpleMath::Matrix& transform, D3D12_PRIMITIVE_TOPOLOGY type);
    void BuildRenderItems();

    // post-process
    void BuildPostProcessHeaps();
    void BuildPostProcessResources();
    void BuildPostProcessSignature();
    void BuildPostProcessPSO();

    // particles
    void BuildParticleResources();
    void BuildParticleDescriptors();
    void BuildParticleCS_RS();
    void BuildParticleGfx_RS();
    void BuildParticlePSO();

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
    ComPtr<ID3D12DescriptorHeap> m_rtvGBufferHeap;
    ComPtr<ID3D12DescriptorHeap> m_dsvGBufferHeap;
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
    bool isSkyBoxEnabled = true;
    std::shared_ptr<RenderItem> skyBoxRI;

    enum skyBoxEnums {
        prefilter   = 0,
        brdf        = 1,
        irradiance  = 2
    };
    CD3DX12_GPU_DESCRIPTOR_HANDLE skyBoxHandles[3];
	
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
    bool mUseFrustumCulling = false;
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

#pragma region post-process resources
    ComPtr<ID3D12Resource> mPostProcessRenderTarget;
    ComPtr<ID3D12DescriptorHeap> mPostProcessRTVHeap;
    ComPtr<ID3D12DescriptorHeap> mPostProcessSRVHeap;
    std::unordered_map<std::string, ComPtr<ID3DBlob>> mPostProcessShaders;
    ComPtr<ID3D12RootSignature> mPostProcessRootSignature;
    CD3DX12_GPU_DESCRIPTOR_HANDLE mPostProcessSrvGpuHandle;

#pragma endregion

#pragma region Particle System
    ComPtr<ID3D12Resource> mParticlesA; // default SRV/UAV
    ComPtr<ID3D12Resource> mParticlesB; // default SRV/UAV
    ID3D12Resource* inRes;
    ID3D12Resource* outRes;
    CD3DX12_GPU_DESCRIPTOR_HANDLE srv_ParticleA;
    CD3DX12_GPU_DESCRIPTOR_HANDLE uav_ParticleA;
    CD3DX12_GPU_DESCRIPTOR_HANDLE srv_ParticleB;
    CD3DX12_GPU_DESCRIPTOR_HANDLE uav_ParticleB;

    std::unique_ptr<UploadBuffer<Particle>> mParticlesInit; // UPLOAD
    struct SimCB { float dt; XMFLOAT3 gravity; };
    std::unique_ptr<UploadBuffer<SimCB>> mSimCB; // UPLOAD
    // shader-visible heap for SRV/UAV particles + SRV billboards
    ComPtr<ID3D12DescriptorHeap> mParticleHeap;
    enum { KSRV_A = 0, KUAV_A = 1, KSRV_B = 2, KUAV_B = 3, KSRV_Sprite = 4 };
    ComPtr<ID3D12RootSignature> mParticleCS_RS; // CS RS
    ComPtr<ID3D12RootSignature> mParticleGfx_RS; // PS RS
    ComPtr<ID3D12PipelineState> mParticleCS_PSO; // CS PSO
    ComPtr<ID3D12PipelineState> mParticleGfx_PSO; // PS PSO
    UINT mParticleCount = 10000;
    bool isParticleEnabled = false;
    SimpleMath::Vector3 mParticleForce = SimpleMath::Vector3(0.0f, 2.0f, 0.0f); // For ImGui control, replacing origin
#pragma endregion

#pragma region Utility
    bool mIsWireframe = false;
    float m_cameraSpeed = 20;
    float m_mouseSensitivity = 0.01f;

    // Inputs
    DirectX::SimpleMath::Vector2 m_mousePos     = DirectX::SimpleMath::Vector2::Zero;
    DirectX::SimpleMath::Vector2 m_mouseDelta   = DirectX::SimpleMath::Vector2::Zero;
    float m_mouseWheelDelta;
#pragma endregion
};