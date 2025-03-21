#include "Scene.h"
#include "Entity.h"
#include <algorithm>
#include "MeshRenderer.h"

void Scene::AddEntity(std::shared_ptr<Entity> entity)
{
    entities.push_back(entity);
    if (!entity->isHidden()) UpdateRenderCache(entity.get());
}

void Scene::RemoveEntity(const Entity* entity)
{
    auto it = std::find_if(entities.begin(), entities.end(),
                            [entity](const auto& ptr) { return ptr.get() == entity; });
    if (it != entities.end())
    {
        ClearRenderCache(entity);
        entities.erase(it);
    }
}

void Scene::Update(float deltaTime)
{
    for (auto& entity : entities)
    {
        entity->Update(deltaTime);
    }
}

void Scene::Render(ComPtr<ID3D12GraphicsCommandList> commandList)
{
    // Разделяем на непрозрачные и прозрачные из кэша
    std::vector<RendererComponent*> opaque, transparent;
    for (const auto& [type, renders] : rendererCache)
    {
        for (auto* renderer : renders)
        {
            if (renderer->IsEnabled())
            {
                (renderer->IsTransparent() ? transparent : opaque).push_back(renderer);
            }
        }
    }

    // Рендеринг: сначала непрозрачные, затем прозрачные
    for (auto* renderer : opaque)
    {
        renderer->Render(commandList);
    }
    for (auto* renderer : transparent)
    {
        renderer->Render(commandList);
    }
}

void Scene::UpdateRenderCache(Entity* entity)
{
    const auto& renders = entity->GetRenderables();
    for (auto* render : renders)
    {
        std::string type = (dynamic_cast<MeshRenderer*>(render) ? "Mesh" : "Sprite");
        rendererCache[type].push_back(render);
        // Сортируем сразу при добавлении
        std::sort(rendererCache[type].begin(), rendererCache[type].end(),
                    [](const RendererComponent* a, const RendererComponent* b) { return a->GetOrder() < b->GetOrder(); });
    }
}

void Scene::ClearRenderCache(const Entity* entity)
{
    const auto& renderers = entity->GetRenderables();
    for (auto* renderer : renderers)
    {
        std::string type = (dynamic_cast<MeshRenderer*>(renderer) ? "Mesh" : "Sprite");
        auto& cache = rendererCache[type];
        cache.erase(std::remove(cache.begin(), cache.end(), renderer), cache.end());
    }
}
