#pragma once
#include "Window.h"
#include "Inputs/InputDevice.h"
#include "NeneApp.h"
#include "Common/GameTimer.h"
#include <memory>


class NeneEngine {
public:
    NeneEngine();

    void Initialize();
    void CalculateFrameStats();

    void OnWindowResized(int width, int height);

    void Run();
    std::shared_ptr<Window> GetWindow() const { return m_window; }

private:
    std::shared_ptr<Window> m_window;
    std::unique_ptr<InputDevice> m_inputDevice;
    std::unique_ptr<NeneApp> m_d12App;
    GameTimer m_timer;
    HWND m_hWnd;
    HINSTANCE m_hInstance;
    std::string m_title = "Nene Engine";
    bool m_running;
    bool m_isPaused;
};