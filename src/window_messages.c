
//
// DEPENDENCIES
//

#include <windows.h>
#include <stdint.h> // int64_t, uint32_t, etc.
#include <wchar.h> // swprintf_s, wcslen, etc.
#include <vsstyle.h> // Required for BP_CHECKBOX (Redrawing radio buttons).
#include <vssym32.h> // Required for CBS_UNCHECKEDNORMAL (Redrawing radio buttons).
#include <commctrl.h> // Allows some UI controls to be subclassed and some messages handled to change their graphics. (Library added to CMakeLists.txt).
#include "window_messages.h"
#include "main_window.h"
#include "app_logging.h"
#include "minify_config.h"
#include "file_picker.h"
#include "parser_common.h"

//
// FUNCTIONS
//

static void internalAppendToRichEditOrSendToConsole(StateGUI* pStateGUI,_In_ HWND hWnd,_In_  LPARAM lParam)
{
    if (pStateGUI->pMiniCfg->flagHeadless)
    {
        wprintf (L"%s\n", (wchar_t*)lParam);
    }
    else
    {
        SendMessageW(hWnd, EM_SETSEL, (WPARAM)-1, (LPARAM)-1);
        SendMessageW(hWnd, EM_REPLACESEL, 0, lParam);
        SendMessageW(hWnd, EM_SETSEL, (WPARAM)-1, (LPARAM)-1);
        SendMessageW(hWnd, EM_REPLACESEL, 0, (LPARAM)L"\n");
        SendMessageW(hWnd, WM_VSCROLL, SB_BOTTOM, 0);
    }
    free((wchar_t*)lParam); // Free heap.
}


