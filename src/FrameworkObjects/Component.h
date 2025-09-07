#pragma once
#include <string>

// Абстрактный базовый класс Component
class Component
{
public:
    virtual ~Component() = default;
    virtual std::string GetType() const { return "Component"; }
    bool IsEnabled() const { return enabled; }
    void SetEnabled(bool value) { enabled = value; }

private:
    bool enabled = true;
};