#pragma once
#include "RendererComponent.h"

class SpriteRenderer : public RendererComponent
{
public:
    SpriteRenderer(int order = 0, bool isTransparent = true) : RendererComponent(order, isTransparent) {}
    void Render(ComPtr<ID3D12GraphicsCommandList>& commandList) override {}
};
