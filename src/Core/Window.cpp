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

bool Window::ProcessMessages() {
    MSG msg;
    if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            m_shouldClose = true;
            return true;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        return true;
    }
    return false;
}

bool Window::RegisterWindowClass() {
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = m_instance;
    wc.hIcon = LoadIcon(nullptr, IDI_WINLOGO);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszMenuName = nullptr;
    wc.lpszClassName = L"EngineWindowClass";
    wc.hIconSm = wc.hIcon;

    return RegisterClassEx(&wc);
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

    if (!window)
    {
        return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }

    if (uMsg == WM_SIZE)
        window->SetResolution(LOWORD(lParam), HIWORD(lParam));

    window->OnWndProc.Broadcast(hWnd, uMsg, wParam, lParam);

    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}