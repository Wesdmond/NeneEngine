#include "WinApp.h"

bool WinApp::InitWindow(HINSTANCE instHandle, int show)
{
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WinApp::WndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = instHandle;
    wc.hIcon = LoadIcon(nullptr, IDI_WINLOGO);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszMenuName = nullptr;
    wc.lpszClassName = L"NeneEngine";
    wc.hIconSm = wc.hIcon;

    if (!RegisterClassEx(&wc))
    {
        MessageBox(NULL, L"InitWindow: Не получилось проинициализировать класс окна", L"Ошибка WinApp", MB_OK);
        return false;
    }

    RECT windowRect = { 0, 0, 1920, 1080 };
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hWnd = CreateWindowEx(
        WS_EX_APPWINDOW,
        L"NeneEngine",
        L"NeneEngine",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        windowRect.right - windowRect.left,
        windowRect.bottom - windowRect.top,
        nullptr,
        nullptr,
        instHandle,
        mEngine
    );
    if (!hWnd) {
        MessageBox(NULL, L"InitWindow: CreateWindowEx вернул NULL", L"Ошибка WinApp", MB_OK);
        return false;
    }

    g_hWnd = hWnd;

    ShowWindow(g_hWnd, SW_SHOW);
    UpdateWindow(g_hWnd);

    return true;
}

LRESULT CALLBACK WinApp::WndProc(HWND hWnd, UINT uMessage, WPARAM wParam, LPARAM lParam)
{
    if (uMessage == WM_CREATE)
    {
        const auto* const createStruct = reinterpret_cast<CREATESTRUCT*>(lParam);
        ::SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(createStruct->lpCreateParams));
    }

    switch (uMessage)
    {
    case WM_DESTROY:
    case WM_CLOSE:
    {
        PostQuitMessage(0);
        return 0;
    }
    case WM_KEYDOWN:
    {
        const LONG_PTR ptr = ::GetWindowLongPtr(hWnd, GWLP_USERDATA);
        NeneEngine* engine = reinterpret_cast<NeneEngine*>(ptr);
        engine->handleWindowMessage(hWnd, uMessage, wParam, lParam);
        return 0;
    }
    default:
    {
        return DefWindowProc(hWnd, uMessage, wParam, lParam);
    }
    }

}

int WinApp::Run()
{
    MSG msg = {};
    while (!isExitRequested) {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                isExitRequested = true;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        // Do game specific stuff here
    }
    return (int)msg.wParam;
}

WinApp::WinApp()
{
    mEngine = new NeneEngine();
}

WinApp::~WinApp()
{
    delete(mEngine);
}
