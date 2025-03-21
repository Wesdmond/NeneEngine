#pragma once
#include "RendererComponent.h"

class MeshRenderer : public RendererComponent
{
public:
    MeshRenderer(int order = 0, bool isTransparent = false) : RendererComponent(order, isTransparent) {}
    void Render(ComPtr<ID3D12GraphicsCommandList>& commandList) override {}
};

