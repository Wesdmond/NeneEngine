#include "Window.h"
#include <windowsx.h>

Window::Window()
    : m_handle(nullptr), m_instance(nullptr), m_width(0), m_height(0), m_shouldClose(false) {
}

Window::~Window() {
    if (m_handle) {
        DestroyWindow(m_handle);
    }
}

bool Window::Create(const std::string& title, int width, int height) {
    m_title = title;
    m_width = width;
    m_height = height;
    m_instance = GetModuleHandle(nullptr);

    RegisterWindowClass();

    RECT rect = { 0, 0, width, height };
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

    m_handle = CreateWindowEx(
        0,
        L"EngineWindowClass",
        std::wstring(title.begin(), title.end()).c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        nullptr,
        nullptr,
        m_instance,
        this
    );

    return m_handle != nullptr;
}

void Window::Show() { ShowWindow(m_handle, SW_SHOW); }
void Window::Hide() { ShowWindow(m_handle, SW_HIDE); }
void Window::Close() { DestroyWindow(m_handle); m_shouldClose = true; }
bool Window::ShouldClose() const { return m_shouldClose; }

void Window::ProcessMessages() {
    MSG msg;
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            m_shouldClose = true;
            break;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

void Window::RegisterWindowClass() {
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L,
                      GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr,
                      L"EngineWindowClass", nullptr };
    RegisterClassEx(&wc);
}

LRESULT CALLBACK Window::WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    Window* window = nullptr;

    if (uMsg == WM_NCCREATE) {
        CREATESTRUCT* create = reinterpret_cast<CREATESTRUCT*>(lParam);
        window = reinterpret_cast<Window*>(create->lpCreateParams);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
    }
    else {
        window = reinterpret_cast<Window*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    }

    if (!window) return DefWindowProc(hWnd, uMsg, wParam, lParam);

    switch (uMsg) {
    case WM_SIZE:
        if (window->OnResize.GetSize() > 0) {
            window->OnResize.Broadcast(LOWORD(lParam), HIWORD(lParam));
        }
        return 0;

    case WM_KEYDOWN:
        if (window->OnKey.GetSize() > 0) {
            window->OnKey.Broadcast(static_cast<int>(wParam), true);
        }
        return 0;

    case WM_KEYUP:
        if (window->OnKey.GetSize() > 0) {
            window->OnKey.Broadcast(static_cast<int>(wParam), false);
        }
        return 0;

    case WM_MOUSEMOVE:
        if (window->OnMouseMove.GetSize() > 0) {
            window->OnMouseMove.Broadcast(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        }
        return 0;

    case WM_CLOSE:
        if (window->OnClose.GetSize() > 0) {
            bool canClose = true;
            window->OnClose.Broadcast(canClose);
            if (canClose) DestroyWindow(hWnd);
            return 0;
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}
