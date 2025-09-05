#pragma once
#include "Window.h"
#include <memory>
#include "InputDevice.h"

class NeneEngine {
public:
    NeneEngine();
    ~NeneEngine();

    void Initialize();
    void Update(float deltaTime);
    void Render();
    void Shutdown();

    void OnWindowResized(int width, int height);
    void OnKeyPressed(int key);
    void OnMouseMoved(int x, int y);

    void Run();
    std::shared_ptr<Window> GetWindow() const { return m_window; }

private:
    std::shared_ptr<Window> m_window;
    std::unique_ptr<InputDevice> m_inputDevice;
    bool m_running;
    float m_deltaTime;
};
