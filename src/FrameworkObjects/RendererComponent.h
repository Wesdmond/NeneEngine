#pragma once
#include "Component.h"
#include <d3d12.h>
#include <wrl.h>
using Microsoft::WRL::ComPtr;

class RendererComponent : public Component
{
public:
    RendererComponent(int order = 0, bool isTransparent = false)
        : order(order), isTransparent(isTransparent)
    {}

    virtual void Render(ComPtr<ID3D12GraphicsCommandList>& commandList) = 0;
    int GetOrder() const { return order; }
    bool IsTransparent() const { return isTransparent; }

private:
    int order;
    bool isTransparent;
};