LRESULT CALLBACK windowMessagesCallback(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    StateGUI* pStateGUI = nullptr;

    if (uMsg == WM_NCCREATE)
    {
        // Extract the pointer passed in CreateWindow.
        CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
        pStateGUI = (StateGUI*)pCreate->lpCreateParams;
        
        // Store the pointer to a StateGUI struct in the window instance.
        SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)pStateGUI);
        
        // Ensure the struct knows its own main handle.
        if (pStateGUI) pStateGUI->hwnds[mainWindow] = hWnd;
    }
    else
    {
        // Retrieve pStateGUI pointer for all other messages. 
        pStateGUI = (StateGUI*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    }

    // Guard: If message comes before NCCREATE, or pStateGUI is nullptr, don't try pStateGUI->hwnds[something].
    if (!pStateGUI) return DefWindowProcW(hWnd, uMsg, wParam, lParam);

    switch (uMsg)
    {
    case MSGCUSTOM_PRINTINPUT: // ---------------------------------------------------------------------- MSGCUSTOM_PRINTINPUT
    {
        internalAppendToRichEditOrSendToConsole(pStateGUI,pStateGUI->hwnds[richEditInput], lParam);
        return 0;
    }
    case MSGCUSTOM_PRINTOUTPUT: // --------------------------------------------------------------------- MSGCUSTOM_PRINTOUTPUT
    {
        internalAppendToRichEditOrSendToConsole(pStateGUI,pStateGUI->hwnds[richEditOutput], lParam);
        return 0;
    }
    case MSGCUSTOM_PRINTCONSOLE: // -------------------------------------------------------------------- MSGCUSTOM_PRINTCONSOLE
    {
        static volatile LONG currentLine = 0;

        // Thread-Safe. Loop at 99999 messages.
        LONG lineNumberToPrint = (InterlockedIncrement(&currentLine) % 99999);

        // Cast the lParam back to our wchar_t pointer
        wchar_t* pMessage = (wchar_t*)lParam;

        if (wcslen(pMessage) < APP_LOG_CONSOLE_NUMBER_DIGIT_COUNT + 1)
        {
            free(pMessage); // internalAppendToRichEditControl won't free the heap if we return 0 here.
            return 0;
        }

        // Copy the number as a null terminated array first, and then to lParam/pMessage.
        wchar_t tempNum[APP_LOG_CONSOLE_NUMBER_DIGIT_COUNT + 1]; 
        swprintf_s(tempNum, APP_LOG_CONSOLE_NUMBER_DIGIT_COUNT + 1, L"%05d", lineNumberToPrint);
        wmemcpy_s(pMessage, APP_LOG_CONSOLE_NUMBER_DIGIT_COUNT + 1, tempNum, APP_LOG_CONSOLE_NUMBER_DIGIT_COUNT);

        internalAppendToRichEditOrSendToConsole(pStateGUI,pStateGUI->hwnds[richEditConsole], lParam);
        return 0;
    }
    case WM_COPYDATA: // ------------------------------------------------------------------------------- WM_COPYDATA
    {
        // Arguments forwarded by a recently executed instance before terminating.
        COPYDATASTRUCT* pCds = (COPYDATASTRUCT*)lParam;
        miniCfgParseCLI(pStateGUI->pMiniCfg, pStateGUI, (PWSTR)pCds->lpData);
        return TRUE;
    }
    case WM_COMMAND: // -------------------------------------------------------------------------------- WM_COMMAND
    {
        if (HIWORD(wParam) != EN_CHANGE)
        {
            switch LOWORD(wParam)
            {
            case editFilename: GetDlgItemText(hWnd, editFilename, pStateGUI->pMiniCfg->outFile, MAX_PATH); break;
            case editPathStrip: GetDlgItemText(hWnd, editPathStrip, pStateGUI->pMiniCfg->stripSeg, MAX_PATH); break;
            case editOutDir: GetDlgItemText(hWnd, editOutDir, pStateGUI->pMiniCfg->outPath, MAX_PATH); break;
            }
        }

        if (HIWORD(wParam) != BN_CLICKED) break;

        switch LOWORD(wParam)
        {
        case buttonFiles: filePickerInPath(pStateGUI); break;
        case buttonOutDir: filePickerOutPath(pStateGUI); break;
        case buttonLoad: miniCfgLoad(pStateGUI->pMiniCfg, pStateGUI); break;
        case buttonSave: miniCfgSave(pStateGUI->pMiniCfg); break;
        case buttonGo:
        {
            pStateGUI->pMiniCfg->currentlyParsing = true;
            mainWindowEnableControls(pStateGUI, false); // Disable all controls while parsing.
            parserCommonRun(pStateGUI);
            break;
        }
        case radioButtonHTML: pStateGUI->pMiniCfg->inputType = radioButtonHTML; break;
        case radioButtonCSS: pStateGUI->pMiniCfg->inputType = radioButtonCSS; break;
        case radioButtonJS: pStateGUI->pMiniCfg->inputType = radioButtonJS; break;
        case radioButtonAutodetect: pStateGUI->pMiniCfg->inputType = radioButtonAutodetect; break;
        case radioButtonNoOutFile: pStateGUI->pMiniCfg->outOpt = radioButtonNoOutFile; break;
        case radioButtonOutFileStrip: pStateGUI->pMiniCfg->outOpt = radioButtonOutFileStrip; break;
        case radioButtonOutFilePath: pStateGUI->pMiniCfg->outOpt = radioButtonOutFilePath; break;
        case checkboxFallbackToPrev: pStateGUI->pMiniCfg->fallbackToPrevFile = IsDlgButtonChecked(hWnd, checkboxFallbackToPrev); break;
        case checkboxMangle:
        {
        pStateGUI->pMiniCfg->mangle = IsDlgButtonChecked(hWnd, checkboxMangle);
        EnableWindow(GetDlgItem(hWnd, checkboxRandomMangle), pStateGUI->pMiniCfg->mangle); // Disable Randomize mangling if Mangle is unchecked.
        break;
        }
        case checkboxRandomMangle: pStateGUI->pMiniCfg->randomMangle = IsDlgButtonChecked(hWnd, checkboxRandomMangle); break;
        case checkboxFilename: pStateGUI->pMiniCfg->outFilename = IsDlgButtonChecked(hWnd, checkboxFilename); break;
        }   

        break;
    }
    case WM_SIZE: // ----------------------------------------------------------------------------------- WM_SIZE
    {
        if (hWnd != pStateGUI->hwnds[mainWindow]) { break; }
        
        return mainWindowSizing(pStateGUI, lParam);
    }
    case WM_DPICHANGED: // ----------------------------------------------------------------------------- WM_DPICHANGED
    {
        // We're not dragging different windows between monitors but using one only window.
        if (hWnd != pStateGUI->hwnds[mainWindow]) { break; }

        // Update window DPI.
        pStateGUI->currentDPI = LOWORD(wParam);

        // Update the Font.
        HFONT hNewFont = mainWindowGetFont(pStateGUI->currentDPI);
        
        // Apply font to main window and children
        for (uint8_t i = mainWindow; i < countOfHwnd; i++)
        {
            SendMessage(pStateGUI->hwnds[i], WM_SETFONT, (WPARAM)hNewFont, TRUE);
        }

        // lParam contains a pointer to a RECT with the suggested new size/pos.
        RECT* wndSz = (RECT*)lParam;

        // Resize the window to the suggested rect, triggering WM_SIZE.
        SetWindowPos(hWnd, NULL, wndSz->left, wndSz->top, wndSz->right - wndSz->left, wndSz->bottom - wndSz->top, SWP_NOZORDER | SWP_NOACTIVATE);

        return 0;
    }
    case WM_CTLCOLOREDIT: // --------------------------------------------------------------------------- WM_CTLCOLORBTN
    {
        // Change radio buttons and buttons background color and text color.
        if (pStateGUI->darkModeApplied == nullptr) { break; }
        SetTextColor((HDC)wParam, MAIN_WINDOW_WHITE_TXT); // Text letters
        SetBkColor((HDC)wParam, RGB(0,0,0)); // Behind text letters
        SetDCBrushColor((HDC)wParam, RGB(0,0,0)); // Background other than behind text
        return (LRESULT)GetStockObject(DC_BRUSH);
    }
    case WM_CTLCOLORBTN: // ---------------------------------------------------------------------------- WM_CTLCOLORBTN
    case WM_CTLCOLORSTATIC: // ------------------------------------------------------------------------- WM_CTLCOLORSTATIC
    {
        // Change radio buttons and buttons background color and text color.
        if (pStateGUI->darkModeApplied == nullptr) { break; }

        SetTextColor((HDC)wParam, MAIN_WINDOW_WHITE_TXT); // Text letters
        SetBkColor((HDC)wParam, MAIN_WINDOW_GRAY_BKG); // Behind text letters
        SetDCBrushColor((HDC)wParam, MAIN_WINDOW_GRAY_BKG); // Background other than behind text

        if ((HWND)lParam == pStateGUI->hwnds[staticPathStripBkgnd])
        {
            SetDCBrushColor((HDC)wParam, RGB(0,0,0)); // Background other than behind text
        }
        return (LRESULT)GetStockObject(DC_BRUSH);
    }
    case WM_NOTIFY: // --------------------------------------------------------------------------------- WM_NOTIFY
    {
        // Changing radio buttons text color while keeping the dark theme can't be done handling WM_CTLCOLORBTN.
        if (((NMHDR*)lParam)->code != NM_CUSTOMDRAW) { break; }
        if (((NMHDR*)lParam)->idFrom < radioButtonStart || ((NMHDR*)lParam)->idFrom >= radioButtonEnd) { break; }

        LRESULT lr = mainWindowRadioBtnCustomDraw(lParam, pStateGUI);
        if (lr != MAIN_WINDOW_CDRF_NOTHANDLED) return lr;

        break; 
    }
    case WM_ERASEBKGND: // ----------------------------------------------------------------------------- WM_ERASEBKGND
    {
        // Make the application background the same color as the title bar.
        if (pStateGUI->darkModeApplied == nullptr) { break; }
        HDC hdc = (HDC)wParam;
        RECT rc;
        GetClientRect(hWnd, &rc);
        SetDCBrushColor(hdc, MAIN_WINDOW_GRAY_BKG);
        FillRect(hdc, &rc, (HBRUSH)GetStockObject(DC_BRUSH));
        return 1;
    }
    case WM_GETMINMAXINFO: // -------------------------------------------------------------------------- WM_GETMINMAXINFO
    {
        // Prevent users from resizing the window too small.
        return mainWindowHandleGetMinMaxInfo(lParam, pStateGUI);
    }
    case WM_DESTROY: // -------------------------------------------------------------------------------- WM_DESTROY
    {
        // Delete HFONT.
        DeleteObject(mainWindowGetFont(pStateGUI->currentDPI));

        PostQuitMessage(0);
        return 0;
    }
    }
    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

