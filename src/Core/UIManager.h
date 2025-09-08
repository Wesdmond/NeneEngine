#pragma once
#include <windows.h>

class UIManager
{
public:
	UIManager(HWND hWnd);
	void InitImGui();

private:
	HWND g_hWnd = 0;
};

