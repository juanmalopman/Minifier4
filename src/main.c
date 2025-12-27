
//
// INCLUDES
//

#include "framework.h"
#include "main.h"
#include "GUI.h"

//
// GLOBAL VARIABLES
//


//
// FUNCTIONS
//

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ [[maybe_unused]] HINSTANCE hPrevInstance, _In_ [[maybe_unused]] PWSTR pCmdLine, _In_ [[maybe_unused]] int nCmdShow)
{
    // If other instance is running, forward arguments and exit. // TODO: Handle forwarding when target is mid-processing.
    if (!checkForReadilyRunningInstance(pCmdLine)) return 0;

    // Avoid global variables for window handles.
    StateAPP stateAPP = { };

    // Create the main window and all the child controls.
    if (!initializeGUI(hInstance, &stateAPP)){ return 1; }

    
    // Message loop.
    MSG msg = { 0 };
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
} 