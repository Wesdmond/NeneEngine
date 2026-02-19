//***************************************************************************************
// TexColumnsApp.cpp by Frank Luna (C) 2015 All Rights Reserved.
//***************************************************************************************
#include "../../Common/Camera.h"
#include "../../Common/d3dApp.h"
#include "../../Common/MathHelper.h"
#include "../../Common/UploadBuffer.h"
#include "../../Common/GeometryGenerator.h"
#include <filesystem>
#include "FrameResource.h"
#include <iostream>

#include "imgui_impl_dx12.h"
#include "imgui_impl_win32.h"
#include "imgui.h"
Camera cam;
static int imguiID = 0;
using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")

const int gNumFrameResources = 3;
bool f = false;

// Lightweight structure stores parameters to draw a shape.  This will
// vary from app-to-app.

struct LodLevel
{
	MeshGeometry* Geo = nullptr;
	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	int BaseVertexLocation = 0;
	float SwitchDistance = 0.0f;
	DirectX::BoundingBox Bounds;
};

struct Tile
{
	XMFLOAT3 worldPos;

	int lodLevel;
	int maxLodLevel;
	float tileSize;
	int tileIndex;

	int rItemIndex;
	int NumFramesDirty;
	DirectX::BoundingBox Bounds;
	bool isVisible = true;
};


struct Node
{
	Tile* tile;
	int nodeDepth;
	std::unique_ptr<Node> children[4];
	Node* parent;
	BoundingBox Bounds;          
          
	bool ShouldSplitTree(const XMFLOAT3& cameraPos, float heightscale, int mapsize) const;
	void UpdateTreeVisibility(BoundingFrustum& frustum, const XMFLOAT3& cameraPos, std::vector<Tile*>& visibleTiles, float heightscale, int mapsize);
};

struct AABB
{
	XMFLOAT3 minPoint;
	XMFLOAT3 maxPoint;
	BoundingBox aabb;
	bool IntersectsFrustum(const XMFLOAT4 frustumPlanes[6]) const;
};


class Terrain
{
public:
	Terrain() {};

	void InitializeTerrain(ID3D12Device* device, int HeightMapIndex,
		float worldSize, int maxLOD);
	std::vector<std::shared_ptr<Tile>>& GetAllTiles();
	void GetVisibleTiles(std::vector<Tile*>& outTiles);
	float mWorldSize;
	int mHmapIndex = 0;
	int mHeightScale;
	int renderlodlevel = 0;
	int tileRenderIndex = 0;
	std::unique_ptr<Node> mRootNode;


	std::vector<float> mHeightData;
	int mHeightmapWidth = 0;
	int mHeightmapHeight = 0;

	void BuildQuadTree(Node* node, int x, int y, int size, int depth);
	BoundingBox CalculateTileAABB(const XMFLOAT3& pos, float size, float minHeight, float maxHeight);
	void Update(const XMFLOAT3& cameraPos, BoundingFrustum& frustum);


	float GetWorldSize() const { return mWorldSize; };
	float Terrain::SampleHeight(float u, float v);

private:

	ComPtr<ID3D12Resource> mHeightmapTexture;
	int LodLevel;
	int maxLodLevel;
	int tileIndex = 0;

	std::vector<std::shared_ptr<Tile>>mAllTiles;
	std::vector<Tile*> mVisibleTiles;
	
};

void Terrain::Update(const XMFLOAT3& cameraPos, BoundingFrustum& frustum)
{
	mVisibleTiles.clear();
	if (mRootNode)
	{
		mRootNode->UpdateTreeVisibility(frustum, cameraPos, mVisibleTiles, mHeightScale, (int)mWorldSize);
	}
}

void Terrain::GetVisibleTiles(std::vector<Tile*>& outTiles)
{

	//for (auto& t : mAllTiles)
	//	mVisibleTiles.push_back(t.get());
	outTiles = mVisibleTiles;



}

void Terrain::InitializeTerrain(ID3D12Device* device, int HeightMapIndex,
	float worldSize, int inMaxLodLevel)
{
	mWorldSize = worldSize;
	maxLodLevel = inMaxLodLevel;
	mHmapIndex = HeightMapIndex;

	mRootNode = std::make_unique<Node>();
	mRootNode->nodeDepth = 0;


	int initialSize = (int)worldSize;
	BuildQuadTree(mRootNode.get(), 0, 0, initialSize, 0);
}


void Terrain::BuildQuadTree(Node* node, int x, int y, int size, int depth)
{
	node->nodeDepth = depth;

	float tileSize = mWorldSize / (1 << depth);

	node->Bounds = CalculateTileAABB(XMFLOAT3((float)x, 0, (float)y), tileSize, -10.0f, 400.0f);

	auto tile = std::make_unique<Tile>();
	tile->worldPos = XMFLOAT3((float)x, 0, (float)y);
	tile->lodLevel = depth;
	tile->isVisible = true;
	tile->tileSize = tileSize;
	tile->Bounds = node->Bounds;
	tile->tileIndex = tileIndex++;
	mAllTiles.push_back(std::move(tile));
	node->tile = mAllTiles.back().get();
	if (depth != maxLodLevel)
	{
		int halfSize = tileSize / 2;
		for (int i = 0; i < 4; i++)
		{
			node->children[i] = std::make_unique<Node>();
			int childX = x + (i % 2) * halfSize;
			int childY = y + (i / 2) * halfSize;
			BuildQuadTree(node->children[i].get(), childX, childY, halfSize, depth + 1);
		}
	}
}


void Node::UpdateTreeVisibility(BoundingFrustum& frustum, const XMFLOAT3& cameraPos, std::vector<Tile*>& visibleTiles, float heightscale, int mapsize)
{
	if (frustum.Contains(Bounds) == DISJOINT)
	{
		return;
	}

	if (!children[0] || !ShouldSplitTree(cameraPos, heightscale, mapsize))
	{
		
		if (tile)
		{
			visibleTiles.push_back(tile);
		}
	}
	else
	{
		
		for (int i = 0; i < 4; i++)
		{
			if (children[i])
			{
				children[i]->UpdateTreeVisibility(frustum, cameraPos, visibleTiles, heightscale, mapsize);
			}
		}
	}
}

bool Node::ShouldSplitTree(const XMFLOAT3& cameraPos, float heightscale, int mapsize) const
{
	auto camPos = cameraPos;
	camPos.y = 0;
	XMVECTOR camPosVec = XMLoadFloat3(&camPos);
	float lodneeddist = (mapsize / 2.0f - nodeDepth * mapsize / 16);
	BoundingSphere sphere;
	sphere.Center = cameraPos;
	sphere.Radius = lodneeddist;
	if (sphere.Intersects(Bounds))
	{
		return true;
	}
	return false;
}

BoundingBox Terrain::CalculateTileAABB(const XMFLOAT3& pos, float size, float minHeight, float maxHeight)
{
	BoundingBox aabb;
	auto minPoint = XMFLOAT3(pos.x, 0, pos.z);
	auto maxPoint = XMFLOAT3(pos.x + size, 1000, pos.z + size);
	XMVECTOR pt1 = XMLoadFloat3(&minPoint);
	XMVECTOR pt2 = XMLoadFloat3(&maxPoint);
	BoundingBox::CreateFromPoints(aabb, pt1, pt2);
	return aabb;
}

struct RenderItem
{
	RenderItem() = default;
    RenderItem(const RenderItem& rhs) = delete;

    // World matrix of the shape that describes the object's local space
    // relative to the world space, which defines the position, orientation,
    // and scale of the object in the world.
    XMFLOAT4X4 World = MathHelper::Identity4x4();
    XMMATRIX ScaleM = XMMatrixIdentity();
	XMMATRIX RotationM = XMMatrixIdentity();
	XMMATRIX TranslationM = XMMatrixIdentity();
	XMFLOAT3 Position = { 0.0f, 0.0f, 2.0f };
	XMFLOAT3 RotationAngle = { 0.0f, .0f, 0.0f };
	XMFLOAT3 Scale = { 1.0f, 1.0f, 1.0f };
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
	std::string Name;
};

class TexColumnsApp : public D3DApp
{
public:
    TexColumnsApp(HINSTANCE hInstance);
    TexColumnsApp(const TexColumnsApp& rhs) = delete;
    TexColumnsApp& operator=(const TexColumnsApp& rhs) = delete;
    ~TexColumnsApp();

    virtual bool Initialize()override;

private:
    virtual void OnResize()override;
    virtual void Update(const GameTimer& gt)override;
    virtual void Draw(const GameTimer& gt)override;
	virtual void DeferredDraw(const GameTimer& gt)override;
    virtual void OnMouseDown(WPARAM btnState, int x, int y)override;
    virtual void OnMouseUp(WPARAM btnState, int x, int y)override;
    virtual void OnMouseMove(WPARAM btnState, int x, int y)override;
	virtual void MoveBackFwd(float step)override;
	virtual void MoveLeftRight(float step)override;
	virtual void MoveUpDown(float step)override;
	void OnKeyPressed(const GameTimer& gt, WPARAM key) override;
	void OnKeyReleased(const GameTimer& gt, WPARAM key) override;
	std::wstring GetCamSpeed() override;
	void UpdateCamera(const GameTimer& gt);
	void BuildShadowMapViews();
	void AnimateMaterials(const GameTimer& gt);
	void UpdateObjectCBs(const GameTimer& gt);
	void UpdateLightCBs(const GameTimer& gt);
	void UpdateMaterialCBs(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& gt);
	void CreateGBuffer() override;
	void LoadAllTextures();
	void LoadTexture(const std::string& name);
    void BuildRootSignature();
    void BuildLightingRootSignature();
	void BuildShadowPassRootSignature();
	void BuildLights();
	void SetLightShapes();
	void BuildDescriptorHeaps();
    void BuildShadersAndInputLayout();
    void BuildShapeGeometry();
    void BuildPSOs();
    void BuildFrameResources();
	void CreateMaterial(std::string _name, int _CBIndex, int _SRVDiffIndex, int _SRVNMapIndex, XMFLOAT4 _DiffuseAlbedo, XMFLOAT3 _FresnelR0, float _Roughness);
    void BuildMaterials();
	void RenderCustomMesh(std::string unique_name, std::string meshname, std::string materialName, XMFLOAT3 Scale, XMFLOAT3 Rotation, XMFLOAT3 Position);
	void BuildCustomMeshGeometry(std::string name, UINT& meshVertexOffset, UINT& meshIndexOffset, UINT& prevVertSize, UINT& prevIndSize, std::vector<Vertex>& vertices, std::vector<std::uint16_t>& indices, MeshGeometry* Geo);
    void BuildRenderItems();
	void DrawSceneToShadowMap();
    void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 7> GetStaticSamplers();

	void DrawShadowDebug(ID3D12GraphicsCommandList* cmdList, UINT size);

	void CreateSpotLight(XMFLOAT3 pos, XMFLOAT3 rot, XMFLOAT3 color, float faloff_start, float faloff_end, float strength, float spotpower);
	void CreatePointLight(XMFLOAT3 pos, XMFLOAT3 color, float faloff_start, float faloff_end,float strength);

	///

	void GenerateTileGeometry(const XMFLOAT3& worldPos, float tileSize, int lodLevel, std::vector<Vertex>& vertices, std::vector<std::uint32_t>& indices);
	void BuildTerrainGeometry();
	void BuildTerrainRootSignature();
	void BuildTerrainComputeRootSignature();
	void DrawTileRenderItems(ID3D12GraphicsCommandList* cmdList, std::vector<Tile*> tiles, int HeightIndex);
	void UpdateTerrainCBs(const GameTimer& gt);
	void UpdateTerrain(const GameTimer& gt);

	bool TexColumnsApp::RaycastToTerrainUV(int sx, int sy, XMFLOAT2& uvOut, XMFLOAT3& hitWorld);
	void TexColumnsApp::InitializeHeightModificationTexture();
	void TexColumnsApp::ApplyBrushWithPersistence(const XMFLOAT2& uv, bool raise);
	void TexColumnsApp::UpdateHeightModificationTexture();



private:
	std::unordered_map<std::string, unsigned int>ObjectsMeshCount;
    std::vector<std::unique_ptr<FrameResource>> mFrameResources;
    FrameResource* mCurrFrameResource = nullptr;
    int mCurrFrameResourceIndex = 0;
	//
	std::unordered_map<std::string, int>TexOffsets;
	//
    UINT mCbvSrvDescriptorSize = 0;

    ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
    ComPtr<ID3D12RootSignature> mLightingRootSignature = nullptr;
	ComPtr<ID3D12RootSignature> mShadowPassRootSignature = nullptr;
	ComPtr<ID3D12RootSignature> mTerrainRootSignature = nullptr;
	ComPtr<ID3D12RootSignature> mTerrainComputeRootSignature = nullptr;



	ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;
	ComPtr<ID3D12DescriptorHeap> m_ImGuiSrvDescriptorHeap; // Member variable

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;
	std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;
	std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;

    std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;
 
	// List of all the render items.
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;
	std::vector<Light>mLights;
	// Render items divided by PSO.
	std::vector<RenderItem*> mOpaqueRitems;

    PassConstants mMainPassCB;
	XMFLOAT3 mEyePos = { 0.0f, 0.0f, 0.0f };
	XMFLOAT4X4 mView = MathHelper::Identity4x4();
	XMFLOAT4X4 mProj = MathHelper::Identity4x4();

    float mTheta = 1.5f*XM_PI;
    float mPhi = 0.2f*XM_PI;
    float mRadius = 15.0f;

    POINT mLastMousePos;

	//
	std::unique_ptr<Terrain> mTerrain;
	std::vector<Tile*> m_visibleTerrainTiles;
	float heightScale = 100;


	// G-Buffer ресурсы
	ComPtr<ID3D12Resource> mGBufferPosition;
	ComPtr<ID3D12Resource> mGBufferNormal;
	ComPtr<ID3D12Resource> mGBufferAlbedo;
	ComPtr<ID3D12Resource> mGBufferDepthStencil;
	ComPtr<ID3D12DescriptorHeap> mGBufferSrvHeap = nullptr;

	// Дескрипторы для G-Buffer
	CD3DX12_CPU_DESCRIPTOR_HANDLE mGBufferRTVs[3]; // 0:Position, 1:Normal, 2:Albedo
	CD3DX12_CPU_DESCRIPTOR_HANDLE mGBufferDSV;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mGBufferSRVs[3]; // SRV для шейдеров

	UINT mGBufferRTVDescriptorSize;
	UINT mGBufferDSVDescriptorSize;

	// shadow resources 
	const UINT SHADOW_MAP_WIDTH = 2048;
	const UINT SHADOW_MAP_HEIGHT = 2048;
	const DXGI_FORMAT SHADOW_MAP_FORMAT = DXGI_FORMAT_R24G8_TYPELESS; // Resource format
	const DXGI_FORMAT SHADOW_MAP_DSV_FORMAT = DXGI_FORMAT_D24_UNORM_S8_UINT; // DSV format
	const DXGI_FORMAT SHADOW_MAP_SRV_FORMAT = DXGI_FORMAT_R24_UNORM_X8_TYPELESS; // SRV format
	Microsoft::WRL::ComPtr<ID3D12Resource> mShadowMap;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mShadowDsvHeap; // A separate heap for shadow map DSVs
	D3D12_VIEWPORT mShadowViewport;
	D3D12_RECT mShadowScissorRect;


	int mShadowDebugLightIndex = 0; // какой light показываем в overlay
	std::vector<int> mShadowCastingLights;
	int mShadowDebugLightIdx = 0; // индекс в этом массиве

	// Размеры как у окна
	UINT width = mClientWidth;
	UINT height = mClientHeight;

	// Форматы:
	const DXGI_FORMAT positionFormat = DXGI_FORMAT_R32G32B32A32_FLOAT;
	const DXGI_FORMAT normalFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
	const DXGI_FORMAT albedoFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

	bool     mBrushActive = false; 
	XMFLOAT2 mBrushHitUV = { -1.0f, -1.0f }; 
	float    mBrushRadius = 0.05f;     
	float mBrushStrength = 0.1f;

