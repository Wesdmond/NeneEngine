#include "TransformComponent.h"



TransformComponent::TransformComponent(float x, float y, float z)
{
	this->x = x;
	this->y = y;
	this->z = z;
}

void TransformComponent::Update(float deltaTime)
{
	// Например, обновление позиции
}
