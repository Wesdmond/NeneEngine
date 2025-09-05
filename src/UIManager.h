#pragma once
#include <windows.h>
#include "backends/imgui_impl_win32.h"

class UIManager
{
public:
	UIManager(HWND hWnd);
	void InitImGui();

private:
	HWND g_hWnd = 0;
};

