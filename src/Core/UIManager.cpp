#include "UIManager.h"
#include "backends/imgui_impl_dx12.h"
#include "backends/imgui_impl_win32.h"

UIManager::UIManager(HWND hWnd) : g_hWnd(hWnd)
{

}

UIManager::~UIManager()
{
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

void UIManager::InitImGui(ID3D12Device* device, int backBufferCnt, ID3D12DescriptorHeap* srv_desc_heap)
{
    // Инициализация ImGui контекста
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Включить управление клавиатурой
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;     // Включить docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;   // (Опционально) Включить мультивьюпорты

    // Настройка стиля (опционально)
    ImGui::StyleColorsDark();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 0.0f; // Убрать скругление углов для платформенных окон
        style.Colors[ImGuiCol_WindowBg].w = 1.0f; // Полностью непрозрачный фон
    }

    // Инициализация бэкендов ImGui
    ImGui_ImplWin32_Init(g_hWnd); // hWnd - ваше окно Win32
    ImGui_ImplDX12_Init(
        device, // ID3D12Device*
        backBufferCnt, // Количество буферов (обычно 2 или 3)
        DXGI_FORMAT_R8G8B8A8_UNORM, // Формат рендер-таргета
        srv_desc_heap, // ID3D12DescriptorHeap* для шейдерных ресурсов
        srv_desc_heap->GetCPUDescriptorHandleForHeapStart(), // CPU handle для шрифтов
        srv_desc_heap->GetGPUDescriptorHandleForHeapStart()  // GPU handle для шрифтов
    );
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
