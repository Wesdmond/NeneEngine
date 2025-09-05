#include "NeneEngine.h"

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    try {
        NeneEngine engine;
        engine.Initialize();
        engine.Run();
    }
    catch (const std::exception& e) {
        MessageBoxA(nullptr, e.what(), "Engine Error", MB_ICONERROR);
        return 1;
    }

    return 0;
}