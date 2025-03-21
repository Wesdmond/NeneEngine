#include <d3d12.h>
#include <dxgi1_6.h>
#include <windows.h>
#include "NeneEngine.h"
#include "Entity.h"
#include "TransformComponent.h"

int main()
{      
    // Проверка доступности DirectX12
    IDXGIFactory4* factory = nullptr;
    HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&factory));
    if (SUCCEEDED(hr))
    {
        MessageBox(NULL, L"DirectX12 работает!", L"Успех", MB_OK);
        factory->Release();
    }
    else
    {
        MessageBox(NULL, L"Ошибка DirectX12", L"Ошибка", MB_OK);
    }
    return 0;
}