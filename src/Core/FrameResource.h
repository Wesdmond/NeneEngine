#pragma once

#include "Common/d3dUtil.h"
#include "Common/MathHelper.h"
#include "Common/UploadBuffer.h"

struct ObjectConstants
{
    DirectX::SimpleMath::Matrix World = DirectX::SimpleMath::Matrix::Identity;
    DirectX::SimpleMath::Matrix TexTransform = DirectX::SimpleMath::Matrix::Identity;
};

struct PassConstants
{
    DirectX::SimpleMath::Matrix View        = DirectX::SimpleMath::Matrix::Identity;
    DirectX::SimpleMath::Matrix InvView     = DirectX::SimpleMath::Matrix::Identity;
    DirectX::SimpleMath::Matrix Proj        = DirectX::SimpleMath::Matrix::Identity;
    DirectX::SimpleMath::Matrix InvProj     = DirectX::SimpleMath::Matrix::Identity;
    DirectX::SimpleMath::Matrix ViewProj    = DirectX::SimpleMath::Matrix::Identity;
    DirectX::SimpleMath::Matrix InvViewProj = DirectX::SimpleMath::Matrix::Identity;
    DirectX::SimpleMath::Vector3 EyePosW    = DirectX::SimpleMath::Vector3::Zero;
    float cbPerObjectPad1 = 0.0f;
    DirectX::SimpleMath::Vector2 RenderTargetSize       = DirectX::SimpleMath::Vector2::Zero;
    DirectX::SimpleMath::Vector2 InvRenderTargetSize    = DirectX::SimpleMath::Vector2::Zero;
    float NearZ = 0.0f;
    float FarZ = 0.0f;
    float TotalTime = 0.0f;
    float DeltaTime = 0.0f;

    DirectX::SimpleMath::Color AmbientLight; // {0, 0, 0, 1} by default
};

struct Vertex
{
    DirectX::SimpleMath::Vector3 Pos;
    DirectX::SimpleMath::Vector3 Normal;
    DirectX::SimpleMath::Vector3 TangentU;
    DirectX::SimpleMath::Vector2 TexC;
};

enum LightTypes {
    AMBIENT     = 0,
    DIRECTIONAL = 1,
    POINTLIGHT  = 2,
    SPOTLIGHT   = 3
};

struct LightData {
    Light light;
    UINT lightType;
    float pad[3];
    DirectX::SimpleMath::Matrix WorldLight = DirectX::SimpleMath::Matrix::Identity;
};

// Stores the resources needed for the CPU to build the command lists
// for a frame.  
struct FrameResource
{
public:

    FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount, UINT materialCount, UINT lightCount);
    FrameResource(const FrameResource& rhs) = delete;
    FrameResource& operator=(const FrameResource& rhs) = delete;
    ~FrameResource();

    // We cannot reset the allocator until the GPU is done processing the commands.
    // So each frame needs their own allocator.
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CmdListAlloc;

    // We cannot update a cbuffer until the GPU is done processing the commands
    // that reference it.  So each frame needs their own cbuffers.
   // std::unique_ptr<UploadBuffer<FrameConstants>> FrameCB = nullptr;
    std::unique_ptr<UploadBuffer<PassConstants>> PassCB = nullptr;
    std::unique_ptr<UploadBuffer<MaterialConstants>> MaterialCB = nullptr;
    std::unique_ptr<UploadBuffer<ObjectConstants>> ObjectCB = nullptr;
    std::unique_ptr<UploadBuffer<LightData>> LightCB = nullptr;

    // Fence value to mark commands up to this fence point.  This lets us
    // check if these frame resources are still in use by the GPU.
    UINT64 Fence = 0;
};