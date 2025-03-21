#pragma once
#include <vector>
#include <memory>
#include <d3d12.h>
#include <wrl.h>
#include <unordered_map>
#include "RendererComponent.h"
using Microsoft::WRL::ComPtr;

class Entity;

class Scene
{
private:
	std::vector<std::shared_ptr<Entity>> entities;
    std::unordered_map<std::string, std::vector<RendererComponent*>> rendererCache; // Кэш для всех Renderer-компонентов, разделенный по типу
public:
    void AddEntity(std::shared_ptr<Entity> entity);

    void RemoveEntity(const Entity* entity);

    void Update(float deltaTime);

    void Render(ComPtr<ID3D12GraphicsCommandList> commandList);

private:
    void UpdateRenderCache(Entity* entity);

    void ClearRenderCache(const Entity* entity);
};

