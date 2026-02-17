#include "NeneEngine.h"
#include <iostream>

void EnableConsole()
{
    AllocConsole(); // Allocate a console
    FILE* dummy;
    freopen_s(&dummy, "CONOUT$", "w", stdout); // Redirect stdout to console
    std::cout.clear(); // Ensure cout is in a good state
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR pCmdLine, int nShowCmd) {
#if defined(DEBUG) || defined(_DEBUG)
    EnableConsole();
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
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