	Microsoft::WRL::ComPtr<ID3D12Resource> mHeightMapTexture;        // SRV версия
	Microsoft::WRL::ComPtr<ID3D12Resource> mHeightMapTextureUAV;     // UAV версия для записи
	Microsoft::WRL::ComPtr<ID3D12Resource> mHeightMapStagingBuffer;  // Для загрузки данных

	int mHeightMapSrvHeapIndex = -1;
	int mHeightMapUavHeapIndex = -1;

	bool mTerrainNeedsUpdate = false;
	UINT mHeightMapWidth = 512;
	UINT mHeightMapHeight = 512;


	ComPtr<ID3D12Resource> mHeightMapResource;

	ComPtr<ID3D12Resource> mHeightModificationTexture;    // Текстура изменений высоты (R32_FLOAT)
	ComPtr<ID3D12Resource> mHeightModificationTextureUpload;
	std::vector<float> mHeightModificationData;

	UINT mHeightModificationWidth =512;
	UINT mHeightModificationHeight = 512;

	bool mHeightModificationDirty = false;
	D3D12_CPU_DESCRIPTOR_HANDLE mHeightModificationSrvHandle;
	int mHeightModificationSrvIndex = -1;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> mTerrainUpdatePSO;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> mTerrainUpdateRootSignature;


};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,
    PSTR cmdLine, int showCmd)
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    try
    {
        TexColumnsApp theApp(hInstance);
        if(!theApp.Initialize())
            return 0;

        return theApp.Run();
    }
    catch(DxException& e)
    {
        MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
        return 0;
    }
}

TexColumnsApp::TexColumnsApp(HINSTANCE hInstance)
    : D3DApp(hInstance)
{
}

TexColumnsApp::~TexColumnsApp()
{
    if(md3dDevice != nullptr)
        FlushCommandQueue();
}
void TexColumnsApp::MoveBackFwd(float step) {
	XMFLOAT3 newPos;
	XMVECTOR fwd = cam.GetLook();
	XMStoreFloat3(&newPos, cam.GetPosition() + fwd * step);
	cam.SetPosition(newPos);
	cam.UpdateViewMatrix();
}
void TexColumnsApp::MoveLeftRight(float step) {
	XMFLOAT3 newPos;
	XMVECTOR right = cam.GetRight();
	XMStoreFloat3(&newPos, cam.GetPosition() + right * step);
	cam.SetPosition(newPos);
	cam.UpdateViewMatrix();
}
void TexColumnsApp::MoveUpDown(float step) {
	XMFLOAT3 newPos;
	XMVECTOR up = cam.GetUp();
	XMStoreFloat3(&newPos, cam.GetPosition() + up * step);
	cam.SetPosition(newPos);
	cam.UpdateViewMatrix();
}

bool TexColumnsApp::Initialize()
{
	// Создаем консольное окно.
	AllocConsole();

	// Перенаправляем стандартные потоки.
	 freopen("CONIN$", "r", stdin);
	freopen("CONOUT$", "w", stdout);
	freopen("CONOUT$", "w", stderr);

	cam.SetPosition(0, 3, 10);
	cam.RotateY(MathHelper::Pi);
    if(!D3DApp::Initialize())
        return false;

    // Reset the command list to prep for initialization commands.
    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

    // Get the increment size of a descriptor in this heap type.  This is hardware specific, 
	// so we have to query this information.
    mCbvSrvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

 
	LoadAllTextures();
	InitializeHeightModificationTexture();
	//mHeightMapResource = mTextures["textures/terr_height"]->Resource;
	/*auto it = mTextures.find("terr_height");
	assert(it != mTextures.end());
	assert(it->second->Resource != nullptr);

	mHeightMapResource = it->second->Resource;*/
	//BuildMaterials();
    BuildRootSignature();
    BuildLightingRootSignature();
	BuildShadowPassRootSignature();
	// TERRAIN HERE
	BuildTerrainRootSignature();
	BuildTerrainComputeRootSignature();
	BuildLights();
	BuildShadowMapViews();
	BuildDescriptorHeaps();
	//TERRAIN STUFF
	mTerrain = std::make_unique<Terrain>();
	mTerrain->InitializeTerrain(md3dDevice.Get(), TexOffsets["textures/terr_height"], 512, 6);
	BuildTerrainGeometry();

	//InitializeHeightModificationTexture();
    BuildShapeGeometry();
	SetLightShapes();
    BuildShadersAndInputLayout();
	BuildMaterials();
    BuildPSOs();
    BuildRenderItems();
    BuildFrameResources();

	D3D12_DESCRIPTOR_HEAP_DESC imGuiHeapDesc = {};
	imGuiHeapDesc.NumDescriptors = 1;
	imGuiHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	imGuiHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	imGuiHeapDesc.NodeMask = 0; // Or the appropriate node mask if you have multiple GPUs
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&imGuiHeapDesc, IID_PPV_ARGS(&m_ImGuiSrvDescriptorHeap)));

	// INITIALIZE IMGUI ////////////////////
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	////////////////////////////////////////
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

	ImGui_ImplDX12_InitInfo init_info = {};
	init_info.Device = md3dDevice.Get();
	init_info.CommandQueue = mCommandQueue.Get();
	init_info.NumFramesInFlight = gNumFrameResources;
	init_info.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM; // Or your render target format.
	init_info.DSVFormat = DXGI_FORMAT_UNKNOWN;
	init_info.SrvDescriptorHeap = m_ImGuiSrvDescriptorHeap.Get();
	init_info.LegacySingleSrvCpuDescriptor = m_ImGuiSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	init_info.LegacySingleSrvGpuDescriptor = m_ImGuiSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
	ImGui_ImplWin32_Init(mhMainWnd);
	ImGui_ImplDX12_Init(&init_info);
    // Execute the initialization commands.
    ThrowIfFailed(mCommandList->Close());
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // Wait until initialization is complete.
    FlushCommandQueue();
    return true;
}
 
void TexColumnsApp::OnResize()
{
    D3DApp::OnResize();
	CreateGBuffer();
	BuildDescriptorHeaps();
    // The window resized, so update the aspect ratio and recompute the projection matrix.
    XMMATRIX P = XMMatrixPerspectiveFovLH(0.4*MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
    XMStoreFloat4x4(&mProj, P);


}

void TexColumnsApp::Update(const GameTimer& gt)
{

	for (int idx : mShadowCastingLights)
	{
		auto& l = mLights[idx];
		std::wostringstream outs;
		outs << L"Shadow-casting light: " << idx
			<< L" heap index: " << l.ShadowMapSrvHeapIndex << L"\n";
		OutputDebugString(outs.str().c_str());
	}


	imguiID = 0;
	// Cycle through the circular frame resource array.
	mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
	mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

	// Has the GPU finished processing the commands of the current frame resource?
	// If not, wait until the GPU has completed commands up to this fence point.
	if (mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}
	UpdateCamera(gt);
	UpdateTerrain(gt);
	// === ImGui Setup ===
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	ImGui::Begin("Settings");
	ImGui::Text("Terrain Brush");

	// Параметры кисти
	ImGui::SliderFloat("Brush Radius", &mBrushRadius, 0.01f, 0.5f);
	ImGui::SliderFloat("Brush Strength", &mBrushStrength, 0.01f, 1.0f);

	ImGui::Text("\n\nLights\n\n");
	AnimateMaterials(gt);
	UpdateObjectCBs(gt);
	UpdateMaterialCBs(gt);
	UpdateLightCBs(gt);

	if (mBrushActive)
	{
		XMFLOAT2 uv;
		XMFLOAT3 hit;
		if (RaycastToTerrainUV(mLastMousePos.x, mLastMousePos.y, uv, hit))
			mBrushHitUV = uv;
	}

	UpdateTerrainCBs(gt);
	UpdateMainPassCB(gt);
	ImGui::End();
}


bool TexColumnsApp::RaycastToTerrainUV(int sx, int sy, XMFLOAT2& uvOut, XMFLOAT3& hitWorld)
{
	// Матрицы камеры
	XMMATRIX view = cam.GetView();
	XMMATRIX proj = cam.GetProj();
	XMMATRIX invView = XMMatrixInverse(nullptr, view);
	XMMATRIX invProj = XMMatrixInverse(nullptr, proj);


	float px = 2.0f * sx / mClientWidth - 1.0f;
	float py = -2.0f * sy / mClientHeight + 1.0f;

	XMVECTOR rayClip = XMVectorSet(px, py, 1.0f, 1.0f);


	XMVECTOR rayView = XMVector3TransformCoord(rayClip, invProj);
	rayView = XMVectorSetW(rayView, 0.0f);


	XMVECTOR rayWorld = XMVector3Normalize(XMVector3TransformNormal(rayView, invView));
	XMVECTOR camPos = cam.GetPosition();

	// Плоскость террейна: y = 0
	float camY = XMVectorGetY(camPos);
	float rayY = XMVectorGetY(rayWorld);

	// Если луч параллелен земле — пересечения нет
	if (fabs(rayY) < 1e-6f)
		return false;

	// t — точка пересечения луча с y=0
	float t = -camY / rayY;
	if (t < 0.0f)
		return false; // земля позади камеры

	// hit point
	XMVECTOR p = camPos + rayWorld * t;
	XMStoreFloat3(&hitWorld, p);

	// проверка внутри террейна
	if (hitWorld.x < 0 || hitWorld.z < 0 ||
		hitWorld.x > mTerrain->GetWorldSize() ||
		hitWorld.z > mTerrain->GetWorldSize())
		return false;

	// UV координаты
	uvOut.x = hitWorld.x / mTerrain->GetWorldSize();
	uvOut.y = hitWorld.z / mTerrain->GetWorldSize();

	return true;
}



void TexColumnsApp::OnMouseDown(WPARAM btnState, int x, int y)
{
    //mLastMousePos.x = x;
    //mLastMousePos.y = y;

    //SetCapture(mhMainWnd);

	SetCapture(mhMainWnd);

	if (ImGui::GetIO().WantCaptureMouse)
		return;

	XMFLOAT2 uv;
	XMFLOAT3 hit;
	bool hitOK = RaycastToTerrainUV(x, y, uv, hit);

	if (RaycastToTerrainUV(x, y, uv, hit))
	{
		mBrushActive = true;
		mBrushHitUV = uv;
		if (RaycastToTerrainUV(x, y, uv, hit))
		{
			// Применяем кисть с сохранением
			bool raise = ((btnState & MK_LBUTTON) != 0);
			ApplyBrushWithPersistence(uv, raise);

			// Также обновляем для отображения в реальном времени
			mBrushActive = true;
			mBrushHitUV = uv;

			//std::cout << "Mouse painting at UV: (" << uv.x << ", " << uv.y
			//	<< "), World: (" << hit.x << ", " << hit.y << ", " << hit.z << ")" << std::endl;
		}
	}

	if (!hitOK)
	{
		std::cout << "No terrain hit\n";
		return;
	}

	if (btnState & MK_LBUTTON)
		std::cout << "LMB terrain UV = " << uv.x << " " << uv.y
		<< " world: " << hit.x << " " << hit.y << " " << hit.z << "\n";

	if (btnState & MK_RBUTTON)
		std::cout << "RMB terrain UV = " << uv.x << " " << uv.y
		<< " world: " << hit.x << " " << hit.y << " " << hit.z << "\n";
}

void TexColumnsApp::OnMouseUp(WPARAM btnState, int x, int y)
{
	mBrushActive = false;
    ReleaseCapture();
}

void TexColumnsApp::OnMouseMove(WPARAM btnState, int x, int y)
{
	if (!ImGui::GetIO().WantCaptureMouse)
	{
		if ((btnState & MK_LBUTTON) != 0)
		{
			// Make each pixel correspond to a quarter of a degree.
			float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
			float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));

			// Update angles based on input to orbit camera around box.

			cam.YawPitch(dx, -dy);

		}

		if ((btnState & MK_LBUTTON) != 0 || (btnState & MK_RBUTTON) != 0)
		{
			XMFLOAT2 uv;
			XMFLOAT3 hit;
			if (RaycastToTerrainUV(x, y, uv, hit))
			{
				// Применяем кисть с сохранением
				bool raise = ((btnState & MK_LBUTTON) != 0);
				ApplyBrushWithPersistence(uv, raise);

				// Также обновляем для отображения в реальном времени
				mBrushActive = true;
				mBrushHitUV = uv;
			}
		}

		mLastMousePos.x = x;
		mLastMousePos.y = y;
	}
}



 
void TexColumnsApp::OnKeyPressed(const GameTimer& gt, WPARAM key)
{
	if (GET_WHEEL_DELTA_WPARAM(key) > 0 && !ImGui::GetIO().WantCaptureMouse)
	{
		cam.IncreaseSpeed(0.05);
	}
	else if (GET_WHEEL_DELTA_WPARAM(key) < 0 && !ImGui::GetIO().WantCaptureMouse)
	{
		cam.IncreaseSpeed(-0.05);
	}
	switch (key)
	{
	case 'A':
		MoveLeftRight(-cam.GetSpeed());
		return;
	case 'W':
		MoveBackFwd(cam.GetSpeed());
		return;
	case 'S':
		MoveBackFwd(-cam.GetSpeed());
		return;
	case 'D':
		MoveLeftRight(cam.GetSpeed());
		return;
	case 'Q':
		MoveUpDown(-cam.GetSpeed());
		return;
	case 'E':
		MoveUpDown(cam.GetSpeed());
		return;
	case VK_SHIFT:
		cam.SpeedUp();
		return;
	}
	
}

void TexColumnsApp::OnKeyReleased(const GameTimer& gt, WPARAM key)
{
	
	switch (key)
	{
	case VK_SHIFT:
		cam.SpeedDown();
		return;
	}
}

std::wstring TexColumnsApp::GetCamSpeed()
{
	return std::to_wstring(cam.GetSpeed());
}
 
void TexColumnsApp::UpdateCamera(const GameTimer& gt)
{
	// Convert Spherical to Cartesian coordinates.
	float x = mRadius * sinf(mPhi) * cosf(mTheta);
	float z = mRadius * sinf(mPhi) * sinf(mTheta);
	float y = mRadius * cosf(mPhi);

	// Build the view matrix.
	XMVECTOR pos = XMVectorSet(x, y, z, 1.0f);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMVECTOR campos = cam.GetPosition();
	pos = XMVectorSet(campos.m128_f32[0], campos.m128_f32[1], campos.m128_f32[2], 0.0f);
	target = cam.GetLook();
	up = cam.GetUp();
	
	XMMATRIX view = XMMatrixLookToLH(pos, target, up);
	XMStoreFloat4x4(&mView, view);
}




void TexColumnsApp::AnimateMaterials(const GameTimer& gt)
{
	
}

void TexColumnsApp::UpdateObjectCBs(const GameTimer& gt)
{
	auto currObjectCB = mCurrFrameResource->ObjectCB.get();
	for(auto& e : mAllRitems)
	{

		// Only update the cbuffer data if the constants have changed.  
		// This needs to be tracked per frame resource.
		if(e->NumFramesDirty > 0)
		{
			XMMATRIX world = XMLoadFloat4x4(&e->World);
			XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);

			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
			XMStoreFloat4x4(&objConstants.InvWorld,MathHelper::InverseTranspose(world));
			XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));

			currObjectCB->CopyData(e->ObjCBIndex, objConstants);

			// Next FrameResource need to be updated too.
			e->NumFramesDirty--;
		}
	}
}

