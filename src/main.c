
//
// DEPENDENCIES
//

#include <windows.h>
#include <stdint.h> // int64_t, uint32_t, etc.
#include <wchar.h> // swprintf_s, wcslen, etc.
#include "main_window.h"
#include "minify_config.h"

//
// FUNCTIONS
//

int WINAPI wWinMain(_In_ HINSTANCE hInstance, [[maybe_unused]] _In_opt_ HINSTANCE hPrevInstance, [[maybe_unused]] _In_ PWSTR pCmdLine, [[maybe_unused]] _In_ int nCmdShow)
{
    // If other instance is running, forward arguments and exit. // TODO: Handle forwarding when target is mid-processing.
    if (!mainWindowCheckForOtherInstance()) return 0;

    // Load default minification settings.
    MiniCfg miniCfg = { };
    StateGUI stateGUI = { };
    miniCfgInit(&miniCfg, &stateGUI);

    // If the -noGUI flag is set, do the converion and exit here.
    if (!miniCfgParseCLI(&miniCfg, nullptr, nullptr)) return 0;
    
    // Create the main window and all the child controls.
    if (!mainWindowInit(hInstance, &stateGUI)){ return 1; }
    
    // Message loop.
    MSG msg = { 0 };
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
} 