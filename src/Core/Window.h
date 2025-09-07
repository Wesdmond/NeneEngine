#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <string>
#include <Windows.h>
#include "../Utility/Delegates.h"

class Window {
public:
    Window();
    ~Window();

    bool Create(const std::string& title, int width, int height);
    void Show();
    void Hide();
    void Close();
    bool ProcessMessages();
    bool ShouldClose() const;

    HWND GetHandle() const { return m_handle; }
    HINSTANCE GetInstanceHandle() const { return m_instance; }
    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }
    void SetResolution(int width, int height) { m_width = width; m_height = height; }

    MulticastDelegate<HWND, UINT, WPARAM, LPARAM> OnWndProc;

private:
    HWND m_handle;
    HINSTANCE m_instance;
    std::string m_title;
    int m_width, m_height;
    bool m_shouldClose;

    bool RegisterWindowClass();
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
};