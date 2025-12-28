
//
// INCLUDES
//

#include "framework.h"
#include "callbackWNDPROC.h"
#include "GUI.h"
#include "printRichEdit.h"
#include "minificationSetup.h"
#include "commonItemDialog.h"

//
// GLOBAL VARIABLES
//


//
// FUNCTIONS
//

void appendToRichEditControl(HWND hWnd, LPARAM lParam)
{
    SendMessageW(hWnd, EM_SETSEL, (WPARAM)-1, (LPARAM)-1);
    SendMessageW(hWnd, EM_REPLACESEL, 0, lParam);
    SendMessageW(hWnd, WM_VSCROLL, SB_BOTTOM, 0);
    free((wchar_t*)lParam); // Free heap.
}


LRESULT CALLBACK callbackWNDPROC(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
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
        appendToRichEditControl(pStateGUI->hwnds[richEditInput], lParam);
        return 0;
    }
    case MSGCUSTOM_PRINTOUTPUT: // --------------------------------------------------------------------- MSGCUSTOM_PRINTOUTPUT
    {
        appendToRichEditControl(pStateGUI->hwnds[richEditOutput], lParam);
        return 0;
    }
    case MSGCUSTOM_PRINTCONSOLE: // -------------------------------------------------------------------- MSGCUSTOM_PRINTCONSOLE
    {
        static volatile LONG currentLine = 0;

        // Thread-Safe. Loop at 99999 messages.
        LONG lineNumberToPrint = (InterlockedIncrement(&currentLine) % 99999);

        // Cast the lParam back to our wchar_t pointer
        wchar_t* pMessage = (wchar_t*)lParam;

        if (wcslen(pMessage) < CONSOLE_PREFIX_LEN)
        {
            free(pMessage); // appendToRichEditControl won't free the heap if we return 0 here.
            return 0;
        }

        // Copy the number as a null terminated array first, and then to lParam/pMessage.
        wchar_t tempNum[CONSOLE_PREFIX_LEN]; 
        swprintf_s(tempNum, CONSOLE_PREFIX_LEN, L"%05d", lineNumberToPrint);
        wmemcpy_s(pMessage, CONSOLE_PREFIX_LEN, tempNum, CONSOLE_NUMBER_LEN);

        appendToRichEditControl(pStateGUI->hwnds[richEditConsole], lParam);
        return 0;
    }
    case WM_COPYDATA: // ------------------------------------------------------------------------------- WM_COPYDATA
    {
        // Arguments forwarded by a recently executed instance before terminating.
        COPYDATASTRUCT* pCds = (COPYDATASTRUCT*)lParam;
        parseArgumentsCLI(pStateGUI, (PWSTR)pCds->lpData);
        return TRUE;
    }
    case WM_COMMAND: // -------------------------------------------------------------------------------- WM_COMMAND
    {
        if (HIWORD(wParam) != BN_CLICKED) break;

        if (LOWORD(wParam) == buttonFiles) chooseInPath(pStateGUI);

        if (LOWORD(wParam) == buttonOutDir) chooseOutPath(pStateGUI);

        if (LOWORD(wParam) == buttonGo) alertPopup(L"buttonGo");

        break;
    }
    case WM_SIZE: // ----------------------------------------------------------------------------------- WM_SIZE
    {
        if (hWnd != pStateGUI->hwnds[mainWindow]) { break; }
        
        return sizeControls(pStateGUI, lParam);
    }
    case WM_DPICHANGED: // ----------------------------------------------------------------------------- WM_DPICHANGED
    {
        // We're not dragging different windows between monitors but using one only window.
        if (hWnd != pStateGUI->hwnds[mainWindow]) { break; }

        // Update window DPI.
        pStateGUI->currentDPI = LOWORD(wParam);

        // Update the Font.
        HFONT hNewFont = getDpiAwareFont(pStateGUI->currentDPI);
        
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
        SetTextColor((HDC)wParam, WHITE_TXT); // Text letters
        SetBkColor((HDC)wParam, RGB(0,0,0)); // Behind text letters
        SetDCBrushColor((HDC)wParam, RGB(0,0,0)); // Background other than behind text
        return (LRESULT)GetStockObject(DC_BRUSH);
    }
    case WM_CTLCOLORBTN: // ---------------------------------------------------------------------------- WM_CTLCOLORBTN
    case WM_CTLCOLORSTATIC: // ------------------------------------------------------------------------- WM_CTLCOLORSTATIC
    {
        // Change radio buttons and buttons background color and text color.
        if (pStateGUI->darkModeApplied == nullptr) { break; }

        SetTextColor((HDC)wParam, WHITE_TXT); // Text letters
        SetBkColor((HDC)wParam, GRAY_BKG); // Behind text letters
        SetDCBrushColor((HDC)wParam, GRAY_BKG); // Background other than behind text

        if ((HWND)lParam == pStateGUI->hwnds[staticPathStripBkgnd])
        {
            SetDCBrushColor((HDC)wParam, RGB(0,0,0)); // Background other than behind text
        }
        return (LRESULT)GetStockObject(DC_BRUSH);
    }
    case WM_NOTIFY: // --------------------------------------------------------------------------------- WM_NOTIFY
    {
        // Changing radio buttons text color while keeping the dark theme can't be done handling WM_CTLCOLORBTN.
        // We need to handle CDDS_PREPAINT, do the text painting, and return CDRF_SKIPDEFAULT.
        // Some information online suggest CDRF_SKIPDEFAULT would skip drawing the radio button.
        // This is either untrue or valid on old versions of windows. Tested on Win 11 25H2.
        // If no text is manually drawn in CDDS_PREPAINT before returning CDRF_SKIPDEFAULT, the radio button is not drawn eiter.

        if (((NMHDR*)lParam)->code != NM_CUSTOMDRAW) { break; }
        if (((NMHDR*)lParam)->idFrom < radioButtonStart || ((NMHDR*)lParam)->idFrom >= radioButtonEnd) { break; }        
        if (((NMCUSTOMDRAW*)lParam)->dwDrawStage != CDDS_PREPAINT) { break; }
        if (pStateGUI->darkModeApplied == nullptr) { break; }

        NMCUSTOMDRAW* pNMCD = (LPNMCUSTOMDRAW)lParam;
        SetTextColor(pNMCD->hdc, WHITE_TXT); // White text
        RECT rc = pNMCD->rc;
        rc.left += scale(GAP_normal * 2, pStateGUI->currentDPI);
        WCHAR wszText[MAX_PATH];
        GetWindowTextW(pStateGUI->hwnds[((NMHDR*)lParam)->idFrom], wszText, MAX_PATH);
        DrawTextW(pNMCD->hdc, wszText, -1, &rc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        return CDRF_SKIPDEFAULT;
    }
    case WM_ERASEBKGND: // ----------------------------------------------------------------------------- WM_ERASEBKGND
    {
        // Make the application background the same color as the title bar.
        if (pStateGUI->darkModeApplied == nullptr) { break; }
        HDC hdc = (HDC)wParam;
        RECT rc;
        GetClientRect(hWnd, &rc);
        SetDCBrushColor(hdc, GRAY_BKG);
        FillRect(hdc, &rc, (HBRUSH)GetStockObject(DC_BRUSH));
        return 1;
    }
    case WM_GETMINMAXINFO: // -------------------------------------------------------------------------- WM_GETMINMAXINFO
    {
        // Prevent users from resizing the window too small.
        LPMINMAXINFO lpMMI = (LPMINMAXINFO)lParam;
        lpMMI->ptMinTrackSize.x = scale(W_MIN_mainWindow, pStateGUI->currentDPI);
        lpMMI->ptMinTrackSize.y = scale(H_MIN_mainWindow, pStateGUI->currentDPI);
        return 0; // Return 0 to tell Windows we handled this message
    }
    case WM_DESTROY: // -------------------------------------------------------------------------------- WM_DESTROY
    {
        // Delete HFONT.
        DeleteObject(getDpiAwareFont(pStateGUI->currentDPI));

        PostQuitMessage(0);
        return 0;
    }
    }
    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}