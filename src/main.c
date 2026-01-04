
//
// DEPENDENCIES
//

#include <windows.h>
#include <stdint.h> // int64_t, uint32_t, etc.
#include <wchar.h> // swprintf_s, wcslen, etc.
#include <stdio.h> // freopen_s, setvbuf, etc.
#include "main_window.h"
#include "minify_config.h"
#include "app_logging.h"


//
// FUNCTIONS
//

// Helper to bind a C stream (stdout/err/in) to the Windows Console handle.
// If the user redirected output to a file, respect that choice.
static void internalBindStdStream(_In_ DWORD stdHandleType, _Inout_ FILE* stream, _In_ const char* mode)
{
    HANDLE hOs = GetStdHandle(stdHandleType);
    DWORD  type = GetFileType(hOs);

    // If the OS handle is a generic "Character Device", re-open the C stream to
    //"CONOUT$"/"CONIN$" to make printf work (unless the type is DISK (File) or
    // PIPE (Redirection).
    if (type == FILE_TYPE_CHAR || type == FILE_TYPE_UNKNOWN)
    {
        FILE* dummy = nullptr;
        const char* dev = (stdHandleType == STD_INPUT_HANDLE) ? "CONIN$" : "CONOUT$";
        
        // Connect the CRT stream to the console
        freopen_s(&dummy, dev, mode, stream);
        
        // Remove buffering so text appears immediately
        setvbuf(stream, nullptr, _IONBF, 0);
    }
}

[[nodiscard]] 
static bool internalSetupConsoleAttachment()
{
    // Attempt to attach to the parent process's console.
    if (AttachConsole(ATTACH_PARENT_PROCESS))
    {
        // Sync the C Runtime (printf, etc.) with the Operating System Handles.
        // (The OS knows we now have a cosole, but the CRT still doesn't)
        // This runs regardless of whether we attached or allocated.
        internalBindStdStream(STD_OUTPUT_HANDLE, stdout, "w");
        internalBindStdStream(STD_ERROR_HANDLE, stderr, "w");
        internalBindStdStream(STD_INPUT_HANDLE,  stdin,  "r");

        // Set UTF-8 encoding for modern support.
        SetConsoleOutputCP(CP_UTF8);
        return true;
    }
    return false;    
}

int WINAPI wWinMain(_In_ HINSTANCE hInstance, [[maybe_unused]] _In_opt_ HINSTANCE hPrevInstance, [[maybe_unused]] _In_ PWSTR pCmdLine, [[maybe_unused]] _In_ int nCmdShow)
{
    MessageBoxA(NULL, "Test: ★", "UTF-8 Check", MB_OK);
    
    // Try to attach to a console. If we fail, we notify errors as popups and not through printf.
    if (!internalSetupConsoleAttachment()) appLogErrorSetup(false);

    // If other instance is running, forward arguments and exit.
    if (mainWindowCheckForOtherInstance()) return 0;

    appLogPrint(L"Minifier 4 - Juan Manuel López Manzano 2025", APP_LOG_TO_CONSOLE);

    // Load default minification settings.
    MiniCfg miniCfg = { };
    StateGUI stateGUI = { };
    miniCfgLoad(&miniCfg, &stateGUI);

    // If the --headless flag is set, create a message only window, do the conversion and exit.
    if (!miniCfgParseCLI(&miniCfg, nullptr, nullptr))
    {
        if (!mainWindowMsgOnlyWindowInit(hInstance, &stateGUI)){ return 1; }
        // PostMessageW(stateGUI.hwnds[mainWindow], CUSTOM_RUNANDQUIT)...
        // parserCommonRun(&stateGUI);
        // return 0;
    }
    else
    {
        // Create the main window and all the child controls.
        if (!mainWindowInit(hInstance, &stateGUI)){ return 1; }
    }
    
    // Message loop.
    MSG msg = { 0 };
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
} 