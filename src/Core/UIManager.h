#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "imgui.h"
#include <d3d12.h>
#include <dxgi1_6.h>
#include "backends/imgui_impl_dx12.h"
#include "backends/imgui_impl_win32.h"

class UIManager
{
public:
	UIManager(HWND hWnd);
	~UIManager();

	void InitImGui(ImGui_ImplDX12_InitInfo* init_info);
	void BeginFrame();
	void Render(ID3D12GraphicsCommandList* cmdList);
	void RenderImGui(ID3D12GraphicsCommandList* command_list);

private:
	HWND g_hWnd = 0;
};

