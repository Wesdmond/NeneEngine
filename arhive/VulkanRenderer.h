#pragma once
#include "Renderer.h"
class VulkanRenderer : public Renderer
{
	DECLARE_SINGLETON(VulkanRenderer)
public:
	bool Initialize() override;
	void Render() override;
	void Shutdown() override;
};