void TexColumnsApp::UpdateLightCBs(const GameTimer& gt)
{
	ImGui::Checkbox("Rotating/Flashy lights", &f);

	auto currLightCB = mCurrFrameResource->LightCB.get();
	auto currShadowCB = mCurrFrameResource->PassShadowCB.get();
	int lId = 0;
	for (auto& l : mLights)
	{
		LightConstants lConst;
		PassShadowConstants shConst;

		
		if (l.type == 0)
		{
			//l.Color = mLights[0].Color; // ambient light equals directional;
			std::string s = "\Ambient Light " + std::to_string(lId);
			ImGui::PushID(++imguiID);
			ImGui::Text(s.c_str());
			ImGui::DragFloat("Strength", (float*)&l.Strength,0.02f);
			ImGui::PopID();
			
		}
		else if (l.type == 1)
		{
			std::string s = "\nPoint Light " + std::to_string(lId);
			ImGui::PushID(++imguiID);
			ImGui::Text(s.c_str());
			float* a[] = { &l.Position.x,&l.Position.y,&l.Position.z };
			XMStoreFloat4x4(&l.gWorld, XMMatrixTranspose(XMMatrixScaling(l.FalloffEnd * 2, l.FalloffEnd * 2, l.FalloffEnd * 2) * XMMatrixTranslation(l.Position.x, l.Position.y, l.Position.z)));
			
			ImGui::DragFloat3("Position", *a, 0.1f, -100,100);
			
			ImGui::ColorEdit3("Color", (float*)&l.Color);
			
			ImGui::DragFloat("Strength", &l.Strength,0.1f,0,100);
			
			ImGui::DragFloat("FaloffStart", &l.FalloffStart,0.1f, 1, l.FalloffEnd);
			
			ImGui::DragFloat("FaloffEnd", &l.FalloffEnd, 0.1f, l.FalloffStart, 100);
			
			bool b = l.isDebugOn;
			ImGui::Checkbox("is Debug On", &b);
			l.isDebugOn = b;
			
			ImGui::PopID();
		
			//l.Position.z = sin(gt.TotalTime()*3)*6;
		}
		else if (l.type == 2)
		{

			//l.Direction = { 0.5f*cos(gt.TotalTime()), 1.0f, 0.5f*sin(gt.TotalTime()) };
			std::string s = "\nDirectional Light " + std::to_string(lId);
			ImGui::PushID(++imguiID);
			ImGui::Text(s.c_str());
			ImGui::SliderFloat3("Direction", (float*)&l.Direction, -1, 1);

			ImGui::ColorEdit3("Color", (float*)&l.Color);

			ImGui::DragFloat("Strength", &l.Strength, 0.1f, 0, 100);

			if (f) { l.Direction = { 0.5f * cos(gt.TotalTime()), -1.0f, 0.5f * sin(gt.TotalTime()) }; }

			bool b = l.CastsShadows;
			ImGui::Checkbox("Cast Shadows", &b);
			l.CastsShadows = b;

			bool c = l.enablePCF;
			ImGui::Checkbox("Enable PCF", &c);
			l.enablePCF = c;

			ImGui::DragInt("PCF level", &l.pcf_level, 1, 0, 100);

			ImGui::PopID();
			
		}
		else if (l.type == 3)
		{
			
			std::string s = "\nSpot Light " + std::to_string(lId);
			ImGui::PushID(++imguiID);
			ImGui::Text(s.c_str());
			float* a[] = { &l.Position.x,&l.Position.y,&l.Position.z };
			ImGui::DragFloat3("Position", (float*)&l.Position, 0.1f, -100, 100);

			ImGui::DragFloat3("Rotation", (float*)&l.Rotation, 0.1f, -180, 180);
			XMStoreFloat4x4(&l.gWorld, XMMatrixTranspose(XMMatrixScaling(l.FalloffEnd*4/3, l.FalloffEnd,l.FalloffEnd*4/3) * XMMatrixTranslation(0, -l.FalloffEnd/2, 0) *
				XMMatrixRotationRollPitchYaw(XMConvertToRadians(l.Rotation.x), XMConvertToRadians(l.Rotation.y), XMConvertToRadians(l.Rotation.z)) *
				XMMatrixTranslation(l.Position.x, l.Position.y, l.Position.z)));
			XMFLOAT3 d(0, -1, 0);
			XMVECTOR v = XMLoadFloat3(&d);
			
			v = XMVector3TransformNormal(v, XMMatrixRotationRollPitchYaw(XMConvertToRadians(l.Rotation.x),XMConvertToRadians(l.Rotation.y),XMConvertToRadians(l.Rotation.z)));
			
			XMStoreFloat3(&l.Direction, v);
			d = XMFLOAT3(-1, 0, 0);
			v = XMLoadFloat3(&d);
			v = XMVector3TransformNormal(v, XMMatrixRotationRollPitchYaw(XMConvertToRadians(l.Rotation.x), XMConvertToRadians(l.Rotation.y), XMConvertToRadians(l.Rotation.z)));
			l.LightUp = v;

			ImGui::ColorEdit3("Color", (float*)&l.Color);

			ImGui::DragFloat("Strength", &l.Strength, 0.1f, 0, 100);

			ImGui::DragFloat("Faloff Start", &l.FalloffStart, 0.1f, 0,100);
	
			ImGui::DragFloat("Faloff End", &l.FalloffEnd,0.1f, 0, 100);
		
			ImGui::SliderFloat("Spot Power", &l.SpotPower, 0, 10);
			
			ImGui::DragInt("PCF level", &l.pcf_level, 1, 0, 100);

			if (f) { l.Position = { abs(10.0f * cos(gt.TotalTime())), 3.0f, 10.0f * sin(gt.TotalTime()) }; };

			ImGui::PopID();
			

		}
		if (l.type == 2 && l.CastsShadows || l.type == 3 && l.CastsShadows) // Directional Light
		{
			// Create an orthographic projection for the directional light.
			// The volume needs to encompass the scene or relevant parts.
			// This is a simplified approach; Cascaded Shadow Maps (CSM) are better for large scenes.
			XMFLOAT3 Pos(l.Position);
			XMVECTOR lightPos = XMLoadFloat3(&Pos);
			XMVECTOR lightDir = XMLoadFloat3(&l.Direction); 
			XMVECTOR targetPos = lightPos + lightDir; // Look at origin or scene center
			XMVECTOR lightUp = l.LightUp;

			XMMATRIX lightView = XMMatrixLookAtLH(lightPos, targetPos, lightUp);
			XMStoreFloat4x4(&l.LightView, lightView);

			// Define the orthographic projection volume
			// These values depend heavily on your scene size.
			float viewWidth = 1024.0f; // Adjust to fit your scene
			float viewHeight = 1024.0f;
			float nearZ = 1.0f;
			float farZ = 10000.0f; // Adjust
			XMMATRIX lightProj = XMMatrixIdentity();
			if (l.type == 2)
				lightProj = XMMatrixOrthographicLH(viewWidth, viewHeight, nearZ, farZ);
			else 
				lightProj = XMMatrixPerspectiveFovLH(0.5f * MathHelper::Pi, 1.0f, 1.0f, 1000.0f);
			XMStoreFloat4x4(&l.LightProj, lightProj);
			XMStoreFloat4x4(&l.LightViewProj, XMMatrixTranspose(XMMatrixMultiply(lightView,lightProj)));
		}
		lConst.light = l;
		shConst.LightViewProj = l.LightViewProj;
		currShadowCB->CopyData(l.LightCBIndex,shConst);
		currLightCB->CopyData(l.LightCBIndex, lConst);
		lId++;
	}
}

void TexColumnsApp::UpdateMaterialCBs(const GameTimer& gt)
{
	auto currMaterialCB = mCurrFrameResource->MaterialCB.get();
	for(auto& e : mMaterials)
	{
		
		// Only update the cbuffer data if the constants have changed.  If the cbuffer
		// data changes, it needs to be updated for each FrameResource.
		Material* mat = e.second.get();
		if(mat->NumFramesDirty > 0)
		{
			XMMATRIX matTransform = XMLoadFloat4x4(&mat->MatTransform);

			MaterialConstants matConstants;
			matConstants.DiffuseAlbedo = mat->DiffuseAlbedo;
			matConstants.FresnelR0 = mat->FresnelR0;
			matConstants.Roughness = mat->Roughness;
			XMStoreFloat4x4(&matConstants.MatTransform, XMMatrixTranspose(matTransform));

			currMaterialCB->CopyData(mat->MatCBIndex, matConstants);

			// Next FrameResource need to be updated too.
			mat->NumFramesDirty--;
		}
	}
}

void TexColumnsApp::UpdateMainPassCB(const GameTimer& gt)
{
	XMMATRIX view = XMLoadFloat4x4(&mView);
	XMMATRIX proj = XMLoadFloat4x4(&mProj);

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
	mMainPassCB.EyePosW = mEyePos;
	mMainPassCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
	mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
	//mMainPassCB.AmbientLight = { 0.0f, 1.0f, 0.0f, 1.0f };
	mMainPassCB.NearZ = 1.0f;
	mMainPassCB.FarZ = 20000.0f;
	mMainPassCB.TotalTime = gt.TotalTime();
	mMainPassCB.DeltaTime = gt.DeltaTime();
	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, mMainPassCB);
}


void  TexColumnsApp::UpdateTerrainCBs(const GameTimer& gt)
{
	auto currTileCB = mCurrFrameResource->TerrainCB.get();

	for (auto& t : mTerrain->GetAllTiles())
	{
		TerrainConstants tConstants;
		tConstants.TilePosition = t->worldPos;
		tConstants.TileSize = t->tileSize;
		tConstants.mapSize = mTerrain->mWorldSize;
		tConstants.hScale = heightScale;

		if (mBrushActive)
		{
			tConstants.HitUV = mBrushHitUV;
			tConstants.BrushRadius = mBrushRadius;


			bool leftDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
			bool rightDown = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;

			if (leftDown)
				tConstants.BrushActive = 5.0f;      // Поднимаем
			else if (rightDown)
				tConstants.BrushActive = -5.0f;     // Опускаем
			else
				tConstants.BrushActive = 0.0f;      // Кисть неактивна


		}

		else
		{
			// Кисть неактивна
			tConstants.HitUV = XMFLOAT2(-1, -1);
			tConstants.BrushRadius = 0.0f;
			tConstants.BrushActive = 0.0f;
		}

		tConstants.BrushRadius = mBrushRadius;
		//tConstants.BrushActive = mBrushActive ? 1.0f : 0.0f;


		currTileCB->CopyData(t->tileIndex, tConstants);

		t->NumFramesDirty--;
	}
	//std::cout << "Size of TerrainConstants in C++: " << sizeof(TerrainConstants) << " bytes\n";

}

void TexColumnsApp::CreateGBuffer()
{
	// Форматы
	const DXGI_FORMAT positionFormat = DXGI_FORMAT_R32G32B32A32_FLOAT;
	const DXGI_FORMAT normalFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
	const DXGI_FORMAT albedoFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	FlushCommandQueue();

	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));
	// Описание ресурса
	D3D12_RESOURCE_DESC texDesc = {};
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Alignment = 0;
	texDesc.Width = mClientWidth;
	texDesc.Height = mClientHeight;
	texDesc.DepthOrArraySize = 1;
	texDesc.MipLevels = 1;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET ;

	mGBufferPosition.Reset();
	mGBufferNormal.Reset();
	mGBufferAlbedo.Reset();
	// Создание ресурсов --------------------------------------------------------
	// Position
	texDesc.Format = positionFormat;
	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		&CD3DX12_CLEAR_VALUE(positionFormat, Colors::Black),
		IID_PPV_ARGS(&mGBufferPosition)));

	// Normal
	texDesc.Format = normalFormat;
	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		&CD3DX12_CLEAR_VALUE(normalFormat, Colors::Black),
		IID_PPV_ARGS(&mGBufferNormal)));

	// Albedo
	texDesc.Format = albedoFormat;
	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		&CD3DX12_CLEAR_VALUE(albedoFormat, Colors::Black),
		IID_PPV_ARGS(&mGBufferAlbedo)));

	// Создание RTV -------------------------------------------------------------
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(
		mRtvHeap->GetCPUDescriptorHandleForHeapStart(),
		SwapChainBufferCount, // Начинаем после SwapChain
		mRtvDescriptorSize
	);

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Texture2D.MipSlice = 0;

	// Albedo
	rtvDesc.Format = albedoFormat;
	md3dDevice->CreateRenderTargetView(mGBufferAlbedo.Get(), &rtvDesc, rtvHandle);
	mGBufferRTVs[0] = rtvHandle;
	rtvHandle.Offset(1, mRtvDescriptorSize);

	// Normal
	rtvDesc.Format = normalFormat;
	md3dDevice->CreateRenderTargetView(mGBufferNormal.Get(), &rtvDesc, rtvHandle);
	mGBufferRTVs[1] = rtvHandle;
	rtvHandle.Offset(1, mRtvDescriptorSize);
	// Position
	rtvDesc.Format = positionFormat;
	md3dDevice->CreateRenderTargetView(mGBufferPosition.Get(), &rtvDesc, rtvHandle);
	mGBufferRTVs[2] = rtvHandle;


	// Execute the resize commands.
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until resize is complete.
	FlushCommandQueue();

}


void TexColumnsApp::LoadAllTextures()
{
	// MEGA COSTYL
	for (const auto& entry : std::filesystem::directory_iterator("../../Textures/textures"))
	{
		if (entry.is_regular_file() && entry.path().extension() == ".dds")
		{
			std::string filepath = entry.path().string();
			filepath = filepath.substr(24, filepath.size());
			filepath = filepath.substr(0, filepath.size()-4);
			filepath = "textures/" + filepath;
			LoadTexture(filepath);
		}
	}
}

void TexColumnsApp::LoadTexture(const std::string& name)
{
	auto tex = std::make_unique<Texture>();
	tex->Name = name;
	tex->Filename = L"../../Textures/" + std::wstring(name.begin(), name.end()) + L".dds";
	
	if (FAILED(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), tex->Filename.c_str(),
		tex->Resource, tex->UploadHeap))) std::cout << name << "\n";
	mTextures[name] = std::move(tex);
}

void TexColumnsApp::BuildRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE diffuseRange;
	diffuseRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); // Диффузная текстура в регистре t0

	CD3DX12_DESCRIPTOR_RANGE normalRange;
	normalRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);  // Нормальная карта в регистре t1

    // Root parameter can be a table, root descriptor or root constants.
    CD3DX12_ROOT_PARAMETER slotRootParameter[5];

	// Perfomance TIP: Order from most frequent to least frequent.
	slotRootParameter[0].InitAsDescriptorTable(1, &diffuseRange, D3D12_SHADER_VISIBILITY_ALL);
	slotRootParameter[1].InitAsDescriptorTable(1, &normalRange, D3D12_SHADER_VISIBILITY_ALL);

    slotRootParameter[2].InitAsConstantBufferView(0); // register b0
    slotRootParameter[3].InitAsConstantBufferView(1); // register b1
    slotRootParameter[4].InitAsConstantBufferView(2); // register b2

	auto staticSamplers = GetStaticSamplers();

    // A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(5, slotRootParameter,
		(UINT)staticSamplers.size(), staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    // create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;
    HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
        serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

    if(errorBlob != nullptr)
    {
        ::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    }
    ThrowIfFailed(hr);

    ThrowIfFailed(md3dDevice->CreateRootSignature(
		0,
        serializedRootSig->GetBufferPointer(),
        serializedRootSig->GetBufferSize(),
        IID_PPV_ARGS(mRootSignature.GetAddressOf())));
}
void TexColumnsApp::BuildTerrainRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE heightRange;
	heightRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); // Диффузная текстура в регистре t0

	CD3DX12_DESCRIPTOR_RANGE diffuseRange;
	diffuseRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1); // Диффузная текстура в регистре t0

	CD3DX12_DESCRIPTOR_RANGE normalRange;
	normalRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);  // Нормальная карта в регистре t1

	CD3DX12_DESCRIPTOR_RANGE heightModRange;
	heightModRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3); // t3 - gHeightModificationMap (НОВАЯ!)

	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER slotRootParameter[8];

	// Perfomance TIP: Order from most frequent to least frequent.
	slotRootParameter[0].InitAsDescriptorTable(1, &heightRange, D3D12_SHADER_VISIBILITY_ALL);
	slotRootParameter[1].InitAsDescriptorTable(1, &diffuseRange, D3D12_SHADER_VISIBILITY_ALL);
	slotRootParameter[2].InitAsDescriptorTable(1, &normalRange, D3D12_SHADER_VISIBILITY_ALL);
	slotRootParameter[3].InitAsDescriptorTable(1, &heightModRange, D3D12_SHADER_VISIBILITY_ALL);

	slotRootParameter[4].InitAsConstantBufferView(0); // register b0
	slotRootParameter[5].InitAsConstantBufferView(1); // register b1
	slotRootParameter[6].InitAsConstantBufferView(2); // register b2
	slotRootParameter[7].InitAsConstantBufferView(3); // register b3

	auto staticSamplers = GetStaticSamplers();

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(8, slotRootParameter,
		(UINT)staticSamplers.size(), staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr)
	{
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	ThrowIfFailed(md3dDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(mTerrainRootSignature.GetAddressOf())));
}


