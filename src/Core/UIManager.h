#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "imgui.h"
#include <d3d12.h>
#include <dxgi1_6.h>

class UIManager
{
public:
	UIManager(HWND hWnd);
	~UIManager();
	void InitImGui(ID3D12Device* device, int backBufferCnt, ID3D12DescriptorHeap* srv_desc_heap);
	void RenderImGui(ID3D12GraphicsCommandList* command_list);

private:
	HWND g_hWnd = 0;
};

