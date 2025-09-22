#pragma once

#include "Common/d3dUtil.h"
#include "SimpleMath.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;

// Структура частицы
struct Particle {
    SimpleMath::Vector3 pos;        // текущая позиция частицы
    SimpleMath::Vector3 prevPos;    // предыдущая позиция частицы
    SimpleMath::Vector3 velocity;   // скорость и направление
    SimpleMath::Vector3 acceleration; // ускорение
    float energy;                   // энергия (время жизни)
    float size;                     // размер частицы
    float sizeDelta;                // изменение размера с течением времени
    float weight;                   // вес (влияние гравитации)
    float weightDelta;              // изменение веса с течением времени
    float color[4];                 // текущий цвет (RGBA)
    float colorDelta[4];            // изменение цвета с течением времени
};

// Класс ParticleSystem
class ParticleSystem {
public:
    ParticleSystem(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, ID3D12DescriptorHeap* srvHeap, UINT descriptorOffset, int maxParticles, SimpleMath::Vector3 origin);
    virtual ~ParticleSystem();

    virtual void Update(float elapsedTime);
    virtual void Simulate(ID3D12GraphicsCommandList* cmdList, float deltaTime);
    virtual void Render(ID3D12GraphicsCommandList* cmdList, const SimpleMath::Matrix& view, const SimpleMath::Matrix& proj);
    virtual int Emit(int numParticles);
    virtual void InitializeSystem();
    virtual void KillSystem();

public:
    virtual void InitializeParticle(int index);

    // Приватные члены данных
    Particle* particleList;             // массив частиц
    int maxParticles;                   // максимальное количество частиц
    int numParticles;                   // текущее количество частиц
    SimpleMath::Vector3 origin;         // центр системы частиц
    float accumulatedTime;              // накопленное время для эмиссии
    SimpleMath::Vector3 force;          // внешняя сила (гравитация, ветер и т.д.)

    // DirectX 12 ресурсы
    ComPtr<ID3D12Resource> mParticleBuffer1; // StructuredBuffer для частиц
    ComPtr<ID3D12Resource> mParticleBuffer2; // Второй StructuredBuffer для ping-pong
    ID3D12Resource* mCurrentBuffer;         // Текущий буфер
    ComPtr<ID3D12Resource> mUploadBuffer;   // Буфер для загрузки начальных данных

    // Дескрипторы
    D3D12_GPU_DESCRIPTOR_HANDLE mAppendUAV; // UAV для AppendStructuredBuffer
    D3D12_GPU_DESCRIPTOR_HANDLE mConsumeSRV; // SRV для ConsumeStructuredBuffer
    D3D12_GPU_DESCRIPTOR_HANDLE mBuffer1UAV;
    D3D12_GPU_DESCRIPTOR_HANDLE mBuffer1SRV;
    D3D12_GPU_DESCRIPTOR_HANDLE mBuffer2UAV;
    D3D12_GPU_DESCRIPTOR_HANDLE mBuffer2SRV;

    // Root signatures и PSO
    ComPtr<ID3D12RootSignature> mComputeRootSig;
    ComPtr<ID3D12RootSignature> mRenderRootSig;
    ComPtr<ID3D12PipelineState> mComputePSO;
    ComPtr<ID3D12PipelineState> mRenderPSO;

    // Устройство и дескрипторный хип
    ID3D12Device* mDevice;
    ID3D12GraphicsCommandList* mInitialCmdList;
    ID3D12DescriptorHeap* mSrvHeap;
    UINT mDescriptorOffset;
    UINT mDescriptorSize;

private:
    void BuildBuffers();
    void CreateDescriptors();
    void BuildShadersAndPSOs();
    void SwapBuffers();
    D3D12_VERTEX_BUFFER_VIEW VertexBufferView() const;
};