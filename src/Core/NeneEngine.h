#pragma once
#include "Window.h"
#include "Inputs/InputDevice.h"
#include "NeneApp.h"
#include "Common/GameTimer.h"
#include <memory>


class NeneEngine {
public:
    NeneEngine(HINSTANCE hInstance);

    void Initialize();
    void SetDelegates();
    void CalculateFrameStats();

    int Run();

    int OnWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

private:
    std::shared_ptr<Window> m_window;
    std::shared_ptr<InputDevice> m_inputDevice;
    std::unique_ptr<NeneApp> m_d12App;
    GameTimer m_timer;

    // Application options
    std::string m_title = "Nene Engine";
    HWND m_hWnd;
    HINSTANCE m_hInstance;
    bool m_minimized;
    bool m_maximized;
    bool m_isPaused;
    bool m_resizing;
};