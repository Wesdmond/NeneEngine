#include "NeneEngine.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR pCmdLine, int nShowCmd) {
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