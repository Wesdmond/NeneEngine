#include "NeneEngine.h"

#include <iomanip>
#include <iostream>

using namespace std;

NeneEngine::NeneEngine(HINSTANCE hInstance) : m_hInstance(hInstance)
{
    m_window = std::make_shared<Window>();
}

void NeneEngine::OnWindowResized(int width, int height)
{
    m_d12App->SetWindowSize(width, height);
    m_d12App->OnResize();
}

int NeneEngine::OnWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
    case WM_ACTIVATE:
        if (LOWORD(wParam) == WA_INACTIVE)
        {
            m_isPaused = true;
            m_timer.Stop();
        }
        else
        {
            m_isPaused = false;
            m_timer.Start();
        }
        return 0;

        // WM_SIZE is sent when the user resizes the window.  
    case WM_SIZE:
        // Save the new client area dimensions.
        m_d12App->SetWindowSize(LOWORD(lParam), HIWORD(lParam));
        if (m_d12App->CheckIsDeviceInit())
        {
            if (wParam == SIZE_MINIMIZED)
            {
                m_isPaused = true;
                m_minimized = true;
                m_maximized = false;
            }
            else if (wParam == SIZE_MAXIMIZED)
            {
                m_isPaused = false;
                m_minimized = false;
                m_maximized = true;
                m_d12App->OnResize();
            }
            else if (wParam == SIZE_RESTORED)
            {

                // Restoring from minimized state?
                if (m_minimized)
                {
                    m_isPaused = false;
                    m_minimized = false;
                    m_d12App->OnResize();
                }

                // Restoring from maximized state?
                else if (m_maximized)
                {
                    m_isPaused = false;
                    m_maximized = false;
                    m_d12App->OnResize();
                }
                else if (m_resizing)
                {
                    // If user is dragging the resize bars, we do not resize 
                    // the buffers here because as the user continuously 
                    // drags the resize bars, a stream of WM_SIZE messages are
                    // sent to the window, and it would be pointless (and slow)
                    // to resize for each WM_SIZE message received from dragging
                    // the resize bars.  So instead, we reset after the user is 
                    // done resizing the window and releases the resize bars, which 
                    // sends a WM_EXITSIZEMOVE message.
                }
                else // API call such as SetWindowPos or mSwapChain->SetFullscreenState.
                {
                    m_d12App->OnResize();
                }
            }
        }
        return 0;

        // WM_EXITSIZEMOVE is sent when the user grabs the resize bars.
        case WM_ENTERSIZEMOVE:
            m_isPaused = true;
            m_resizing = true;
            m_timer.Stop();
            return 0;

        // WM_EXITSIZEMOVE is sent when the user releases the resize bars.
        // Here we reset everything based on the new window dimensions.
        case WM_EXITSIZEMOVE:
            m_isPaused = false;
            m_resizing = false;
            m_timer.Start();
            m_d12App->OnResize();
            return 0;

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
                m_inputDevice->OnKeyDown(args);
            }
            else if (raw->header.dwType == RIM_TYPEMOUSE) {
                // Process mouse
                InputDevice::RawMouseEventArgs args;
                args.Mode = raw->data.mouse.usFlags;  // MOUSE_MOVE_RELATIVE or absolute
                args.ButtonFlags = raw->data.mouse.usButtonFlags;
                args.ExtraInformation = raw->data.mouse.ulExtraInformation;
                args.Buttons = raw->data.mouse.ulButtons;
                args.WheelDelta = static_cast<short>(raw->data.mouse.usButtonData);  // For mouse wheel
                args.X = raw->data.mouse.lLastX;
                args.Y = raw->data.mouse.lLastY;
                m_inputDevice->OnMouseMove(args);
            }
            // default Proc, not necessary
            PRAWINPUT pRawInput = raw;
            DefRawInputProc(&pRawInput, 1, sizeof(RAWINPUTHEADER));
        }
        return 0;
    }

    // Catch this message so to prevent the window from becoming too small.
    case WM_GETMINMAXINFO:
        ((MINMAXINFO*)lParam)->ptMinTrackSize.x = 256;
        ((MINMAXINFO*)lParam)->ptMinTrackSize.y = 224;
        return 0;
    }
}


void NeneEngine::Initialize() {
    if (!m_window->Create(m_title, 800, 800))
    {
        throw std::runtime_error("Failed to create window");
    }

    m_hWnd = m_window->GetHandle();
    m_hInstance = m_window->GetInstanceHandle();
    m_inputDevice = std::make_unique<InputDevice>(m_hWnd);
    m_d12App = std::make_unique<NeneApp>(m_hInstance, m_hWnd);
    if (!m_d12App->Initialize())
    {
        throw std::runtime_error("Failed to init DX12App");
    }
    SetDelegates();

    m_window->Show();
}

void NeneEngine::SetDelegates()
{
    //m_window->OnWndProc.AddRaw(this, NeneEngine::OnWndProc, HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    m_window->OnWndProc.AddLambda([this](HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        OnWndProc(hWnd, uMsg, wParam, lParam);
        });

    // Test function for cursor test
    m_inputDevice->MouseMove.AddLambda([this](const InputDevice::MouseMoveEventArgs& args) {
        std::cout << "Mouse moved to: (" << args.Position.x << ", " << args.Position.y
            << "), Offset: (" << args.Offset.x << ", " << args.Offset.y
            << "), Wheel: " << args.WheelDelta << "\n";
        });
}

void NeneEngine::CalculateFrameStats()
{
    // Code computes the average frames per second, and also the 
    // average time it takes to render one frame.  These stats 
    // are appended to the window caption bar.
    
    static int frameCnt = 0;
    static float timeElapsed = 0.0f;

    frameCnt++;
    // Compute averages over one-second period.
    if( (m_timer.TotalTime() - timeElapsed) >= 1.0f )
    {
        float fps = (float)frameCnt; // fps = frameCnt / 1
        float mspf = 1000.0f / fps;
        
        std::wstringstream wss;
        wss << std::fixed << std::setprecision(0);
        wss << fps;
        wstring fpsStr = wss.str();
        wss.str(L""); // Reset wstringstream
        wss << std::setprecision(6);
        wss << mspf;
        wstring mspfStr = wss.str();

        wstring windowText = AnsiToWString(m_title) +
            L"   fps: " + fpsStr +
            L"   mspf: " + mspfStr;

        SetWindowText(m_hWnd, windowText.c_str());
		
        // Reset for next average.
        frameCnt = 0;
        timeElapsed += 1.0f;
    }
}

int NeneEngine::Run() {
    MSG msg = { 0 };

    m_timer.Reset();

    while (msg.message != WM_QUIT)
    {
        // If there are Window messages then process them.
        if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        // Otherwise, do animation/game stuff.
        else
        {
            m_timer.Tick();

            if (!m_isPaused)
            {
                CalculateFrameStats();
                m_d12App->Update(m_timer);
                m_d12App->Draw(m_timer);
            }
            else
            {
                Sleep(100);
            }
        }
    }

    return (int)msg.wParam;
}