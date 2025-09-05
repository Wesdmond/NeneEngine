#include <d3d12.h>
#include <dxgi1_6.h>
#include "WinApp.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR pCmdLine,
                   int nShowCmd)
{
    WinApp winApp;

    if (!winApp.InitWindow(hInstance, nShowCmd)) {
        MessageBox(NULL, L"Ошибка DirectX12", L"Ошибка", MB_OK);
        return 0;
    }
    return winApp.Run();

}
