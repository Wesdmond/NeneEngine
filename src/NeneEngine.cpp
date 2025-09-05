#include "NeneEngine.h"
#include <chrono>
#include <iostream>

NeneEngine::NeneEngine() : m_running(false), m_deltaTime(0.0f) {
    m_window = std::make_shared<Window>();
}

NeneEngine::~NeneEngine()
{
}

void NeneEngine::OnWindowResized(int width, int height) {

}

void NeneEngine::OnKeyPressed(int key)
{
}

void NeneEngine::OnMouseMoved(int x, int y) {

}


void NeneEngine::Initialize() {
    if (!m_window->Create("Nene Engine", 1920, 1080)) {
        throw std::runtime_error("Failed to create window");
    }

    m_inputDevice = std::make_unique<InputDevice>(m_window->GetHandle());

    m_window->OnRawKey.AddLambda([this](InputDevice::KeyboardInputEventArgs args) {
        m_inputDevice->OnKeyDown(args);
    });
    m_window->OnRawMouse.AddLambda([this](InputDevice::RawMouseEventArgs args) {
        m_inputDevice->OnMouseMove(args);
    });

    // Test function for cursor test
    m_inputDevice->MouseMove.AddLambda([this](const InputDevice::MouseMoveEventArgs& args) {
        std::cout << "Mouse moved to: (" << args.Position.x << ", " << args.Position.y
                  << "), Offset: (" << args.Offset.x << ", " << args.Offset.y
                  << "), Wheel: " << args.WheelDelta << "\n";
    });
    
    m_window->OnResize.AddLambda([this](int w, int h) { OnWindowResized(w, h); });
    m_window->OnClose.AddLambda([](bool& canClose) { canClose = true; });

    m_window->Show();
    m_running = true;
}

void NeneEngine::Update(float deltaTime)
{
}

void NeneEngine::Render()
{
}

void NeneEngine::Shutdown()
{
}

void NeneEngine::Run() {
    auto lastTime = std::chrono::high_resolution_clock::now();

    while (m_running && !m_window->ShouldClose()) {
        auto currentTime = std::chrono::high_resolution_clock::now();
        m_deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
        lastTime = currentTime;

        m_window->ProcessMessages();
        Update(m_deltaTime);
        Render();
    }
}