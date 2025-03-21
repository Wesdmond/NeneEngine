#pragma once
#include <memory>

class Renderer;
class Scene;
class ResourceManager;
class InputManager;
class UIManager;

class NeneEngine
{
public:
    void Initialize();
    void Run();
    void Shutdown();
private:
    std::unique_ptr<Renderer> renderer;         // Абстрактный рендерер
    std::unique_ptr<Scene> scene;               // Сцена
    std::unique_ptr<ResourceManager> resMgr;    // Менеджер ресурсов
    std::unique_ptr<InputManager> inputMgr;     // Менеджер ввода
    std::unique_ptr<UIManager> uiMgr;           // Менеджер интерфейса (imgui)
};

