#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <string>
#include <Windows.h>
#include "Delegates.h"

class Window {
public:
    Window();
    ~Window();

    bool Create(const std::string& title, int width, int height);
    void Show();
    void Hide();
    void Close();
    void ProcessMessages();
    bool ShouldClose() const;

    HWND GetHandle() const { return m_handle; }
    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }

    // Используем MulticastDelegate вместо std::function
    MulticastDelegate<int, int> OnResize;
    MulticastDelegate<int, bool> OnKey;
    MulticastDelegate<int, int> OnMouseMove;
    MulticastDelegate<bool> OnClose;

private:
    HWND m_handle;
    HINSTANCE m_instance;
    std::string m_title;
    int m_width, m_height;
    bool m_shouldClose;

    void RegisterWindowClass();
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
};
