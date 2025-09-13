#include "UIManager.h"

UIManager::UIManager(HWND hWnd) : g_hWnd(hWnd)
{

}

UIManager::~UIManager()
{
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

void UIManager::InitImGui(ImGui_ImplDX12_InitInfo* init_info)
{
    // Инициализация ImGui контекста
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    // Настройка стиля (опционально)
    ImGui::StyleColorsDark();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Включить управление клавиатурой
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;     // Включить docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;   // (Опционально) Включить мультивьюпорты


    // Инициализация бэкендов ImGui
    ImGui_ImplWin32_Init(g_hWnd); // hWnd - ваше окно Win32
    ImGui_ImplDX12_Init(init_info);

    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 0.0f; // Убрать скругление углов для платформенных окон
        style.Colors[ImGuiCol_WindowBg].w = 1.0f; // Полностью непрозрачный фон
    }

    // Create ImGui device objects (font texture, etc.)
    ImGui_ImplDX12_CreateDeviceObjects();
}

void UIManager::BeginFrame()
{
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void UIManager::Render(ID3D12GraphicsCommandList* cmdList)
{
    ImGui::Render();
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmdList);

    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }
}

void UIManager::RenderImGui(ID3D12GraphicsCommandList* command_list) {
    // Начало нового ImGui кадра
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // Создание полноэкранного DockSpace
    static bool dockspace_open = true;
    static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

    // Настройка флагов окна
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    // Создание окна DockSpace
    ImGui::Begin("DockSpace", &dockspace_open, window_flags);
    ImGui::PopStyleVar(2);

    // Создание DockSpace
    ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);

    // Пример: добавление окон
    ImGui::Begin("Example Window");
    ImGui::Text("This is an example docked window!");
    ImGui::End();

    ImGui::End(); // Завершение окна DockSpace

    // Рендеринг ImGui
    ImGui::Render();
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), command_list); // command_list - ваш ID3D12GraphicsCommandList*

    // (Опционально) Обновление платформенных окон, если включены мультивьюпорты
    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }
}
