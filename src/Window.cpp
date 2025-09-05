#include "Window.h"
#include <windowsx.h>
// #include <hidsdi.h>

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

    if (!window) return DefWindowProc(hWnd, uMsg, wParam, lParam);

    switch (uMsg) {
    case WM_SIZE:
        if (window->OnResize.GetSize() > 0) {
            window->OnResize.Broadcast(LOWORD(lParam), HIWORD(lParam));
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

    case WM_INPUT: {
            // gets buffer size
            UINT size = 0;
            GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER));
            if (size == 0) break;

            // Allocate buffer and receive data
            std::vector<BYTE> buffer(size);
            if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, buffer.data(), &size, sizeof(RAWINPUTHEADER)) == size) {
                RAWINPUT* raw = reinterpret_cast<RAWINPUT*>(buffer.data());

                if (raw->header.dwType == RIM_TYPEKEYBOARD) {
                    // Process keyboard
                    InputDevice::KeyboardInputEventArgs args;
                    args.MakeCode = raw->data.keyboard.MakeCode;
                    args.Flags = raw->data.keyboard.Flags;
                    args.VKey = raw->data.keyboard.VKey;
                    args.Message = raw->data.keyboard.Message;
                    window->OnRawKey.Broadcast(args);
                } else if (raw->header.dwType == RIM_TYPEMOUSE) {
                    // Process mouse
                    InputDevice::RawMouseEventArgs args;
                    args.Mode = raw->data.mouse.usFlags;  // MOUSE_MOVE_RELATIVE or absolute
                    args.ButtonFlags = raw->data.mouse.usButtonFlags;
                    args.ExtraInformation = raw->data.mouse.ulExtraInformation;
                    args.Buttons = raw->data.mouse.ulButtons;
                    args.WheelDelta = static_cast<short>(raw->data.mouse.usButtonData);  // For mouse wheel
                    args.X = raw->data.mouse.lLastX;
                    args.Y = raw->data.mouse.lLastY;
                    window->OnRawMouse.Broadcast(args);
                }
                // default Proc, not necessary
                PRAWINPUT pRawInput = raw;
                DefRawInputProc(&pRawInput, 1, sizeof(RAWINPUTHEADER));
            }
            return 0;
    }
    }

    

    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}