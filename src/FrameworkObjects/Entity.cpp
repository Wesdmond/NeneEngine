#include "Entity.h"

void Entity::AddComponent(std::unique_ptr<Component> component)
{
    if (auto* render = dynamic_cast<RendererComponent*>(component.get()))
    {
        renderables.push_back(render);
    }
    components.push_back(std::move(component));
}

void Entity::Update(float deltaTime)
{
    for (auto& [component, updatable] : updatables)
    {
        if (component->IsEnabled())
        {
            updatable->Update(deltaTime);
        }
    }
}