void TexColumnsApp::BuildTerrainComputeRootSignature()
{
	// UAV: RWTexture2D<float>
	CD3DX12_DESCRIPTOR_RANGE uavRange;
	uavRange.Init(
		D3D12_DESCRIPTOR_RANGE_TYPE_UAV,
		1,      // один UAV
		0       // register(u0)
	);

	CD3DX12_ROOT_PARAMETER slotRootParameter[2];

	// u0 — heightmap UAV
	slotRootParameter[0].InitAsDescriptorTable(
		1,
		&uavRange,
		D3D12_SHADER_VISIBILITY_ALL
	);

	// b3 — cbTerrainTile (тот же самый!)
	slotRootParameter[1].InitAsConstantBufferView(
		3 // register(b3)
	);

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
		_countof(slotRootParameter),
		slotRootParameter,
		0,
		nullptr,
		D3D12_ROOT_SIGNATURE_FLAG_NONE // compute — без IA
	);

	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;

	HRESULT hr = D3D12SerializeRootSignature(
		&rootSigDesc,
		D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(),
		errorBlob.GetAddressOf()
	);

	if (errorBlob != nullptr)
	{
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	ThrowIfFailed(md3dDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(mTerrainComputeRootSignature.GetAddressOf())
	));
}



// build lighting root signature 
void TexColumnsApp::BuildLightingRootSignature()
{


	CD3DX12_DESCRIPTOR_RANGE gPosition;
	gPosition.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); // t0
	CD3DX12_DESCRIPTOR_RANGE gNormal;
	gNormal.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1); // t1
	CD3DX12_DESCRIPTOR_RANGE gAlbedo;
	gAlbedo.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2); // t2
	CD3DX12_DESCRIPTOR_RANGE shadowMapRange;
	shadowMapRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3); // Shadow map at register t3

	CD3DX12_DESCRIPTOR_RANGE debugShadowRange;

	debugShadowRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 10); // например t10

	CD3DX12_ROOT_PARAMETER rootParams[8];
	rootParams[0].InitAsDescriptorTable(1, &gPosition, D3D12_SHADER_VISIBILITY_ALL);
	rootParams[1].InitAsDescriptorTable(1, &gNormal, D3D12_SHADER_VISIBILITY_ALL);
	rootParams[2].InitAsDescriptorTable(1, &gAlbedo, D3D12_SHADER_VISIBILITY_ALL);
	rootParams[3].InitAsConstantBufferView(0); // b0 
	rootParams[4].InitAsConstantBufferView(1); // b1
	rootParams[5].InitAsConstantBufferView(2); // b2
	rootParams[6].InitAsDescriptorTable(1, &shadowMapRange, D3D12_SHADER_VISIBILITY_PIXEL); // PIXEL visibility
	rootParams[7].InitAsDescriptorTable(1, &debugShadowRange, D3D12_SHADER_VISIBILITY_PIXEL);

	auto staticSamplers = GetStaticSamplers();

	CD3DX12_ROOT_SIGNATURE_DESC rsDesc;
	rsDesc.Init(_countof(rootParams), rootParams,
		(UINT)staticSamplers.size(), staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);


	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr)
	{
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	ThrowIfFailed(md3dDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(mLightingRootSignature.GetAddressOf())));
}

// shadow root signature 
void TexColumnsApp::BuildShadowPassRootSignature()
{
	CD3DX12_ROOT_PARAMETER slotRootParameter[2];

	slotRootParameter[0].InitAsConstantBufferView(0); // ObjectConstants (b0)
	slotRootParameter[1].InitAsConstantBufferView(1); // ShadowPassConstants (b1 - gLightViewProj)
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc;
	rootSigDesc.Init(
		_countof(slotRootParameter), slotRootParameter,
		0, nullptr, // No static samplers needed for basic shadow map generation
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr)
	{
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	ThrowIfFailed(md3dDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(&mShadowPassRootSignature)));
}


void TexColumnsApp::CreatePointLight(XMFLOAT3 pos, XMFLOAT3 color, float faloff_start, float faloff_end, float strength)
{
	Light light;
	light.LightCBIndex = mLights.size();

	light.Position = pos;
	light.Color = color;
	light.FalloffStart = faloff_start;
	light.FalloffEnd = faloff_end;
	light.type = 1;
	auto& world = XMMatrixScaling(faloff_end * 2, faloff_end * 2, faloff_end * 2) * XMMatrixTranslation(pos.x, pos.y, pos.z);
	light.enablePCF = true;
	XMStoreFloat4x4(&light.gWorld, XMMatrixTranspose(world));
	mLights.push_back(light);
}
void TexColumnsApp::CreateSpotLight(XMFLOAT3 pos, XMFLOAT3 rot, XMFLOAT3 color, float faloff_start, float faloff_end, float strength, float spotpower)
{
	Light light;
	light.LightCBIndex = mLights.size();

	light.Position = pos;
	light.Color = color;
	light.FalloffStart = faloff_start;
	light.FalloffEnd = faloff_end;
	light.Rotation = rot;
	light.LightUp = XMVectorSet(0, 1, 0, 0);
	light.type = 3;
	light.Strength = strength;
	light.SpotPower = spotpower;
	light.enablePCF = true;
	mLights.push_back(light);
}

void TexColumnsApp::BuildLights()
{
	// directional
	Light dir;
	dir.LightCBIndex = mLights.size();
	dir.Position = { 0,300,0 };
	dir.Direction = { 0, -1, 0 };
	dir.Color = { 1,1,1 };
	dir.Strength = 1.2;
	dir.type = 2;
	dir.LightUp = XMVectorSet(0, 0, -1, 0);
	auto& world = XMMatrixScaling(1000,1000,1000);
	XMStoreFloat4x4(&dir.gWorld, XMMatrixTranspose(world));
	dir.enablePCF = true;
	mLights.push_back(dir);

	Light ambient;
	ambient.LightCBIndex = mLights.size();
	ambient.Position = { 3.0f, 0.0f, 3.0f };
	ambient.Color = { 0,0,0 }; // need only x
	ambient.Strength = 0.2; // need only x
	ambient.type = 0;
	XMStoreFloat4x4(&ambient.gWorld, XMMatrixTranspose(XMMatrixTranslation(0, 0, 0) * XMMatrixScaling(1000, 1000, 1000)));
	mLights.push_back(ambient);

	CreateSpotLight({ 0,3,0 }, { 0,0,-90 }, { 1,1,1 }, 1, 30, 6, 1);
}

void TexColumnsApp::SetLightShapes()
{
	for (auto& light : mLights)
	{

		switch (light.type)
		{
		case 1:
			light.ShapeGeo = mGeometries["shapeGeo"]->DrawArgs["sphere"];
			break;
		case 3:
			light.ShapeGeo = mGeometries["shapeGeo"]->DrawArgs["box"];
			break;
		}
	}
	mLights;
}

void TexColumnsApp::CreateMaterial(std::string _name, int _CBIndex, int _SRVDiffIndex, int _SRVNMapIndex, XMFLOAT4 _DiffuseAlbedo, XMFLOAT3 _FresnelR0, float _Roughness)
{
	
	auto material = std::make_unique<Material>();
	material->Name = _name;
	material->MatCBIndex = static_cast<int>(mMaterials.size());
	material->DiffuseSrvHeapIndex = _SRVDiffIndex;
	material->NormalSrvHeapIndex = _SRVNMapIndex;
	material->DiffuseAlbedo = _DiffuseAlbedo;
	material->FresnelR0 = _FresnelR0;
	material->Roughness = _Roughness;
	mMaterials[_name] = std::move(material);
}

void TexColumnsApp::BuildShadowMapViews()
{
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
	dsvHeapDesc.NumDescriptors = 2; // For one shadow map
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&mShadowDsvHeap)));
	int i = 0;
	for (auto& light : mLights)
	{

		if (light.type == 2 || light.type == 3)
		{
			// Define shadow map properties (can be members of the class or taken from a specific light)
			mShadowViewport = { 0.0f, 0.0f, (float)SHADOW_MAP_WIDTH, (float)SHADOW_MAP_HEIGHT, 0.0f, 1.0f };
			mShadowScissorRect = { 0, 0, (int)SHADOW_MAP_WIDTH, (int)SHADOW_MAP_HEIGHT };

			// Create the shadow map texture
			D3D12_RESOURCE_DESC texDesc;
			ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
			texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			texDesc.Alignment = 0;
			texDesc.Width = SHADOW_MAP_WIDTH;
			texDesc.Height = SHADOW_MAP_HEIGHT;
			texDesc.DepthOrArraySize = 1;
			texDesc.MipLevels = 1;
			texDesc.Format = SHADOW_MAP_FORMAT;
			texDesc.SampleDesc.Count = 1;
			texDesc.SampleDesc.Quality = 0;
			texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
			texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

			D3D12_CLEAR_VALUE clearValue;
			clearValue.Format = SHADOW_MAP_DSV_FORMAT;
			clearValue.DepthStencil.Depth = 1.0f;
			clearValue.DepthStencil.Stencil = 0;

			ThrowIfFailed(md3dDevice->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&texDesc,
				D3D12_RESOURCE_STATE_GENERIC_READ, // Start in generic read, will transition to DEPTH_WRITE
				&clearValue,
				IID_PPV_ARGS(&light.ShadowMap)));
			// Create DSV for the shadow map.
			// We need a DSV heap. Let's create one specifically for shadow maps for clarity,
			// or you can extend your existing mDsvHeap if it's not solely for the main depth buffer.
			
			D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
			dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
			dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
			dsvDesc.Format = SHADOW_MAP_DSV_FORMAT;
			dsvDesc.Texture2D.MipSlice = 0;
			light.ShadowMapDsvHandle = mShadowDsvHeap->GetCPUDescriptorHandleForHeapStart();
			light.ShadowMapDsvHandle.Offset(i, mDsvDescriptorSize); // Use the stored index
			md3dDevice->CreateDepthStencilView(light.ShadowMap.Get(), &dsvDesc, light.ShadowMapDsvHandle);

			light.ShadowMapSrvHeapIndex = mTextures.size() + 3 + i;
			i++;
		}
	}
	mLights;

	


	
}

void TexColumnsApp::BuildDescriptorHeaps()
{
	//
	// Create the SRV heap.
	//
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = mTextures.size() + 3 + mLights.size() + 1 +1;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));


	// Создание SRV -------------------------------------------------------------

	//
	// Fill out the heap with actual descriptors.
	//
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	int offset = 0;
	for (const auto& tex : mTextures) {
		auto text = tex.second->Resource;
		DXGI_FORMAT format = text->GetDesc().Format;
		if (format == DXGI_FORMAT_UNKNOWN) {
			abort();
		}
		srvDesc.Format = text->GetDesc().Format;
		srvDesc.Texture2D.MipLevels = text->GetDesc().MipLevels;
		md3dDevice->CreateShaderResourceView(text.Get(), &srvDesc, hDescriptor);
		hDescriptor.Offset(1, mCbvSrvDescriptorSize);
		TexOffsets[tex.first] = offset;
		offset++;
	}
	srvDesc.Texture2D.MipLevels = 1;
	// Albedo SRV
	srvDesc.Format = albedoFormat;
	md3dDevice->CreateShaderResourceView(
		mGBufferAlbedo.Get(), &srvDesc, hDescriptor);
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	// Normal SRV
	srvDesc.Format = normalFormat;
	md3dDevice->CreateShaderResourceView(
		mGBufferNormal.Get(), &srvDesc, hDescriptor);
	//mGBufferSRVs[1] = CD3DX12_GPU_DESCRIPTOR_HANDLE(srvHandle);
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);
	// Position SRV
	srvDesc.Format = positionFormat;
	md3dDevice->CreateShaderResourceView(
		mGBufferPosition.Get(), &srvDesc, hDescriptor);
	for (auto& light : mLights)
	{
		if (light.type == 2 || light.type == 3)
		{
			srvDesc.Format = SHADOW_MAP_SRV_FORMAT; // Use the SRV-compatible format
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MipLevels = 1;
			srvDesc.Texture2D.MostDetailedMip = 0;
			srvDesc.Texture2D.PlaneSlice = 0;
			srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
			CD3DX12_CPU_DESCRIPTOR_HANDLE shadowMapSrvHandle(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
			shadowMapSrvHandle.Offset(light.ShadowMapSrvHeapIndex, mCbvSrvDescriptorSize); // Use the stored index

			md3dDevice->CreateShaderResourceView(light.ShadowMap.Get(), &srvDesc, shadowMapSrvHandle);
		}
	}

	mHeightModificationSrvIndex = mTextures.size() + 3 + mLights.size(); // После shadow maps
	CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
	srvHandle.Offset(mHeightModificationSrvIndex, mCbvSrvDescriptorSize);

	//D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;

	md3dDevice->CreateShaderResourceView(mHeightModificationTexture.Get(), &srvDesc, srvHandle);
	mHeightModificationSrvHandle = srvHandle;


	HRESULT hr = md3dDevice->GetDeviceRemovedReason();
	if (FAILED(hr))
	{
		std::cout << "Error creating SRV: " << std::hex << hr << std::endl;
	}
}

