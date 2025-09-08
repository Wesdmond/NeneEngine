#include "UIManager.h"
#include "backends/imgui_impl_dx12.h"
#include "backends/imgui_impl_win32.h"

UIManager::UIManager(HWND hWnd) : g_hWnd(hWnd)
{
}

void UIManager::InitImGui()
{
    // Создание контекста ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(g_hWnd);
    //ImGui_ImplDX12_Init();
}
