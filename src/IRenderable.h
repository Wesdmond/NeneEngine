#pragma once
#include <d3d12.h>
#include <wrl.h>
using Microsoft::WRL::ComPtr;

class IRenderable
{
public:
    virtual void Render(ComPtr<ID3D12GraphicsCommandList> commandList) = 0;
    virtual ~IRenderable() = default;
};