void TexColumnsApp::BuildShadersAndInputLayout()
{
	const D3D_SHADER_MACRO alphaTestDefines[] =
	{
		"ALPHA_TEST", "1",
		NULL, NULL
	};

	mShaders["standardVS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["opaquePS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", nullptr, "PS", "ps_5_1");
	mShaders["gbufferVS"] = d3dUtil::CompileShader(L"Shaders\\GeometryPass.hlsl", nullptr, "VS", "vs_5_0");
	mShaders["gbufferPS"] = d3dUtil::CompileShader(L"Shaders\\GeometryPass.hlsl", nullptr, "PS", "ps_5_0");
	mShaders["lightingVS"] = d3dUtil::CompileShader(L"Shaders\\LightingPass.hlsl", nullptr, "VS", "vs_5_0");
	mShaders["lightingQUADVS"] = d3dUtil::CompileShader(L"Shaders\\LightingPass.hlsl", nullptr, "VS_QUAD", "vs_5_0");
	mShaders["lightingPS"] = d3dUtil::CompileShader(L"Shaders\\LightingPass.hlsl", nullptr, "PS", "ps_5_0");
	mShaders["lightingPSDebug"] = d3dUtil::CompileShader(L"Shaders\\LightingPass.hlsl", nullptr, "PS_debug", "ps_5_0");
	mShaders["shadowVS"] = d3dUtil::CompileShader(L"Shaders\\ShadowMap.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["lightingQUADVS"] = d3dUtil::CompileShader(L"Shaders\\LightingPass.hlsl", nullptr, "VS_QUAD", "vs_5_1");
	mShaders["shadowDebugPS"] = d3dUtil::CompileShader(L"Shaders\\LightingPass.hlsl", nullptr, "PS_ShadowDebug", "ps_5_1");
	mShaders["terrainVS"] = d3dUtil::CompileShader(L"Shaders\\TerrainShader.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["terrainPS"] = d3dUtil::CompileShader(L"Shaders\\TerrainShader.hlsl", nullptr, "PS", "ps_5_0");


    mInputLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };
}
void TexColumnsApp::BuildCustomMeshGeometry(std::string name, UINT& meshVertexOffset, UINT& meshIndexOffset, UINT& prevVertSize, UINT& prevIndSize, std::vector<Vertex>& vertices, std::vector<std::uint16_t>& indices, MeshGeometry* Geo)
{
	std::vector<GeometryGenerator::MeshData> meshDatas; // Это твоя структура для хранения вершин и индексов

	// Создаем инстанс импортера.
	Assimp::Importer importer;

	// Читаем файл с постпроцессингом: триангуляция, флип UV (если нужно) и генерация нормалей.
	const aiScene* scene = importer.ReadFile("../../Common/" + name + ".obj",
		aiProcess_Triangulate |
		aiProcess_ConvertToLeftHanded |
		aiProcess_FlipUVs |
		aiProcess_GenNormals |
		aiProcess_CalcTangentSpace);
	if (!scene || !scene->mRootNode)
	{
		std::cerr << "Assimp error: " << importer.GetErrorString() << std::endl;
	}
	unsigned int nMeshes = scene->mNumMeshes;
	ObjectsMeshCount[name] = nMeshes;
	
	for (int i = 0;i < scene->mNumMeshes;i++)
	{
		GeometryGenerator::MeshData meshData;
		aiMesh* mesh = scene->mMeshes[i];

		// Подготовка контейнеров для вершин и индексов.
		std::vector<GeometryGenerator::Vertex> vertices;
		std::vector<std::uint16_t> indices;

		// Проходим по всем вершинам и копируем данные.
		for (unsigned int i = 0; i < mesh->mNumVertices; ++i)
		{
			GeometryGenerator::Vertex v;

			v.Position.x = mesh->mVertices[i].x;
			v.Position.y = mesh->mVertices[i].y;
			v.Position.z = mesh->mVertices[i].z;

			if (mesh->HasNormals())
			{
				v.Normal.x = mesh->mNormals[i].x;
				v.Normal.y = mesh->mNormals[i].y;
				v.Normal.z = mesh->mNormals[i].z;
			}

			if (mesh->HasTextureCoords(0))
			{
				v.TexC.x = mesh->mTextureCoords[0][i].x;
				v.TexC.y = mesh->mTextureCoords[0][i].y;
			}
			else
			{
				v.TexC = XMFLOAT2(0.0f, 0.0f);
			}
			if (mesh->HasTangentsAndBitangents())
			{
				v.TangentU.x = mesh->mTangents[i].x;
				v.TangentU.y = mesh->mTangents[i].y;
				v.TangentU.z = mesh->mTangents[i].z;

			}

			// Если необходимо, можно обработать тангенты и другие атрибуты.
			vertices.push_back(v);
		}
		// Проходим по всем граням для формирования индексов.
		for (unsigned int i = 0; i < mesh->mNumFaces; ++i)
		{
			aiFace face = mesh->mFaces[i];
			// Убедимся, что грань треугольная.
			if (face.mNumIndices != 3) continue;
			indices.push_back(static_cast<std::uint16_t>(face.mIndices[0]));
			indices.push_back(static_cast<std::uint16_t>(face.mIndices[1]));
			indices.push_back(static_cast<std::uint16_t>(face.mIndices[2]));
		}

		// Заполняем meshData. Здесь тебе нужно адаптировать под свою структуру:
		meshData.Vertices = vertices;
		meshData.Indices32.resize(indices.size());
		for (size_t j = 0; j < indices.size(); ++j)
			meshData.Indices32[j] = indices[j];

		aiMaterial* mat = scene->mMaterials[mesh->mMaterialIndex];
		aiString texturePath;

		aiString texPath;

		meshData.matName = scene->mMaterials[mesh->mMaterialIndex]->GetName().C_Str();
		// Если требуется, можно выполнить дополнительные операции, например, нормализацию, вычисление тангенсов и т.д.
		meshDatas.push_back(meshData);
	}
	for (int k = 0;k < scene->mNumMaterials;k++)
	{
		aiString texPath;
		scene->mMaterials[k]->GetTexture(aiTextureType_DIFFUSE, 0, &texPath);
		std::string a = std::string(texPath.C_Str());
		a = a.substr(0, a.length() - 4);
		std::cout << "DIFFUSE: " << a << "\n";
		scene->mMaterials[k]->GetTexture(aiTextureType_DISPLACEMENT, 0, &texPath);
		std::string b = std::string(texPath.C_Str());
		b = b.substr(0, b.length() - 4);
		std::cout << "NORMAL: " << b << "\n";

		CreateMaterial(scene->mMaterials[k]->GetName().C_Str(), k, TexOffsets[a], TexOffsets[b], XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT3(0.05f, 0.05f, 0.05f), 0.3f);
	}

	UINT totalMeshSize = 0;
	UINT k = vertices.size();
	std::vector<std::pair<GeometryGenerator::MeshData,SubmeshGeometry>>meshSubmeshes;
	for (auto mesh : meshDatas)
	{
		meshVertexOffset = meshVertexOffset + prevVertSize;
		prevVertSize = mesh.Vertices.size();
		totalMeshSize += mesh.Vertices.size();

		meshIndexOffset = meshIndexOffset + prevIndSize;
		prevIndSize = mesh.Indices32.size();
		SubmeshGeometry meshSubmesh;
		meshSubmesh.IndexCount = (UINT)mesh.Indices32.size();
		meshSubmesh.StartIndexLocation = meshIndexOffset;
		meshSubmesh.BaseVertexLocation = meshVertexOffset;
		GeometryGenerator::MeshData m = mesh;
		meshSubmeshes.push_back(std::make_pair(m,meshSubmesh));
	}
	/////////
	/////
	for (auto mesh : meshDatas)
	{
		for (size_t i = 0; i < mesh.Vertices.size(); ++i, ++k)
		{
			vertices.push_back(Vertex(mesh.Vertices[i].Position, mesh.Vertices[i].Normal, mesh.Vertices[i].TexC,mesh.Vertices[i].TangentU));
		}
	}
	////////

	///////
	for (auto mesh : meshDatas)
	{
		indices.insert(indices.end(), std::begin(mesh.GetIndices16()), std::end(mesh.GetIndices16()));
	}
	///////
	Geo->MultiDrawArgs[name] = meshSubmeshes;
}
void TexColumnsApp::BuildShapeGeometry()
{
    GeometryGenerator geoGen;
	GeometryGenerator::MeshData box = geoGen.CreateBox(1.0f, 1.0f, 1.0f, 0);
	GeometryGenerator::MeshData grid = geoGen.CreateGrid(20.0f, 30.0f, 60, 40);
	GeometryGenerator::MeshData sphere = geoGen.CreateSphere(0.5f, 15, 15);
	GeometryGenerator::MeshData cylinder = geoGen.CreateCylinder(0.25f, 0.00f, 1.0f, 20, 20);

	//
	// We are concatenating all the geometry into one big vertex/index buffer.  So
	// define the regions in the buffer each submesh covers.
	//

	// Cache the vertex offsets to each object in the concatenated vertex buffer.
	UINT boxVertexOffset = 0;
	UINT gridVertexOffset = (UINT)box.Vertices.size();
	UINT sphereVertexOffset = gridVertexOffset + (UINT)grid.Vertices.size();
	UINT cylinderVertexOffset = sphereVertexOffset + (UINT)sphere.Vertices.size();

	// Cache the starting index for each object in the concatenated index buffer.
	UINT boxIndexOffset = 0;
	UINT gridIndexOffset = (UINT)box.Indices32.size();
	UINT sphereIndexOffset = gridIndexOffset + (UINT)grid.Indices32.size();
	UINT cylinderIndexOffset = sphereIndexOffset + (UINT)sphere.Indices32.size();
	SubmeshGeometry boxSubmesh;
	boxSubmesh.IndexCount = (UINT)box.Indices32.size();
	boxSubmesh.StartIndexLocation = boxIndexOffset;
	boxSubmesh.BaseVertexLocation = boxVertexOffset;

	SubmeshGeometry gridSubmesh;
	gridSubmesh.IndexCount = (UINT)grid.Indices32.size();
	gridSubmesh.StartIndexLocation = gridIndexOffset;
	gridSubmesh.BaseVertexLocation = gridVertexOffset;

	SubmeshGeometry sphereSubmesh;
	sphereSubmesh.IndexCount = (UINT)sphere.Indices32.size();
	sphereSubmesh.StartIndexLocation = sphereIndexOffset;
	sphereSubmesh.BaseVertexLocation = sphereVertexOffset;

	SubmeshGeometry cylinderSubmesh;
	cylinderSubmesh.IndexCount = (UINT)cylinder.Indices32.size();
	cylinderSubmesh.StartIndexLocation = cylinderIndexOffset;
	cylinderSubmesh.BaseVertexLocation = cylinderVertexOffset;

	//
	// Extract the vertex elements we are interested in and pack the
	// vertices of all the meshes into one vertex buffer.
	//
	
	auto totalVertexCount =
		box.Vertices.size() +
		grid.Vertices.size() +
		sphere.Vertices.size() +
		cylinder.Vertices.size();

	
	std::vector<Vertex> vertices(totalVertexCount);

	UINT k = 0;
	for(size_t i = 0; i < box.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = box.Vertices[i].Position;
		vertices[k].Normal = box.Vertices[i].Normal;
		vertices[k].TexC = box.Vertices[i].TexC;
	}

	for(size_t i = 0; i < grid.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = grid.Vertices[i].Position;
		vertices[k].Normal = grid.Vertices[i].Normal;
		vertices[k].TexC = grid.Vertices[i].TexC;
	}

	for(size_t i = 0; i < sphere.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = sphere.Vertices[i].Position;
		vertices[k].Normal = sphere.Vertices[i].Normal;
		vertices[k].TexC = sphere.Vertices[i].TexC;
	}

	for(size_t i = 0; i < cylinder.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = cylinder.Vertices[i].Position;
		vertices[k].Normal = cylinder.Vertices[i].Normal;
		vertices[k].TexC = cylinder.Vertices[i].TexC;
	}
	
	std::vector<std::uint16_t> indices;
	indices.insert(indices.end(), std::begin(box.GetIndices16()), std::end(box.GetIndices16()));
	indices.insert(indices.end(), std::begin(grid.GetIndices16()), std::end(grid.GetIndices16()));
	indices.insert(indices.end(), std::begin(sphere.GetIndices16()), std::end(sphere.GetIndices16()));
	indices.insert(indices.end(), std::begin(cylinder.GetIndices16()), std::end(cylinder.GetIndices16()));
	
	
	
	UINT meshVertexOffset = cylinderVertexOffset;
	UINT meshIndexOffset = cylinderIndexOffset;
	UINT prevIndSize = (UINT)cylinder.Indices32.size();
	UINT prevVertSize = (UINT)cylinder.Vertices.size();

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "shapeGeo";
	BuildCustomMeshGeometry("sponza", meshVertexOffset, meshIndexOffset, prevVertSize, prevIndSize, vertices, indices, geo.get());
	BuildCustomMeshGeometry("negr", meshVertexOffset, meshIndexOffset, prevVertSize, prevIndSize, vertices, indices, geo.get());
	BuildCustomMeshGeometry("left", meshVertexOffset, meshIndexOffset, prevVertSize, prevIndSize, vertices, indices, geo.get());
	BuildCustomMeshGeometry("right", meshVertexOffset, meshIndexOffset, prevVertSize, prevIndSize, vertices, indices, geo.get());
	BuildCustomMeshGeometry("plane2", meshVertexOffset, meshIndexOffset, prevVertSize, prevIndSize, vertices, indices, geo.get());
	



	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);




	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	geo->DrawArgs["box"] = boxSubmesh;
	geo->DrawArgs["grid"] = gridSubmesh;
	geo->DrawArgs["sphere"] = sphereSubmesh;
	geo->DrawArgs["cylinder"] = cylinderSubmesh;

	mGeometries[geo->Name] = std::move(geo);
}

void TexColumnsApp::BuildPSOs()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;

	//
	// PSO for opaque objects.
	//
    ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	opaquePsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	opaquePsoDesc.pRootSignature = mRootSignature.Get();
	opaquePsoDesc.VS = 
	{ 
		reinterpret_cast<BYTE*>(mShaders["standardVS"]->GetBufferPointer()), 
		mShaders["standardVS"]->GetBufferSize()
	};
	opaquePsoDesc.PS = 
	{ 
		reinterpret_cast<BYTE*>(mShaders["opaquePS"]->GetBufferPointer()),
		mShaders["opaquePS"]->GetBufferSize()
	};
	opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	opaquePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID; // Изменяем Solid на Wireframe

	opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	opaquePsoDesc.SampleMask = UINT_MAX;
	opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	opaquePsoDesc.NumRenderTargets = 1;
	opaquePsoDesc.RTVFormats[0] = mBackBufferFormat;
	opaquePsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	opaquePsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	opaquePsoDesc.DSVFormat = mDepthStencilFormat;
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&mPSOs["opaque"])));

	// Geometry pass PSO

	D3D12_GRAPHICS_PIPELINE_STATE_DESC gbPsoDesc = {};
	gbPsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	gbPsoDesc.pRootSignature = mRootSignature.Get(); // используем модифицированную корневую сигнатуру
	gbPsoDesc.VS = { reinterpret_cast<BYTE*>(mShaders["gbufferVS"]->GetBufferPointer()),
					 mShaders["gbufferVS"]->GetBufferSize() };
	gbPsoDesc.PS = { reinterpret_cast<BYTE*>(mShaders["gbufferPS"]->GetBufferPointer()),
					 mShaders["gbufferPS"]->GetBufferSize() };
	gbPsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	gbPsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	// Отключаем прозрачность (или настраиваем, если нужны)
	gbPsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	gbPsoDesc.SampleMask = UINT_MAX;
	gbPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	// Теперь указываем несколько рендер-таргетов (G-Buffer)
	gbPsoDesc.NumRenderTargets = 3;
	gbPsoDesc.RTVFormats[0] = albedoFormat;     // альбедо
	gbPsoDesc.RTVFormats[1] = normalFormat; // нормали
	gbPsoDesc.RTVFormats[2] = positionFormat; // позиция
	gbPsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	gbPsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	gbPsoDesc.DSVFormat = mDepthStencilFormat; // з-дефолтовый формат глубины (может быть D32_FLOAT)

	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&gbPsoDesc, IID_PPV_ARGS(&mPSOs["gbuffer"])));

	// Lighting pass PSO


	D3D12_GRAPHICS_PIPELINE_STATE_DESC lightPsoDesc = {};
	lightPsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() }; // если используем SV_VertexID в шейдере, входного layout не нужно
	lightPsoDesc.pRootSignature = mLightingRootSignature.Get(); // наша новая корнев. сигнатура для освещения
	lightPsoDesc.VS = { reinterpret_cast<BYTE*>(mShaders["lightingVS"]->GetBufferPointer()),
						mShaders["lightingVS"]->GetBufferSize() };
	lightPsoDesc.PS = { reinterpret_cast<BYTE*>(mShaders["lightingPS"]->GetBufferPointer()),
						mShaders["lightingPS"]->GetBufferSize() };
	lightPsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	lightPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT;

	D3D12_RENDER_TARGET_BLEND_DESC rtBlendDesc = {};
	rtBlendDesc.BlendEnable = TRUE;                         // включаем смешивание
	rtBlendDesc.LogicOpEnable = FALSE;
	rtBlendDesc.SrcBlend = D3D12_BLEND_ONE;              // src * 1
	rtBlendDesc.DestBlend = D3D12_BLEND_ONE;              // dest * 1
	rtBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;           // сложение
	rtBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
	rtBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;             // альфа сохраняем из src
	rtBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	rtBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL; // RGB + A

	D3D12_BLEND_DESC blendDesc = {};
	blendDesc.AlphaToCoverageEnable = FALSE;
	blendDesc.IndependentBlendEnable = FALSE;
	blendDesc.RenderTarget[0] = rtBlendDesc;
	lightPsoDesc.BlendState = blendDesc;




	lightPsoDesc.SampleMask = UINT_MAX;
	lightPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	
	lightPsoDesc.NumRenderTargets = 1;                   // выводим один финальный цвет
	lightPsoDesc.RTVFormats[0] = mBackBufferFormat;      // формат экрана (обычно DXGI_FORMAT_R8G8B8A8_UNORM)
	lightPsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	lightPsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	lightPsoDesc.DSVFormat = mDepthStencilFormat; // не используем буфер глубины

	//D3D12_DEPTH_STENCIL_DESC dsDesc = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	//dsDesc.DepthEnable = TRUE;
	//dsDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; // можно отключить запись, но оставить тест
	//dsDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	//lightPsoDesc.DepthStencilState = dsDesc;

	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&lightPsoDesc, IID_PPV_ARGS(&mPSOs["lighting"])));

	// Lighting(QUAD) pass PSO


	D3D12_GRAPHICS_PIPELINE_STATE_DESC lightQUADPsoDesc = lightPsoDesc;
	lightQUADPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	lightQUADPsoDesc.VS = { reinterpret_cast<BYTE*>(mShaders["lightingQUADVS"]->GetBufferPointer()),
						mShaders["lightingQUADVS"]->GetBufferSize() };
	
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&lightQUADPsoDesc, IID_PPV_ARGS(&mPSOs["lightingQUAD"])));
	// Debug lighting shapes PSO

	D3D12_GRAPHICS_PIPELINE_STATE_DESC lightShapesPsoDesc = lightPsoDesc;
	lightShapesPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	lightShapesPsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	D3D12_DEPTH_STENCIL_DESC dsDesc = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	dsDesc.DepthEnable = TRUE;
	dsDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; // можно отключить запись, но оставить тест
	dsDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	lightShapesPsoDesc.DepthStencilState = dsDesc;
	lightShapesPsoDesc.PS = { reinterpret_cast<BYTE*>(mShaders["lightingPSDebug"]->GetBufferPointer()),
						mShaders["lightingPSDebug"]->GetBufferSize() };

	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&lightShapesPsoDesc, IID_PPV_ARGS(&mPSOs["lightingShapes"])));

	// PSO for shadow map pass
	D3D12_GRAPHICS_PIPELINE_STATE_DESC shadowPsoDesc = {};
	shadowPsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() }; // Same input layout
	shadowPsoDesc.pRootSignature = mShadowPassRootSignature.Get();
	shadowPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["shadowVS"]->GetBufferPointer()),
		mShaders["shadowVS"]->GetBufferSize()
	};
	// shadowPsoDesc.PS can be omitted if no pixel shader (Null PS)
	// If you have an alpha testing PS:
	// shadowPsoDesc.PS =
	// {
	//    reinterpret_cast<BYTE*>(mShaders["shadowPS"]->GetBufferPointer()),
	//    mShaders["shadowPS"]->GetBufferSize()
	// };
	shadowPsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	// You might need to tweak RasterizerState for shadow acne (DepthBias, SlopeScaledDepthBias)
	// e.g., shadowPsoDesc.RasterizerState.DepthBias = 100000; // Experiment with values
	// shadowPsoDesc.RasterizerState.DepthBiasClamp = 0.0f;
	// shadowPsoDesc.RasterizerState.SlopeScaledDepthBias = 1.0f; // Experiment

	shadowPsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT); // No color writing
	shadowPsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	shadowPsoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	shadowPsoDesc.SampleMask = UINT_MAX;
	shadowPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	shadowPsoDesc.NumRenderTargets = 0; // Not writing to any color targets
	shadowPsoDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
	shadowPsoDesc.SampleDesc.Count = 1; // No MSAA for shadow map
	shadowPsoDesc.SampleDesc.Quality = 0;
	shadowPsoDesc.DSVFormat = SHADOW_MAP_DSV_FORMAT; // Format of our shadow map DSV

	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&shadowPsoDesc, IID_PPV_ARGS(&mPSOs["shadow_map"])));


	D3D12_GRAPHICS_PIPELINE_STATE_DESC debugPsoDesc = {};
	debugPsoDesc.InputLayout = { nullptr, 0 }; // fullscreen quad
	debugPsoDesc.pRootSignature = mLightingRootSignature.Get();
	debugPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["lightingQUADVS"]->GetBufferPointer()),
		mShaders["lightingQUADVS"]->GetBufferSize()
	};
	debugPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["lightingPSDebug"]->GetBufferPointer()),
		mShaders["lightingPSDebug"]->GetBufferSize()
	};
	debugPsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	debugPsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	debugPsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	debugPsoDesc.SampleMask = UINT_MAX;
	debugPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	debugPsoDesc.NumRenderTargets = 1;
	debugPsoDesc.RTVFormats[0] = mBackBufferFormat;
	debugPsoDesc.SampleDesc.Count = 1;
	debugPsoDesc.SampleDesc.Quality = 0;

	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&debugPsoDesc, IID_PPV_ARGS(&mPSOs["debugShadowMap"])));


	D3D12_GRAPHICS_PIPELINE_STATE_DESC pso = {};
	pso.InputLayout = { nullptr, 0 };                    // fullscreen tri
	pso.pRootSignature = mLightingRootSignature.Get();
	pso.VS = {
		reinterpret_cast<BYTE*>(mShaders["lightingQUADVS"]->GetBufferPointer()),
		mShaders["lightingQUADVS"]->GetBufferSize()
	};
	pso.PS = {
		reinterpret_cast<BYTE*>(mShaders["shadowDebugPS"]->GetBufferPointer()),
		mShaders["shadowDebugPS"]->GetBufferSize()
	};
	pso.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	pso.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);


	auto ds = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	ds.DepthEnable = FALSE;
	ds.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	pso.DepthStencilState = ds;

	pso.SampleMask = UINT_MAX;
	pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pso.NumRenderTargets = 1;
	pso.RTVFormats[0] = mBackBufferFormat;
	pso.SampleDesc.Count = 1;
	pso.SampleDesc.Quality = 0;

	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&pso,
		IID_PPV_ARGS(&mPSOs["shadowDebug"])));

	// Terrain PSO 
	D3D12_GRAPHICS_PIPELINE_STATE_DESC terrainPsoDesc = {};
	terrainPsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	terrainPsoDesc.pRootSignature = mTerrainRootSignature.Get();
	terrainPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["terrainVS"]->GetBufferPointer()),
		mShaders["terrainVS"]->GetBufferSize()
	};
	terrainPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["terrainPS"]->GetBufferPointer()),
		mShaders["terrainPS"]->GetBufferSize()
	};
	terrainPsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	terrainPsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;

	terrainPsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	terrainPsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	terrainPsoDesc.SampleMask = UINT_MAX;
	terrainPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	terrainPsoDesc.NumRenderTargets = 3; // G-Buffer
	terrainPsoDesc.RTVFormats[0] = albedoFormat;
	terrainPsoDesc.RTVFormats[1] = normalFormat;
	terrainPsoDesc.RTVFormats[2] = positionFormat;
	terrainPsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	terrainPsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	terrainPsoDesc.DSVFormat = mDepthStencilFormat;

	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&terrainPsoDesc, IID_PPV_ARGS(&mPSOs["terrain"])));
}

