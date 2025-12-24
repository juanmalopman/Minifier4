
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

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);

    // See if other instance is running.

    // Avoid global variables.
    StateGUI stateGUI = { };

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