#pragma once
#include <memory>
#include <vector>

#include "Component.h"
#include "RendererComponent.h"
#include "IUpdatable.h"

class Entity
{
private:
	std::vector<std::unique_ptr<Component>> components;
	std::vector<std::pair<Component*, IUpdatable*>> updatables;
	std::vector<RendererComponent*> renderables; // Кэш с Component*
	bool hidden = false;
public:
	void AddComponent(std::unique_ptr<Component> component);
	void Update(float deltaTime);
	void setHidden(bool newHiddenflag) { hidden = newHiddenflag; }
	bool isHidden() const { return hidden; }
	const std::vector<RendererComponent*>& GetRenderables() const { return renderables; }
};

