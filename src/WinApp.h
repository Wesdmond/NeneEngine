#pragma once
#define WIN32_LEAN_AND_MEAN
#include "NeneEngine.h"

class WinApp
{
private:
	NeneEngine* mEngine;
	int width = 1920, height = 1080;

public:
	HWND g_hWnd = 0;
	bool InitWindow(HINSTANCE instHandle, int show);
	static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMessage, WPARAM wParam, LPARAM lParam);
	int Run();
	WinApp();
	~WinApp();

	bool isExitRequested = false;

};

