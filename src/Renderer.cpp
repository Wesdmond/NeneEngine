#include "Renderer.h"
#include "DirectX12Renderer.h"
#include "VulkanRenderer.h"

std::unique_ptr<Renderer> RendererFactory::CreateRenderer(RenderAPI api)
{
    switch (api)
    {
    case RenderAPI::DirectX12:
        return std::unique_ptr<Renderer>(&DirectX12Renderer::GetInstance());
    case RenderAPI::Vulkan:
        return std::unique_ptr<Renderer>(&VulkanRenderer::GetInstance());
    default:
        return nullptr;
    }
}