#pragma once
#include <memory>
#include "Renderer.h"
#include "Scene.h"
#include "UIManager.h"

class NeneEngine
{
public:
    void Initialize();
    void Run();
    void Shutdown();
    void handleWindowMessage(HWND hWnd, UINT uMessage, WPARAM wParam, LPARAM lParam);
private:
    std::unique_ptr<Renderer> renderer;         // Абстрактный рендерер
    std::unique_ptr<Scene> scene;               // Сцена
    //std::unique_ptr<ResourceManager> resMgr;    // Менеджер ресурсов
    //std::unique_ptr<InputManager> inputMgr;     // Менеджер ввода
    std::unique_ptr<UIManager> uiMgr;           // Менеджер интерфейса (imgui)
};

