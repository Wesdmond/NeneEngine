// ParticleSystem.h
#pragma once

#include <d3d12.h>
#include <wrl.h>
#include <memory>
#include <DirectXMath.h>
#include "Common/UploadBuffer.h"
#include "Common/d3dx12.h"
#include "Common/MathHelper.h"
#include "Common/GameTimer.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;

struct Particle
{
    XMFLOAT3 pos;
    float lifetime;
    XMFLOAT3 vel;
    float life;
    float size;
    float rot;
    int alive;
    XMFLOAT4 color;
};

class ParticleSystem
{
public:
    ParticleSystem(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, ID3D12DescriptorHeap* srvHeap, UINT descriptorOffset, UINT maxParticles, XMFLOAT3 origin);
    ~ParticleSystem() = default;

    void Update(const GameTimer& gt);
    void Draw(ID3D12GraphicsCommandList* cmdList, D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle, D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle, const PassConstants& passCB);

    XMFLOAT3 force = XMFLOAT3(0.0f, -9.8f, 0.0f); // Public for ImGui control
    UINT numParticles = 0;
    UINT maxParticles;

private:
    void BuildResources(ID3D12GraphicsCommandList* cmdList);
    void BuildDescriptors();
    void BuildRootSignatures();
    void BuildShadersAndPSOs();
    void Simulate(const GameTimer& gt);
    void Render();

    ID3D12Device* md3dDevice;
    ID3D12GraphicsCommandList* mCommandList;
    ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap; // Main SRV heap from app
    UINT mDescriptorOffset; // Starting index in the heap for particle descriptors

    ComPtr<ID3D12Resource> mParticlesA;
    ComPtr<ID3D12Resource> mParticlesB;
    std::unique_ptr<UploadBuffer<Particle>> mParticlesInit;
    struct SimCB { float dt; XMFLOAT3 gravity; };
    std::unique_ptr<UploadBuffer<SimCB>> mSimCB;

    ComPtr<ID3D12DescriptorHeap> mParticleHeap;
    enum { KSRV_A = 0, KUAV_A = 1, KSRV_B = 2, KUAV_B = 3, KSRV_Sprite = 4 };

    ComPtr<ID3D12RootSignature> mParticleCS_RS;
    ComPtr<ID3D12RootSignature> mParticleGfx_RS;
    ComPtr<ID3D12PipelineState> mParticleCS_PSO;
    ComPtr<ID3D12PipelineState> mParticleGfx_PSO;

    UINT mParticleCount;
    XMFLOAT3 mOrigin;

    // Assume a checkerboard texture or replace with actual texture
    ComPtr<ID3D12Resource> mSpriteTexture; // Placeholder, load your texture
};