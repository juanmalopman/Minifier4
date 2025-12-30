
//
// DEPENDENCIES
//

#include <windows.h>
#include <stdint.h> // int64_t, uint32_t, etc.
#include <wchar.h> // swprintf_s, wcslen, etc.
#include <shobjidl.h> // Required for pick folder/file dialogs.
#include "main_window.h"
#include "file_picker.h"
#include "minify_config.h"
#include "app_logging.h"

//
// FUNCTIONS
//

static HRESULT internalInitLibCOM()
{
	// Gets initialized only once even if called multiple times.
    static HRESULT hr = E_FAIL;

    if (hr == E_FAIL)  hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    return hr;
}

static void internalOpenDialog(_In_ StateGUI* pStateGUI, _In_ bool isFile, _Out_writes_z_(MAX_PATH) LPWSTR pathToUpdate)
{
    // Resource initialization.
    IFileOpenDialog *pFileOpen = NULL;
    IShellItem *pItem = NULL;
    PWSTR pszFilePath = NULL;
    
    // Initialize COM library.
    HRESULT hr = internalInitLibCOM();
    
    // If COM init fails, return immediately (nothing to uninitialize).
    if (FAILED(hr)) {  appLogErrorPop(L"ERROR: initializeLibraryCOM() failed."); return; }

    // Execution Loop, single pass, allows 'break' on failure for cleanup.
    do
    {
        // Create the FileOpenDialog object.
        hr = CoCreateInstance(
            &CLSID_FileOpenDialog, 
            NULL, 
            CLSCTX_ALL, 
            &IID_IFileOpenDialog, 
            (void**)&pFileOpen
        );

        if (FAILED(hr)) { appLogErrorPop(L"ERROR: CoCreateInstance() failed."); break; }

        // Get current options.
        DWORD dwOptions;
        hr = IFileOpenDialog_GetOptions(pFileOpen, &dwOptions);
        
        if (FAILED(hr)) { appLogErrorPop(L"ERROR: IFileOpenDialog_GetOptions() failed."); break; }

        // Set options.
        dwOptions |= FOS_FORCEFILESYSTEM;
        if (!isFile) dwOptions |= FOS_PICKFOLDERS;
        hr = IFileOpenDialog_SetOptions(pFileOpen, dwOptions);

        if (FAILED(hr)) { appLogErrorPop(L"ERROR: IFileOpenDialog_SetOptions() failed."); break; }

        // Show the dialog (Modal to owner).
        hr = IFileOpenDialog_Show(pFileOpen, pStateGUI->hwnds[mainWindow]);

        // Special Case: User Cancelled. No error popup.
        if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED)) { break; }

        if (FAILED(hr)) { appLogErrorPop(L"ERROR: IFileOpenDialog_Show() failed."); break; }

        // Get the result (Shell Item)
        hr = IFileOpenDialog_GetResult(pFileOpen, &pItem);

        if (FAILED(hr)) { appLogErrorPop(L"ERROR: IFileOpenDialog_GetResult() failed."); break; }

        // Get the file system path
        hr = IShellItem_GetDisplayName(pItem, SIGDN_FILESYSPATH, &pszFilePath);

        if (FAILED(hr)) { appLogErrorPop(L"ERROR: IShellItem_GetDisplayName() failed."); break; }

        // Success.
        wcscpy_s(pathToUpdate, MAX_PATH, pszFilePath);        
        mainWindowUpdateControls(pStateGUI);

    } while (false); 

    // Centralized Cleanup.
    if (pszFilePath != NULL) CoTaskMemFree(pszFilePath);
    
    if (pItem != NULL) IShellItem_Release(pItem);

    if (pFileOpen != NULL) IFileOpenDialog_Release(pFileOpen);

    CoUninitialize();
}

void filePickerInPath(StateGUI* pStateGUI)
{
    bool isFile = true;
    internalOpenDialog(pStateGUI, isFile, pStateGUI->pMiniCfg->inPath);
}

void filePickerOutPath(StateGUI* pStateGUI)
{
    bool isFile = false;
	internalOpenDialog(pStateGUI, isFile, pStateGUI->pMiniCfg->outPath);
}

