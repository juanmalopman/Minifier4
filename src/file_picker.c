
//
// INCLUDES
//

#include "app_base.h"
#include <shobjidl.h> // Required for pick folder/file dialogs.
#include "main_window.h"
#include "file_picker.h"
#include "minify_config.h"
#include "app_logging.h"

//
// GLOBAL VARIABLES
//


//
// FUNCTIONS
//

HRESULT initializeLibraryCOM()
{
	// Gets initialized only once even if called multiple times.
    static HRESULT hr = E_FAIL;

    if (hr == E_FAIL)  hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    return hr;
}

void openFolderPicker(StateGUI* pStateGUI, bool isFile, LPWSTR pathToUpdate)
{
    // Resource initialization.
    IFileOpenDialog *pFileOpen = nullptr;
    IShellItem *pItem = nullptr;
    PWSTR pszFilePath = nullptr;
    
    // Initialize COM library.
    HRESULT hr = initializeLibraryCOM();
    
    // If COM init fails, return immediately (nothing to uninitialize).
    if (FAILED(hr)) {  errorPopup(L"ERROR: initializeLibraryCOM() failed."); return; }

    // Execution Loop, single pass, allows 'break' on failure for cleanup.
    do
    {
        // Create the FileOpenDialog object.
        hr = CoCreateInstance(
            &CLSID_FileOpenDialog, 
            nullptr, 
            CLSCTX_ALL, 
            &IID_IFileOpenDialog, 
            (void**)&pFileOpen
        );

        if (FAILED(hr)) { errorPopup(L"ERROR: CoCreateInstance() failed."); break; }

        // Get current options.
        DWORD dwOptions;
        hr = IFileOpenDialog_GetOptions(pFileOpen, &dwOptions);
        
        if (FAILED(hr)) { errorPopup(L"ERROR: IFileOpenDialog_GetOptions() failed."); break; }

        // Set options.
        dwOptions |= FOS_FORCEFILESYSTEM;
        if (!isFile) dwOptions |= FOS_PICKFOLDERS;
        hr = IFileOpenDialog_SetOptions(pFileOpen, dwOptions);

        if (FAILED(hr)) { errorPopup(L"ERROR: IFileOpenDialog_SetOptions() failed."); break; }

        // Show the dialog (Modal to owner).
        hr = IFileOpenDialog_Show(pFileOpen, pStateGUI->hwnds[mainWindow]);

        // Special Case: User Cancelled. No error popup.
        if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED)) { break; }

        if (FAILED(hr)) { errorPopup(L"ERROR: IFileOpenDialog_Show() failed."); break; }

        // Get the result (Shell Item)
        hr = IFileOpenDialog_GetResult(pFileOpen, &pItem);

        if (FAILED(hr)) { errorPopup(L"ERROR: IFileOpenDialog_GetResult() failed."); break; }

        // Get the file system path
        hr = IShellItem_GetDisplayName(pItem, SIGDN_FILESYSPATH, &pszFilePath);

        if (FAILED(hr)) { errorPopup(L"ERROR: IShellItem_GetDisplayName() failed."); break; }

        // Success.
        wcscpy_s(pathToUpdate, MAX_PATH, pszFilePath);        
        updateMenuSelections(pStateGUI);

    } while (false); 

    // Centralized Cleanup.
    if (pszFilePath != nullptr) CoTaskMemFree(pszFilePath);
    
    if (pItem != nullptr) IShellItem_Release(pItem);

    if (pFileOpen != nullptr) IFileOpenDialog_Release(pFileOpen);

    CoUninitialize();
}

void chooseInPath(StateGUI* pStateGUI)
{
    bool isFile = true;
    openFolderPicker(pStateGUI, isFile, pStateGUI->pMiniCfg->inPath);
}

void chooseOutPath(StateGUI* pStateGUI)
{
    bool isFile = false;
	openFolderPicker(pStateGUI, isFile, pStateGUI->pMiniCfg->outPath);
}