void TexColumnsApp::BuildFrameResources()
{
	FlushCommandQueue();
	mFrameResources.clear();
    for(int i = 0; i < gNumFrameResources; ++i)
    {
        mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(),
            1, (UINT)mAllRitems.size(), (UINT)mMaterials.size(),(UINT)mLights.size(), (UINT)mTerrain->GetAllTiles().size()));
    }
	mCurrFrameResourceIndex = 0;
	mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();
	for (auto& ri : mAllRitems)
	{
		ri->NumFramesDirty = gNumFrameResources;
	}
	for (auto& kv : mMaterials)
	{
		kv.second->NumFramesDirty = gNumFrameResources;
	}
	mShadowCastingLights.clear();
	for (int i = 0; i < (int)mLights.size(); i++)
	{
		if (mLights[i].CastsShadows && mLights[i].ShadowMapSrvHeapIndex > 0)
			mShadowCastingLights.push_back(i);
	}
	for (auto& t : mTerrain->GetAllTiles())
	{
		t->NumFramesDirty = gNumFrameResources;
	}
}

void TexColumnsApp::BuildMaterials()
{
	CreateMaterial("NiggaMat",0, TexOffsets["textures/texture"], TexOffsets["textures/texture_nm"], XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT3(0.05f, 0.05f, 0.05f), 0.3f);
	CreateMaterial("eye",0, TexOffsets["textures/eye"], TexOffsets["textures/eye_nm"], XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT3(0.05f, 0.05f, 0.05f), 0.3f);
	CreateMaterial("map",0, TexOffsets["textures/HeightMap2"], TexOffsets["textures/HeightMap2"], XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT3(0.05f, 0.05f, 0.05f), 0.3f);
	CreateMaterial("map2",0, TexOffsets["textures/HeightMap"], TexOffsets["textures/HeightMap"], XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT3(0.05f, 0.05f, 0.05f), 0.3f);

	CreateMaterial("TerrainMaterial", (int)mMaterials.size(), TexOffsets["textures/terr_diffuse"], TexOffsets["textures/terr_normal"], XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f),XMFLOAT3(0.02f, 0.02f, 0.02f),0.8f);
	//mHeightMapResource = mTextures["textures/terr_height"]->Resource.Get();


}
void TexColumnsApp::RenderCustomMesh(std::string unique_name, std::string meshname, std::string materialName, XMFLOAT3 Scale, XMFLOAT3 Rotation, XMFLOAT3 Position)
{
	for (int i = 0;i < ObjectsMeshCount[meshname];i++)
	{
		auto rItem = std::make_unique<RenderItem>();
		std::string textureFile;
		rItem->Name = unique_name;
		auto trans = XMMatrixTranslation(Position.x, Position.y, Position.z);
		auto rot = XMMatrixRotationRollPitchYaw(Rotation.x, Rotation.y, Rotation.z);
		auto scl = XMMatrixScaling(Scale.x, Scale.y, Scale.z);
		XMStoreFloat4x4(&rItem->TexTransform, XMMatrixScaling(1, 1., 1.));
		XMStoreFloat4x4(&rItem->World, scl * rot * trans);
		rItem->TranslationM =  trans;
		rItem->RotationM = rot;
		rItem->ScaleM = scl;

		rItem->Position = Position;
		rItem->RotationAngle = Rotation;
		rItem->Scale = Scale;
		rItem->ObjCBIndex = mAllRitems.size();
		rItem->Geo = mGeometries["shapeGeo"].get();
		rItem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		std::string matname = rItem->Geo->MultiDrawArgs[meshname][i].first.matName;
		std::cout << " mat : " << matname << "\n";
		std::cout << unique_name << " " << matname << "\n";
		if (materialName != "") matname = materialName;
		rItem->Mat = mMaterials[matname].get();
		rItem->IndexCount = rItem->Geo->MultiDrawArgs[meshname][i].second.IndexCount;
		rItem->StartIndexLocation = rItem->Geo->MultiDrawArgs[meshname][i].second.StartIndexLocation;
		rItem->BaseVertexLocation = rItem->Geo->MultiDrawArgs[meshname][i].second.BaseVertexLocation;
		mAllRitems.push_back(std::move(rItem));
	}
	
}



void TexColumnsApp::BuildRenderItems()
{

	//RenderCustomMesh("building", "sponza", "", XMFLOAT3(0.07, 0.07, 0.07), XMFLOAT3(0, 3.14 / 2, 0), XMFLOAT3(0, 0, 0));
	/*RenderCustomMesh("nigga", "negr", "NiggaMat", XMFLOAT3(3, 3, 3), XMFLOAT3(0, 3.14, 0), XMFLOAT3(0, 3, 0));
	RenderCustomMesh("nigga2", "negr", "NiggaMat", XMFLOAT3(3, 3, 3), XMFLOAT3(0, 3.14, 0), XMFLOAT3(5, 3, 0));
	BuildFrameResources();
	for (auto& e : mAllRitems)
	{
		mOpaqueRitems.push_back(e.get());
	}*/

	std::vector<std::shared_ptr<Tile>>& allTiles = mTerrain->GetAllTiles();
	// Теперь, для каждого видимого тайла, создаем или обновляем его RenderItem.
	int a = 0;
	for (int i = 0; i < 10 && i < allTiles.size(); i++)
	{
		auto& t = allTiles[i];
		std::cout
			<< "Tile " << i
			<< ": worldPos = (" << t->worldPos.x
			<< ", " << t->worldPos.y
			<< ", " << t->worldPos.z << ")"
			<< ", tileSize = " << t->tileSize
			<< std::endl;
	}
	for (auto& tile : allTiles)
	{
		auto renderItem = std::make_unique<RenderItem>();
		renderItem->World = MathHelper::Identity4x4();
		renderItem->TexTransform = MathHelper::Identity4x4();
		renderItem->ObjCBIndex = static_cast<int>(mAllRitems.size());
		renderItem->Mat = mMaterials["TerrainMaterial"].get();
		renderItem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		renderItem->Name = "TILE";
		// Выбираем LOD-уровень в зависимости от глубины узла квадродерева.
		int lodIndex = tile->lodLevel;
		std::string lodName = "tile_" + std::to_string(tile->tileIndex) + "_LOD_" + std::to_string(lodIndex);
		renderItem->Geo = mGeometries["terrainGeo"].get();
		renderItem->IndexCount = renderItem->Geo->DrawArgs[lodName].IndexCount;
		renderItem->StartIndexLocation = renderItem->Geo->DrawArgs[lodName].StartIndexLocation;
		renderItem->BaseVertexLocation = renderItem->Geo->DrawArgs[lodName].BaseVertexLocation;

		// Обновляем мировую трансформацию тайла
		XMMATRIX translation = XMMatrixTranslation(tile->worldPos.x, tile->worldPos.y, tile->worldPos.z);
		//std::cout << tile->worldPos.x << "|" << tile->worldPos.y << "|" << tile->worldPos.z << std::endl;
		XMStoreFloat4x4(&renderItem->World, translation);

		tile->rItemIndex = static_cast<int>(mAllRitems.size()) + a;
		mAllRitems.push_back(std::move(renderItem));
	}

	BuildFrameResources();

	for (auto& e : mAllRitems)
	{
		if (e->Name.find("TILE") != std::string::npos)
		{
			continue;
		}
		mOpaqueRitems.push_back(e.get());
	}
}


