#include "NeneEngine.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR pCmdLine, int nShowCmd) {
    try {
        NeneEngine engine(hInstance);
        engine.Initialize();
        return engine.Run();
    }
    catch (const std::exception& e) {
        MessageBoxA(nullptr, e.what(), "Engine Error", MB_ICONERROR);
        return 1;
    }
}