#pragma once
#include "Component.h"
#include "IUpdatable.h"

class TransformComponent : public Component, public IUpdatable
{
public:
    TransformComponent(float x = 0.0f, float y = 0.0f, float z = 0.0f);
    void Update(float deltaTime) override;
    std::string GetType() const override { return "Transform"; }
private:
    float x, y, z;
};