// NOT USING
void TexColumnsApp::Draw(const GameTimer& gt)
{

	auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;

	// Reuse the memory associated with command recording.
	// We can only reset when the associated command lists have finished execution on the GPU.
	ThrowIfFailed(cmdListAlloc->Reset());

	// A command list can be reset after it has been added to the command queue via ExecuteCommandList.
	// Reusing the command list reuses memory.
	ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque"].Get()));

	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	// Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	// Clear the back buffer and depth buffer.
	mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
	mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	// Specify the buffers we are going to render to.
	mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

	ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvDescriptorHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	auto passCB = mCurrFrameResource->PassCB->Resource();
	mCommandList->SetGraphicsRootConstantBufferView(3, passCB->GetGPUVirtualAddress());


	DrawRenderItems(mCommandList.Get(), mOpaqueRitems);


	// Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	// Done recording commands.
	ThrowIfFailed(mCommandList->Close());

	// Add the command list to the queue for execution.
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
	//DrawShadowDebug(mCommandList.Get(), 256);

	// Swap the back and front buffers
	ThrowIfFailed(mSwapChain->Present(1, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	// Advance the fence value to mark commands up to this fence point.
	mCurrFrameResource->Fence = ++mCurrentFence;

	// Add an instruction to the command queue to set a new fence point. 
	// Because we are on the GPU timeline, the new fence point won't be 
	// set until the GPU finishes processing all the commands prior to this Signal().
	mCommandQueue->Signal(mFence.Get(), mCurrentFence);
}
void TexColumnsApp::DrawSceneToShadowMap()
{
	

	UINT shadowCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(PassShadowConstants));
	for (auto light : mLights)
	{
		if (light.type == 2 || light.type == 3)
		{
			if (light.CastsShadows)
			{
				mCommandList->SetPipelineState(mPSOs["shadow_map"].Get());
				mCommandList->SetGraphicsRootSignature(mShadowPassRootSignature.Get());
				// Set the viewport and scissor rect for the shadow map.
				mCommandList->RSSetViewports(1, &mShadowViewport);
				mCommandList->RSSetScissorRects(1, &mShadowScissorRect);
				// Transition the shadow map from generic read to depth-write.

				mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(

					light.ShadowMap.Get(),

					D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,

					D3D12_RESOURCE_STATE_DEPTH_WRITE));

				// Clear the shadow map.
				mCommandList->ClearDepthStencilView(light.ShadowMapDsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

				// Set the shadow map as the depth-stencil buffer. No render targets.
				mCommandList->OMSetRenderTargets(0, nullptr, FALSE, &light.ShadowMapDsvHandle);

				D3D12_GPU_VIRTUAL_ADDRESS shadowCBAddress = mCurrFrameResource->PassShadowCB->Resource()->GetGPUVirtualAddress() + light.LightCBIndex * shadowCBByteSize;
				mCommandList->SetGraphicsRootConstantBufferView(1, shadowCBAddress);
				// Draw all opaque items.
				UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

				auto objectCB = mCurrFrameResource->ObjectCB->Resource();

				// For each render item...
				for (size_t i = 0; i < mOpaqueRitems.size(); ++i)
				{
					auto ri = mOpaqueRitems[i];
					mCommandList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
					mCommandList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
					mCommandList->IASetPrimitiveTopology(ri->PrimitiveType);

					D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize;
					mCommandList->SetGraphicsRootConstantBufferView(0, objCBAddress);

					mCommandList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
				}
				// Transition the shadow map from depth-write to pixel shader resource for the lighting pass.
				mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(light.ShadowMap.Get(),
					D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
			}
			
		}

	}

}
void TexColumnsApp::DeferredDraw(const GameTimer& gt)
{
	//Terrain Stuff

	//UpdateHeightModificationTexture();
	m_visibleTerrainTiles.clear();
	mTerrain->GetVisibleTiles(m_visibleTerrainTiles);
	/*std::cout << "Tiles total:" << mTerrain->GetAllTiles().size() << std::endl;
	std::cout << "Visible tiles: " << m_visibleTerrainTiles.size() << std::endl;*/
	//std::cout << "HeightIndex: " << mTerrain->mHmapIndex << std::endl;
	//
	//for (auto t : m_visibleTerrainTiles)
	//{
	//	std::cout << t->tileIndex << " Pos: " << t->worldPos.x << "," << t->worldPos.y <<  "," << t->worldPos.z << " LOD: " << t->lodLevel << std::endl; 
	//}

	auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;
	ThrowIfFailed(cmdListAlloc->Reset());
	ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), nullptr));


	// draw shadow maps 
	DrawSceneToShadowMap();


	// ==GEOMETRY PASS==
	mCommandList->SetPipelineState(mPSOs["gbuffer"].Get());


	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	// Обнуляем буферы G-Buffer
	// Очищаем каждый G-Buffer и глубину
	// Стало:
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHs[] = {
	CD3DX12_CPU_DESCRIPTOR_HANDLE(
		mRtvHeap->GetCPUDescriptorHandleForHeapStart(),
		SwapChainBufferCount, // Начинаем после SwapChain
		mRtvDescriptorSize
	),
	CD3DX12_CPU_DESCRIPTOR_HANDLE(
		mRtvHeap->GetCPUDescriptorHandleForHeapStart(),
		SwapChainBufferCount + 1, // Начинаем после SwapChain
		mRtvDescriptorSize
	),
	CD3DX12_CPU_DESCRIPTOR_HANDLE(
		mRtvHeap->GetCPUDescriptorHandleForHeapStart(),
		SwapChainBufferCount + 2, // Начинаем после SwapChain
		mRtvDescriptorSize
	) };
	XMFLOAT4 c(mLights[0].Color.x, mLights[0].Color.y, mLights[0].Color.z,1);
	XMVECTORF32 a;
	a.v = XMLoadFloat4(&c);
	for (int i = 0; i < 3; ++i)
		mCommandList->ClearRenderTargetView(rtvHs[i], a, 0, nullptr);
	mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
	mCommandList->OMSetRenderTargets(3, rtvHs, true, &DepthStencilView());
	
	ID3D12DescriptorHeap* heaps[] = { mSrvDescriptorHeap.Get() /*для текстур*/ };
	mCommandList->SetDescriptorHeaps(_countof(heaps), heaps);
	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	auto passCB = mCurrFrameResource->PassCB->Resource();
	mCommandList->SetGraphicsRootConstantBufferView(3, passCB->GetGPUVirtualAddress());

	DrawRenderItems(mCommandList.Get(), mOpaqueRitems);

	// ===============RENDERING TERRAIN=====================
	if (!m_visibleTerrainTiles.empty())
	{

		if (!mPSOs["terrain"])
		{
			OutputDebugStringA("Terrain PSO не создан!\n");
		}

		mCommandList->SetPipelineState(mPSOs["terrain"].Get());
		mCommandList->SetGraphicsRootSignature(mTerrainRootSignature.Get());
		mCommandList->SetGraphicsRootConstantBufferView(5, passCB->GetGPUVirtualAddress());
		UpdateHeightModificationTexture();
		DrawTileRenderItems(mCommandList.Get(), m_visibleTerrainTiles, mTerrain->mHmapIndex);
	}



	D3D12_RESOURCE_BARRIER barrier[3] = {
	CD3DX12_RESOURCE_BARRIER::Transition(mGBufferAlbedo.Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
	CD3DX12_RESOURCE_BARRIER::Transition(mGBufferNormal.Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
	CD3DX12_RESOURCE_BARRIER::Transition(mGBufferPosition.Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
	};
	mCommandList->ResourceBarrier(3, barrier);
	// ================================================
	
	// ===============LIGHTING PASS=====================

	mCommandList->SetPipelineState(mPSOs["lighting"].Get());
	
	mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

	mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::Black, 0, nullptr);
	



	mCommandList->SetGraphicsRootSignature(mLightingRootSignature.Get());
	
	mCommandList->SetDescriptorHeaps(1, mSrvDescriptorHeap.GetAddressOf());


	CD3DX12_GPU_DESCRIPTOR_HANDLE positionHandle(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	positionHandle.Offset(mTextures.size() + 0, mCbvSrvDescriptorSize);
	CD3DX12_GPU_DESCRIPTOR_HANDLE normalHandle(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	normalHandle.Offset(mTextures.size() + 1, mCbvSrvDescriptorSize);
	CD3DX12_GPU_DESCRIPTOR_HANDLE albedoHandle(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	albedoHandle.Offset(mTextures.size() + 2, mCbvSrvDescriptorSize);
	mCommandList->SetGraphicsRootDescriptorTable(0, positionHandle); // t0
	mCommandList->SetGraphicsRootDescriptorTable(1, normalHandle); // t1
	mCommandList->SetGraphicsRootDescriptorTable(2, albedoHandle); // t2
	mCommandList->SetGraphicsRootConstantBufferView(3, mCurrFrameResource->PassCB->Resource()->GetGPUVirtualAddress()); //b0
	mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);


	
	
	UINT lightCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(LightConstants));
	// draw light
	for (auto& light : mLights)
	{
		auto lightCB = mCurrFrameResource->LightCB->Resource();
		mCommandList->IASetVertexBuffers(0, 1, &mGeometries["shapeGeo"]->VertexBufferView());
		mCommandList->IASetIndexBuffer(&mGeometries["shapeGeo"]->IndexBufferView());

		D3D12_GPU_VIRTUAL_ADDRESS lightCBAddress = lightCB->GetGPUVirtualAddress() + light.LightCBIndex * lightCBByteSize;
		mCommandList->SetGraphicsRootConstantBufferView(5, lightCBAddress); // b2

		if (light.CastsShadows) // Only bind shadow map if this light uses it
		{
			CD3DX12_GPU_DESCRIPTOR_HANDLE shadowSrvHandle(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
			shadowSrvHandle.Offset(light.ShadowMapSrvHeapIndex, mCbvSrvDescriptorSize);
			mCommandList->SetGraphicsRootDescriptorTable(6, shadowSrvHandle); // t3
		}
		// if directional or ambient -> rendering full screen quad
		if (light.type == 0 || light.type == 2 )
		{
			mCommandList->SetPipelineState(mPSOs["lightingQUAD"].Get());
			mCommandList->DrawInstanced(3, 1, 0, 0);
		}
		else
		{
			mCommandList->SetPipelineState(mPSOs["lighting"].Get());
			mCommandList->DrawIndexedInstanced(light.ShapeGeo.IndexCount, 1, light.ShapeGeo.StartIndexLocation, light.ShapeGeo.BaseVertexLocation, 0);
		}
	}


	// draw light shapes
	mCommandList->SetPipelineState(mPSOs["lightingShapes"].Get());
	for (auto& light : mLights)
	{
		if (light.type != 0 && light.type != 2 && light.isDebugOn == 1)
		{
			auto lightCB = mCurrFrameResource->LightCB->Resource();
			mCommandList->IASetVertexBuffers(0, 1, &mGeometries["shapeGeo"]->VertexBufferView());
			mCommandList->IASetIndexBuffer(&mGeometries["shapeGeo"]->IndexBufferView());

			D3D12_GPU_VIRTUAL_ADDRESS lightCBAddress = lightCB->GetGPUVirtualAddress() + light.LightCBIndex * lightCBByteSize;
			mCommandList->SetGraphicsRootConstantBufferView(5, lightCBAddress);

			mCommandList->DrawIndexedInstanced(light.ShapeGeo.IndexCount, 1, light.ShapeGeo.StartIndexLocation, light.ShapeGeo.BaseVertexLocation, 0);
		}
		
	}
	

	// После освещения:
	D3D12_RESOURCE_BARRIER revertBarrier[3] = {
		CD3DX12_RESOURCE_BARRIER::Transition(mGBufferAlbedo.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET),
		CD3DX12_RESOURCE_BARRIER::Transition(mGBufferNormal.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET),
		CD3DX12_RESOURCE_BARRIER::Transition(mGBufferPosition.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET)
	};
	mCommandList->ResourceBarrier(3, revertBarrier);

	ImGui::Render();
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), mCommandList.Get());

	ID3D12DescriptorHeap* correctHeaps[] = { mSrvDescriptorHeap.Get() };

	mCommandList->SetDescriptorHeaps(_countof(correctHeaps), correctHeaps);



	//DrawShadowDebug(mCommandList.Get(), 256);



	// Then transition to PRESENT state

	D3D12_RESOURCE_BARRIER presentBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
		CurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	mCommandList->ResourceBarrier(1, &presentBarrier);


	// Done recording commands.
	ThrowIfFailed(mCommandList->Close());

	// Add the command list to the queue for execution.
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);


	// Swap the back and front buffers
	ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	// Advance the fence value to mark commands up to this fence point.
	mCurrFrameResource->Fence = ++mCurrentFence;

	// Add an instruction to the command queue to set a new fence point. 
	// Because we are on the GPU timeline, the new fence point won't be 
	// set until the GPU finishes processing all the commands prior to this Signal().
	mCommandQueue->Signal(mFence.Get(), mCurrentFence);

}




void TexColumnsApp::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems)
{
    UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
    UINT matCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(MaterialConstants));
 
	auto objectCB = mCurrFrameResource->ObjectCB->Resource();
	auto matCB = mCurrFrameResource->MaterialCB->Resource();

    // For each render item...
    for(size_t i = 0; i < ritems.size(); ++i)
    {
        auto ri = ritems[i];
        cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
        cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
        cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

		CD3DX12_GPU_DESCRIPTOR_HANDLE diffuseHandle(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
		diffuseHandle.Offset(ri->Mat->DiffuseSrvHeapIndex, mCbvSrvDescriptorSize);
		cmdList->SetGraphicsRootDescriptorTable(0, diffuseHandle);
		CD3DX12_GPU_DESCRIPTOR_HANDLE normalHandle(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
		normalHandle.Offset(ri->Mat->NormalSrvHeapIndex, mCbvSrvDescriptorSize);
		cmdList->SetGraphicsRootDescriptorTable(1, normalHandle);

        D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex*objCBByteSize;
		D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = matCB->GetGPUVirtualAddress() + ri->Mat->MatCBIndex*matCBByteSize;

        cmdList->SetGraphicsRootConstantBufferView(2, objCBAddress);
        cmdList->SetGraphicsRootConstantBufferView(4, matCBAddress);

        cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
    }
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 7> TexColumnsApp::GetStaticSamplers()
{
	// Applications usually only need a handful of samplers.  So just define them all up front
	// and keep them available as part of the root signature.  

	const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
		1, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		2, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
		3, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
		4, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
		0.0f,                             // mipLODBias
		8);                               // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
		5, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
		0.0f,                              // mipLODBias
		8);                                // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC shadowSampler(
		6, // shaderRegister s6 (assuming 0-5 are used)
		D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT, // Comparison filter
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // Use BORDER for shadow maps
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,
		0.0f, // mipLODBias
		16,   // maxAnisotropy (not really used for comparison filter but set it)
		D3D12_COMPARISON_FUNC_LESS_EQUAL, // Comparison function
		D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK); // Border color (or opaque white if depth is 1.0)

	return {
		pointWrap, pointClamp,
		linearWrap, linearClamp,
		anisotropicWrap, anisotropicClamp,
		shadowSampler // Add the new sampler
	};
}

void TexColumnsApp::DrawShadowDebug(ID3D12GraphicsCommandList* cmdList, UINT size)
{

	cmdList->SetPipelineState(mPSOs["shadowDebug"].Get());
	cmdList->SetGraphicsRootSignature(mLightingRootSignature.Get());
	std::vector<int> spotLightIndices;
	for (int i = 0; i < mShadowCastingLights.size(); i++)
	{
		int lightIndex = mShadowCastingLights[i];
		if (mLights[lightIndex].type == 3)
		{
			spotLightIndices.push_back(lightIndex);
		}
	}

	if (spotLightIndices.empty()) return;


	int spotLightIdx = mShadowDebugLightIdx % spotLightIndices.size();
	int lightIndex = spotLightIndices[spotLightIdx];
	auto& light = mLights[lightIndex];

	D3D12_VIEWPORT vp{};
	vp.TopLeftX = static_cast<float>(mClientWidth - size);
	vp.TopLeftY = static_cast<float>(mClientHeight - size);
	vp.Width = static_cast<float>(size);
	vp.Height = static_cast<float>(size);
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;

	D3D12_RECT sc{};
	sc.left = (LONG)vp.TopLeftX;
	sc.top = (LONG)vp.TopLeftY;
	sc.right = (LONG)(vp.TopLeftX + vp.Width);
	sc.bottom = (LONG)(vp.TopLeftY + vp.Height);

	cmdList->RSSetViewports(1, &vp);
	cmdList->RSSetScissorRects(1, &sc);


	CD3DX12_GPU_DESCRIPTOR_HANDLE shadowSrvHandle(
		mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart(),
		light.ShadowMapSrvHeapIndex,
		mCbvSrvDescriptorSize);

	cmdList->SetGraphicsRootDescriptorTable(7, shadowSrvHandle);


	cmdList->IASetVertexBuffers(0, 0, nullptr);
	cmdList->IASetIndexBuffer(nullptr);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->DrawInstanced(3, 1, 0, 0);

}

std::vector<std::shared_ptr<Tile>>& Terrain::GetAllTiles()
{
	return mAllTiles;
}

