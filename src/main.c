
//
// INCLUDES
//

#include "framework.h"
#include "main.h"
#include "GUI.h"
#include "minificationSetup.h"

//
// GLOBAL VARIABLES
//


//
// FUNCTIONS
//

int WINAPI wWinMain(_In_ HINSTANCE hInstance, [[maybe_unused]] _In_opt_ HINSTANCE hPrevInstance, [[maybe_unused]] _In_ PWSTR pCmdLine, [[maybe_unused]] _In_ int nCmdShow)
{
    // If other instance is running, forward arguments and exit. // TODO: Handle forwarding when target is mid-processing.
    if (!checkForReadilyRunningInstance()) return 0;

    // Load default minification settings.
    MiniCfg miniCfg = { };
    StateGUI stateGUI = { };
    loadMinificationSettings(&miniCfg, &stateGUI);

    // If the -noGUI flag is set, do the converion and exit here.
    if (!parseArgumentsCLI(&stateGUI, nullptr)) return 0;
    

    // Create the main window and all the child controls.
    if (!initializeGUI(hInstance, &stateGUI)){ return 1; }

    
    // Message loop.
    MSG msg = { 0 };
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
} 