void TexColumnsApp::GenerateTileGeometry(const XMFLOAT3& worldPos, float tileSize, int lodLevel,
	std::vector<Vertex>& vertices, std::vector<std::uint32_t>& indices)
{
	int baseResolution = 16;
	float Factor = 1;
	int resolution = static_cast<int>(baseResolution * std::pow(Factor, lodLevel));
	//int resolution = (1 << (6 - lodLevel)) + 1;
	vertices.clear();
	indices.clear();
	//float stepSize = tileSize;
	float stepSize = tileSize / (resolution - 1 );
	float skirtDepth = 10;

	for (int z = 0; z < resolution; z++)
	{
		for (int x = 0; x < resolution; x++)
		{
			Vertex vertex;
			vertex.Pos = XMFLOAT3(x * stepSize, 0.0f, z * stepSize);
			vertex.TexC = XMFLOAT2((float)x / (resolution - 1), (float)z / (resolution - 1));
			vertex.Normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
			vertex.Tangent = XMFLOAT3(1.0f, 0.0f, 0.0f);
			vertices.push_back(vertex);
		}
	}

	int mainVertexCount = static_cast<int>(vertices.size());


	for (int z = 0; z < resolution; z++)
	{
		Vertex vertex = vertices[z * resolution + 0];
		vertex.Pos.y = -skirtDepth;
		vertices.push_back(vertex);
	}


	for (int z = 0; z < resolution; z++)
	{
		Vertex vertex = vertices[z * resolution + (resolution - 1)];
		vertex.Pos.y = -skirtDepth;
		vertices.push_back(vertex);
	}


	for (int x = 1; x < resolution - 1; x++)
	{
		Vertex vertex = vertices[0 * resolution + x];
		vertex.Pos.y = -skirtDepth;
		vertices.push_back(vertex);
	}


	for (int x = 1; x < resolution - 1; x++)
	{
		Vertex vertex = vertices[(resolution - 1) * resolution + x];
		vertex.Pos.y = -skirtDepth;
		vertices.push_back(vertex);
	}

	
	for (int z = 0; z < resolution - 1; z++)
	{
		for (int x = 0; x < resolution - 1; x++)
		{
			UINT topLeft = z * resolution + x;
			UINT topRight = topLeft + 1;
			UINT bottomLeft = (z + 1) * resolution + x;
			UINT bottomRight = bottomLeft + 1;

			indices.push_back(topLeft);
			indices.push_back(bottomLeft);
			indices.push_back(topRight);

			indices.push_back(topRight);
			indices.push_back(bottomLeft);
			indices.push_back(bottomRight);
		}
	}

	
	int leftSkirtStart = mainVertexCount;
	int rightSkirtStart = leftSkirtStart + resolution;
	int bottomSkirtStart = rightSkirtStart + resolution;
	int topSkirtStart = bottomSkirtStart + (resolution - 2);

	
	for (int z = 0; z < resolution - 1; z++)
	{
		UINT edge1 = z * resolution;
		UINT edge2 = (z + 1) * resolution;
		UINT skirt1 = leftSkirtStart + z;
		UINT skirt2 = leftSkirtStart + z + 1;

		indices.push_back(edge1);
		indices.push_back(skirt1);
		indices.push_back(edge2);

		indices.push_back(edge2);
		indices.push_back(skirt1);
		indices.push_back(skirt2);
	}

	 
	for (int z = 0; z < resolution - 1; z++)
	{
		UINT edge1 = z * resolution + (resolution - 1);
		UINT edge2 = (z + 1) * resolution + (resolution - 1);
		UINT skirt1 = rightSkirtStart + z;
		UINT skirt2 = rightSkirtStart + z + 1;

		indices.push_back(edge1);
		indices.push_back(edge2);
		indices.push_back(skirt1);

		indices.push_back(edge2);
		indices.push_back(skirt2);
		indices.push_back(skirt1);
	}


	for (int x = 1; x < resolution - 2; x++)
	{
		UINT edge1 = x;
		UINT edge2 = x + 1;
		UINT skirt1 = bottomSkirtStart + (x - 1);
		UINT skirt2 = bottomSkirtStart + x;

		indices.push_back(edge1);
		indices.push_back(edge2);
		indices.push_back(skirt1);

		indices.push_back(edge2);
		indices.push_back(skirt2);
		indices.push_back(skirt1);
	}

	
	for (int x = 1; x < resolution - 2; x++)
	{
		UINT edge1 = (resolution - 1) * resolution + x;
		UINT edge2 = (resolution - 1) * resolution + x + 1;
		UINT skirt1 = topSkirtStart + (x - 1);
		UINT skirt2 = topSkirtStart + x;

		indices.push_back(edge1);
		indices.push_back(skirt1);
		indices.push_back(edge2);

		indices.push_back(edge2);
		indices.push_back(skirt1);
		indices.push_back(skirt2);
	}
}

void TexColumnsApp::BuildTerrainGeometry()
{
	auto terrainGeo = std::make_unique<MeshGeometry>();
	terrainGeo->Name = "terrainGeo";

	
	auto& allTiles = mTerrain->GetAllTiles();

	
	std::vector<Vertex> allVertices;
	std::vector<std::uint32_t> allIndices;

	

	for (int tileIdx = 0; tileIdx < allTiles.size(); tileIdx++)
	{
		auto& tile = allTiles[tileIdx];

		std::vector<Vertex> tileVertices;
		std::vector<std::uint32_t> tileIndices;

		GenerateTileGeometry(tile->worldPos, tile->tileSize, tile->lodLevel, tileVertices, tileIndices);


		UINT baseVertex = static_cast<int>(allVertices.size());
		for (auto& index : tileIndices)
		{
			index += baseVertex;
		}

		
		SubmeshGeometry submesh;
		submesh.IndexCount = (UINT)tileIndices.size();
		submesh.StartIndexLocation = (UINT)allIndices.size();
		submesh.BaseVertexLocation = 0;
		//submesh.BaseVertexLocation = baseVertex;

		std::string submeshName = "tile_" + std::to_string(tileIdx) + "_LOD_" + std::to_string(tile->lodLevel);
		terrainGeo->DrawArgs[submeshName] = submesh;
	
		allVertices.insert(allVertices.end(), tileVertices.begin(), tileVertices.end());
		allIndices.insert(allIndices.end(), tileIndices.begin(), tileIndices.end());

	}

	const UINT vbByteSize = (UINT)allVertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)allIndices.size() * sizeof(std::uint32_t);

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &terrainGeo->VertexBufferCPU));
	CopyMemory(terrainGeo->VertexBufferCPU->GetBufferPointer(), allVertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &terrainGeo->IndexBufferCPU));
	CopyMemory(terrainGeo->IndexBufferCPU->GetBufferPointer(), allIndices.data(), ibByteSize);

	terrainGeo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(),
		allVertices.data(), vbByteSize,
		terrainGeo->VertexBufferUploader);

	terrainGeo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(),
		allIndices.data(), ibByteSize,
		terrainGeo->IndexBufferUploader);

	terrainGeo->VertexByteStride = sizeof(Vertex);
	terrainGeo->VertexBufferByteSize = vbByteSize;
	terrainGeo->IndexFormat = DXGI_FORMAT_R32_UINT;
	terrainGeo->IndexBufferByteSize = ibByteSize;

	mGeometries[terrainGeo->Name] = std::move(terrainGeo);
}

void TexColumnsApp::UpdateTerrain(const GameTimer& gt)
{
	if (!mTerrain)
		return;


	// Обновляем позицию камеры
	XMVECTOR camPos = cam.GetPosition();
	XMFLOAT3 cameraPosition;
	XMStoreFloat3(&cameraPosition, camPos);

	// Извлекаем плоскости frustum
	XMMATRIX view = XMLoadFloat4x4(&mView);
	XMMATRIX proj = XMLoadFloat4x4(&mProj);
	XMMATRIX viewProj = XMMatrixMultiply(view, proj);

	// Обновляем terrain систему. Это заполняет m_visibleTiles
	mTerrain->Update(cameraPosition, cam.GetFrustum());
	mTerrain->mHeightScale = heightScale;


}


void TexColumnsApp::DrawTileRenderItems(ID3D12GraphicsCommandList* cmdList, std::vector<Tile*> tiles, int HeightIndex)
{


	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
	UINT matCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(MaterialConstants));
	UINT terrCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(TerrainConstants));

	auto objectCB = mCurrFrameResource->ObjectCB->Resource();
	auto matCB = mCurrFrameResource->MaterialCB->Resource();
	auto terrCB = mCurrFrameResource->TerrainCB->Resource();

	for (auto& t : tiles)
	{
		if (t->rItemIndex < 0 || t->rItemIndex >= mAllRitems.size())
		{
			std::cout << "Invalid rItemIndex for tile: " << t->tileIndex << std::endl;
			continue;
		}

		auto ri = mAllRitems[t->rItemIndex].get();

		cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
		cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
		cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

		// Установка дескрипторных таблиц для текстур
		// Слот 0: heightMap (t0)
		CD3DX12_GPU_DESCRIPTOR_HANDLE heightHandle(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
		heightHandle.Offset(HeightIndex, mCbvSrvDescriptorSize);
		cmdList->SetGraphicsRootDescriptorTable(0, heightHandle);

		// Слот 1: diffuseMap (t1)
		CD3DX12_GPU_DESCRIPTOR_HANDLE diffuseHandle(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
		diffuseHandle.Offset(ri->Mat->DiffuseSrvHeapIndex, mCbvSrvDescriptorSize);
		cmdList->SetGraphicsRootDescriptorTable(1, diffuseHandle);

		// Слот 2: normalMap (t2)
		CD3DX12_GPU_DESCRIPTOR_HANDLE normalHandle(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
		normalHandle.Offset(ri->Mat->NormalSrvHeapIndex, mCbvSrvDescriptorSize);
		cmdList->SetGraphicsRootDescriptorTable(2, normalHandle);

		// Слот 3: heightModificationMap (t3) - НОВЫЙ!
		if (mHeightModificationSrvIndex >= 0)
		{
			CD3DX12_GPU_DESCRIPTOR_HANDLE heightModHandle(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
			heightModHandle.Offset(mHeightModificationSrvIndex, mCbvSrvDescriptorSize);
			cmdList->SetGraphicsRootDescriptorTable(3, heightModHandle);
		}
		else
		{
			// Если текстура изменений не инициализирована, можно установить дескриптор null
			// Или использовать дескриптор с нулевыми значениями
			// Временно устанавливаем тот же дескриптор, что и для heightMap
			cmdList->SetGraphicsRootDescriptorTable(3, heightHandle);
		}

		// Установка константных буферов
		// Слот 4: cbPerObject (b0)
		D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize;
		cmdList->SetGraphicsRootConstantBufferView(4, objCBAddress);

		// Слот 5: cbPass (b1) - Устанавливается в DeferredDraw перед вызовом этой функции
		// cmdList->SetGraphicsRootConstantBufferView(5, ...); // Устанавливается в DeferredDraw

		// Слот 6: cbMaterial (b2)
		D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = matCB->GetGPUVirtualAddress() + ri->Mat->MatCBIndex * matCBByteSize;
		cmdList->SetGraphicsRootConstantBufferView(6, matCBAddress);

		// Слот 7: cbTerrainTile (b3)
		D3D12_GPU_VIRTUAL_ADDRESS tileCBAddress = terrCB->GetGPUVirtualAddress() + t->tileIndex * terrCBByteSize;
		cmdList->SetGraphicsRootConstantBufferView(7, tileCBAddress);

		cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
	}

	//static int drawCount = 0;
	//drawCount++;
	//if (drawCount % 60 == 0) // Выводить каждую секунду (при 60 FPS)
	//{
	//	std::cout << "Drawing " << tiles.size() << " tiles. "
	//		<< "HeightMod SRV index: " << mHeightModificationSrvIndex
	//		<< ", Data dirty: " << mHeightModificationDirty << std::endl;
	//}

}

void TexColumnsApp::InitializeHeightModificationTexture()
{
	// Размер должен совпадать с размером heightmap
	mHeightModificationWidth = mHeightMapWidth;
	mHeightModificationHeight = mHeightMapHeight;

	// Инициализируем нулевыми значениями
	mHeightModificationData.resize(mHeightModificationWidth * mHeightModificationHeight, 0.0f);

	// Создаем ресурс текстуры
	D3D12_RESOURCE_DESC texDesc = {};
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Alignment = 0;
	texDesc.Width = mHeightModificationWidth;
	texDesc.Height = mHeightModificationHeight;
	texDesc.DepthOrArraySize = 1;
	texDesc.MipLevels = 1;
	texDesc.Format = DXGI_FORMAT_R32_FLOAT;  // Один канал для изменения высоты
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&mHeightModificationTexture)));

	// Создаем upload heap
	const UINT64 uploadBufferSize = GetRequiredIntermediateSize(mHeightModificationTexture.Get(), 0, 1);
	heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	D3D12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);
	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&mHeightModificationTextureUpload)));

	// Инициализируем нулями
	D3D12_SUBRESOURCE_DATA textureData = {};
	textureData.pData = mHeightModificationData.data();
	textureData.RowPitch = mHeightModificationWidth * sizeof(float);
	textureData.SlicePitch = textureData.RowPitch * mHeightModificationHeight;

	UpdateSubresources(mCommandList.Get(), mHeightModificationTexture.Get(),
		mHeightModificationTextureUpload.Get(), 0, 0, 1, &textureData);

	// Переводим в состояние для шейдера
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		mHeightModificationTexture.Get(),
		D3D12_RESOURCE_STATE_COPY_DEST,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

	// Создаем SRV в куче дескрипторов
	// Нужно добавить место в куче дескрипторов в BuildDescriptorHeaps
}


void TexColumnsApp::ApplyBrushWithPersistence(const XMFLOAT2& uv, bool raise)
{
	// Преобразуем UV в координаты текстуры
	int texX = static_cast<int>(uv.x * mHeightModificationWidth);
	int texY = static_cast<int>(uv.y * mHeightModificationHeight);

	// Радиус кисти в пикселях
	float brushRadiusPixels = mBrushRadius * mHeightModificationWidth;

	// Область влияния кисти
	int minX = max(0, texX - static_cast<int>(brushRadiusPixels));
	int maxX = (std::min)(static_cast<int>(mHeightModificationWidth), texX + static_cast<int>(brushRadiusPixels));
	int minY = max(0, texY - static_cast<int>(brushRadiusPixels));
	int maxY = std::min(static_cast<int>(mHeightModificationHeight), texY + static_cast<int>(brushRadiusPixels));

	for (int y = minY; y < maxY; ++y)
	{
		for (int x = minX; x < maxX; ++x)
		{
			float dx = x - texX;
			float dy = y - texY;
			float dist = sqrtf(dx * dx + dy * dy);

			if (dist <= brushRadiusPixels)
			{
				// Сглаженное затухание к краям
				float falloff = 1.0f - (dist / brushRadiusPixels);
				falloff = falloff * falloff; // Квадратичное затухание

				// Применяем изменение
				int index = y * mHeightModificationWidth + x;
				if (raise)
				{
					mHeightModificationData[index] += mBrushStrength * falloff;
				}
				else
				{
					mHeightModificationData[index] -= mBrushStrength * falloff;
				}

				// Ограничиваем значения
				mHeightModificationData[index] = (std::max)(-1.0f, std::min(1.0f, mHeightModificationData[index]));
			}
		}
	}

	mHeightModificationDirty = true;
}

void TexColumnsApp::UpdateHeightModificationTexture()
{
	if (!mHeightModificationDirty)
		return;

	// Обновляем текстуру на GPU
	D3D12_SUBRESOURCE_DATA textureData = {};
	textureData.pData = mHeightModificationData.data();
	textureData.RowPitch = mHeightModificationWidth * sizeof(float);
	textureData.SlicePitch = textureData.RowPitch * mHeightModificationHeight;

	// Переводим в состояние для копирования
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		mHeightModificationTexture.Get(),
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_COPY_DEST));

	UpdateSubresources(mCommandList.Get(), mHeightModificationTexture.Get(),
		mHeightModificationTextureUpload.Get(), 0, 0, 1, &textureData);

	// Возвращаем в состояние для шейдера
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		mHeightModificationTexture.Get(),
		D3D12_RESOURCE_STATE_COPY_DEST,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

	mHeightModificationDirty = false;
}