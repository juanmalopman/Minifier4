
//
// INCLUDES
//

#include "framework.h"
#include "GUI.h"
#include "resource.h"
#include "callbackWNDPROC.h"
#include "printRichEdit.h"
#include "minificationSetup.h"

//
// GLOBAL VARIABLES
//



//
// FUNCTIONS
//

bool checkForReadilyRunningInstance()
{
    // This instance hasn't got a window yet. If FindWindowExW returns a HWND there's another instance running.
    HWND readilyRunningInstance = FindWindowExW(NULL, NULL, mainWindowClass, mainWindowName);
    if (!readilyRunningInstance) return 1;


    
    // Send WM_COPYDATA with this instance's arguments in look for a "TRUE" as response.
    COPYDATASTRUCT payload = { 0 };

    LPWSTR cmdLine = GetCommandLineW(); 

    // + 1 because of the null terminator. sizeof(wchar_t) because cbData expects byte count.
    payload.cbData = (wcslen(cmdLine) + 1) * sizeof(wchar_t);
    payload.lpData = cmdLine;

    // Try to send WM_COPYDATA.
    for (uint8_t i = 0; i < 50 ; i++)
    {
        if (SendMessageW(readilyRunningInstance, WM_COPYDATA, (WPARAM)NULL, (LPARAM)&payload)) return 0; // Forwarded.

        // Wait between attempts.
        Sleep(5);
    }

    errorPopup(L"ERROR: Couldn't forward arguments to a detected readily running instance of " mainWindowName ".");

    // Avoid spamming new instances, close this one.
    return 0;
}

void getDarkModeFunctions(StateGUI* pStateGUI, HMODULE* hUxtheme)
{
    // The method and ordinals are stable since Windows 10 (1809) and are used by major open-source projects like Notepad++.
    // The GUI falls back to light colors seamlessly
    *hUxtheme = LoadLibraryExW(L"uxtheme.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    PFN_SetPreferredAppMode setPreferredAppMode = nullptr; // "Undocumented" function pointer declaration.
    pStateGUI->darkModeApplied = nullptr; // "Undocumented" function pointer declaration.
    if (*hUxtheme)
    {
        FARPROC fp;
        fp = GetProcAddress(*hUxtheme, MAKEINTRESOURCEA(135)); // "Undocumented" function pointer definition.
        memcpy(&setPreferredAppMode, &fp, sizeof(fp));
        fp = GetProcAddress(*hUxtheme, MAKEINTRESOURCEA(133)); // "Undocumented" function pointer definition.
        memcpy(&(pStateGUI->darkModeApplied), &fp, sizeof(fp));
    }
    if (setPreferredAppMode) { setPreferredAppMode(PreferredAppMode_AllowDark); }
}

bool registerMainWindowClass(HINSTANCE hInstance, WNDCLASSEXW* pWc)
{
    pWc->cbSize = sizeof(WNDCLASSEXW);
    pWc->lpfnWndProc = callbackWNDPROC;
    pWc->hInstance = hInstance;
    pWc->lpszClassName = mainWindowClass;
    pWc->hbrBackground = NULL; // This is handled in WM_ERASEBKGND to do without CreateSolidBrush(); and DeleteObject();.
    pWc->hIcon = pWc->hIconSm = LoadIconW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(APP_ICON)); // App icon from resources.rc and resource.h

    // Register the window class
    if (!RegisterClassExW(pWc)) { errorPopup(L"Window Registration Failed!"); return 0; } 

    return 1;
}

void getMonitorDPI(StateGUI* pStateGUI)
{
    pStateGUI->currentDPI = 96; // 96 is the ultimate fallback.

    HMONITOR hMonitor = MonitorFromWindow(NULL, MONITOR_DEFAULTTOPRIMARY);
    UINT dpiX, dpiY;
    if (hMonitor && SUCCEEDED(GetDpiForMonitor(hMonitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY)))
    {
        pStateGUI->currentDPI = dpiX;
    }
    else if ( (dpiX = GetDpiForSystem()) )
    {
        // Fallback for older Windows 10 versions or failure
        // GetDpiForSystem is available in Win 10 1607+
        pStateGUI->currentDPI = dpiX; 
    }    
}

bool createMainWindow(HINSTANCE hInstance, StateGUI* pStateGUI)
{
    int centerHorizontally = (GetSystemMetrics(SM_CXSCREEN) - W_MIN_mainWindow) / 2;
    int centerVertically = (GetSystemMetrics(SM_CYSCREEN) - H_MIN_mainWindow) / 2;
    if (!centerHorizontally) { centerHorizontally = centerVertically = CW_USEDEFAULT; } // Fallback.
    if (!(pStateGUI->hwnds[mainWindow] = CreateWindowW(
        mainWindowClass,
        mainWindowName,
        WS_OVERLAPPEDWINDOW,
        centerHorizontally,
        centerVertically,
        scale(W_MIN_mainWindow, pStateGUI->currentDPI),
        scale(H_MIN_mainWindow, pStateGUI->currentDPI),
        NULL, NULL, hInstance,
        pStateGUI // The wndProc will get access to pStateGUI without making the struct or the hwnds global variables. 
    ))) { errorPopup(L"Main window creation failed!"); return 0; }

    // Make title bar dark.
    BOOL useDarkMode = TRUE;
    DwmSetWindowAttribute(pStateGUI->hwnds[mainWindow], 20, &useDarkMode, sizeof(useDarkMode));

    return 1;
}

bool createRichEditControls(HINSTANCE hInstance, StateGUI* pStateGUI)
{
    // Sizing logic for child controls is inside the WM_SIZE message handling.
    if (!LoadLibraryExW(L"msftedit.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32))
    { 
        errorPopup(L"Rich edit control library failed to load!");
        return 0;
    }
    DWORD richEditStyle = WS_CHILD | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | WS_VISIBLE;
    for (uint8_t i = richEditStart; i < richEditEnd; i++)
    {
        if (i == richEditInput) { richEditStyle &= ~ES_READONLY; } else { richEditStyle |= ES_READONLY; }
        if (!(pStateGUI->hwnds[i] = CreateWindowW(L"RICHEDIT50W", NULL, richEditStyle, 0, 0, 0, 0, pStateGUI->hwnds[mainWindow], NULL, hInstance, NULL)))
        { errorPopup(L"Rich edit control CreateWindowW failed!"); return 0; }
        SendMessageW(pStateGUI->hwnds[i], EM_SETBKGNDCOLOR, 0, (LPARAM)RGB(23, 23, 23));
        setRichEditFormatting(pStateGUI->hwnds[i]);
    }

    return 1;
}

bool createStaticControls(HINSTANCE hInstance, StateGUI* pStateGUI)
{
    // Sizing logic for child controls is inside the WM_SIZE message handling.
    // Sets HMENU (the id of each) to the corresponding enum value.
    static const wchar_t* staticText[] = { L"", L"Input", L"", L"Output", L"", L"Out. File", L"", L"" };
    static_assert( _countof(staticText) == (staticEnd - staticStart), "Count mismatch: Update the staticText array!");
    DWORD staticStyle =  WS_CHILD | WS_VISIBLE | SS_CENTER;
    for (uint8_t i = staticStart; i < staticEnd; i++)
    {
        if ((i - staticStart) % 2)
        {
            staticStyle &= ~SS_BLACKFRAME; // Half the static controls are labels with background.
        }
        else
        {
            staticStyle |= SS_BLACKFRAME; // The other half are frames with lines on the perimeter.
        }
        if (!(pStateGUI->hwnds[i] = CreateWindowW(L"STATIC", staticText[i - staticStart], staticStyle, 0, 0, 0, 0, pStateGUI->hwnds[mainWindow], (HMENU)(uintptr_t)i, hInstance, NULL)))
        { errorPopup(L"Static control CreateWindowW failed!"); return 0; }
        SendMessageW(pStateGUI->hwnds[i], WM_SETFONT, (WPARAM)getDpiAwareFont(pStateGUI->currentDPI), TRUE);
    }

    // Swap Z order, staticBorderForEditControl needs to be on top of staticBackgroundForEditControl.
    SetWindowPos(pStateGUI->hwnds[staticBackgroundForEditControl], pStateGUI->hwnds[staticBorderForEditControl - 1], 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

    return 1;
}

bool createRadioButtonControls(HINSTANCE hInstance, StateGUI* pStateGUI)
{
    // Sizing logic for child controls is inside the WM_SIZE message handling.
    // Sets HMENU (the id of each) to the corresponding enum value.
    static const wchar_t* radioButtonText[] = { L"HTML", L"CSS", L"JS", L"No out. file", L"On stripped path", L"On custom path"};
    static_assert( _countof(radioButtonText) == (radioButtonEnd - radioButtonStart), "Count mismatch: Update the radioButtonText array!" );
    DWORD radioButtonStyle =  WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON;
    for (uint8_t i = radioButtonStart; i < radioButtonEnd; i++)
    {
        if (i == radioButtonHTML || i == radioButtonNoOutFile)
        {
            radioButtonStyle |= WS_GROUP; // Groups radio buttons for selection to be mutually exclusive.
        }
        else
        {
            radioButtonStyle &= ~WS_GROUP;
        }
        if (!(pStateGUI->hwnds[i] = CreateWindowW(L"BUTTON", radioButtonText[i - radioButtonStart], radioButtonStyle, 0, 0, 0, 0, pStateGUI->hwnds[mainWindow], (HMENU)(uintptr_t)i, hInstance, NULL)))
        { errorPopup(L"Radio button control CreateWindowW failed!"); return 0; }
        SendMessageW(pStateGUI->hwnds[i], WM_SETFONT, (WPARAM)getDpiAwareFont(pStateGUI->currentDPI), TRUE);        
    }
    return 1;
}

bool createButtonControls(HINSTANCE hInstance, StateGUI* pStateGUI)
{
    // Sizing logic for child controls is inside the WM_SIZE message handling.
    // Sets HMENU (the id of each) to the corresponding enum value.
    static const wchar_t* buttonText[] = { L"Files...", L"Out. Dir.", L"GO !"};
    static_assert( _countof(buttonText) == (buttonEnd - buttonStart), "Count mismatch: Update the buttonText array!");
    for (uint8_t i = buttonStart; i < buttonEnd; i++)
    {
        if (!(pStateGUI->hwnds[i] = CreateWindowW(L"BUTTON", buttonText[i - buttonStart], WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, pStateGUI->hwnds[mainWindow], (HMENU)(uintptr_t)i, hInstance, NULL)))
        { errorPopup(L"Button control CreateWindowW failed!"); return 0; }
        SendMessageW(pStateGUI->hwnds[i], WM_SETFONT, (WPARAM)getDpiAwareFont(pStateGUI->currentDPI), TRUE);
    }

    return 1;
}

bool createCheckboxControls(HINSTANCE hInstance, StateGUI* pStateGUI)
{
    // Sizing logic for child controls is inside the WM_SIZE message handling.
    // Sets HMENU (the id of each) to the corresponding enum value.
    static const wchar_t* checkboxText[] = { L"Default to prev. HTML", L"Mangle"};
    static_assert( _countof(checkboxText) == (checkboxEnd - checkboxStart), "Count mismatch: Update the checkboxText array!" );
    DWORD checkboxStyle =  WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX;
    for (uint8_t i = checkboxStart; i < checkboxEnd; i++)
    {
        if (!(pStateGUI->hwnds[i] = CreateWindowW(L"BUTTON", checkboxText[i - checkboxStart], checkboxStyle, 0, 0, 0, 0, pStateGUI->hwnds[mainWindow], (HMENU)(uintptr_t)i, hInstance, NULL)))
        { errorPopup(L"Checkbox control CreateWindowW failed!"); return 0; }
        SendMessageW(pStateGUI->hwnds[i], WM_SETFONT, (WPARAM)getDpiAwareFont(pStateGUI->currentDPI), TRUE);
    }
    return 1;
}

bool createEditControls(HINSTANCE hInstance, StateGUI* pStateGUI)
{
    // Sizing logic for child controls is inside the WM_SIZE message handling.
    // Sets HMENU (the id of each) to the corresponding enum value.
    // "You cannot set a cue banner on a multiline edit control or on a rich edit control."
    static const wchar_t* editControlCueBanner[] = { PathStrip_TXT };
    static_assert( _countof(editControlCueBanner) == (editEnd- editStart), "Count mismatch: Update the editControlCueBanner array!");
    for (uint8_t i = editStart; i < editEnd; i++)
    {
        DWORD editControlStyle = WS_CHILD | WS_VISIBLE | ES_CENTER;
        if (!(pStateGUI->hwnds[i] = CreateWindowW(L"EDIT", L"", editControlStyle, 0, 0, 0, 0, pStateGUI->hwnds[mainWindow], (HMENU)(uintptr_t)i, hInstance, NULL)))
        { errorPopup(L"Edit control CreateWindowW failed!"); return 0; }
        SendMessageW(pStateGUI->hwnds[i], WM_SETFONT, (WPARAM)getDpiAwareFont(pStateGUI->currentDPI), TRUE);
        Edit_SetCueBannerText(pStateGUI->hwnds[i], L"Stip Path Segment");
    }

    return 1;
}

void applyDarkModeIfAvailable(StateGUI* pStateGUI)
{
    if (pStateGUI->darkModeApplied != nullptr)
    {
        for (uint8_t i = mainWindow; i < countOfHwnd; i++)
        {
            SetWindowTheme(pStateGUI->hwnds[i], L"DarkMode_Explorer", NULL);
            pStateGUI->darkModeApplied(pStateGUI->hwnds[i], true);
            SendMessageW(pStateGUI->hwnds[i], WM_THEMECHANGED, 0, 0);
        }
    }
}

int initializeGUI(HINSTANCE hInstance, StateGUI* pStateGUI)
{
	// Enable dark theme/dark mode.
    HMODULE hUxtheme = nullptr;
    getDarkModeFunctions(pStateGUI, &hUxtheme);

    // Create window class.
    WNDCLASSEXW wc = { };
    if (!registerMainWindowClass(hInstance, &wc)) return 0;
   
    // Determine the initial DPI of the primary monitor.
    getMonitorDPI(pStateGUI);

    // Create main window.
    if (!createMainWindow(hInstance, pStateGUI)) return 0;
   
    // Create rich edit controls.
    if (!createRichEditControls(hInstance, pStateGUI)) return 0;

    // Create static controls.
    if (!createStaticControls(hInstance, pStateGUI)) return 0;

    // Create radio buttons.
    if (!createRadioButtonControls(hInstance, pStateGUI)) return 0;

    // Create buttons.
    if (!createButtonControls(hInstance, pStateGUI)) return 0;

    // Create checkboxes.
    if (!createCheckboxControls(hInstance, pStateGUI)) return 0;

    // Create edit controls.
    if (!createEditControls(hInstance, pStateGUI)) return 0;

    // Apply dark mode to everything.
    applyDarkModeIfAvailable(pStateGUI);

    // Hide focus rectangle if clicking elements and not navigating with the keyboard.
    SendMessageW(pStateGUI->hwnds[mainWindow], WM_CHANGEUISTATE, MAKEWPARAM(UIS_SET, UISF_HIDEFOCUS), 0);

    // Apply changes dictated by the minifier settings.
    updateMenuSelections(pStateGUI);

    // Show main window. All childs have the WS_VISIBLE flag already.
    ShowWindow(pStateGUI->hwnds[mainWindow], SW_SHOW);

    // Done using the dark mode dll.
    if (hUxtheme) { FreeLibrary(hUxtheme); }

    print(pStateGUI->hwnds[mainWindow], L"Minifier 4 - Juan Manuel López Manzano 2025", TO_CONSOLE);

    print(pStateGUI->hwnds[mainWindow], L"<!DOCTYPE html>\r\n<html>\r\n<head>\r\n    <meta charset='utf-8'>\r\n    "
            "<meta name='viewport' content='width=device-width, initial-scale=1'>\r\n     <title>"
            "Test HTML</title>\r\n</head>\r\n<body>\r\n\r\n</body>\r\n</html>", TO_INPUT);

    return 1;
}

// Get DPI for a specific window.
UINT getWindowDPI(HWND hwnd)
{
    // Define the function pointer type for dynamic loading
    typedef UINT (WINAPI *GetDpiForWindowProc)(HWND);

    // Initialize static variables.
    static GetDpiForWindowProc fnGetDpiForWindow = nullptr;
    static bool checked = false;

    if (!checked) {
        HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
        if (hUser32) {
            // Get the generic pointer from the API
            FARPROC fp = GetProcAddress(hUser32, "GetDpiForWindow");
            
            // Securely copy the address to the typed function pointer
            if (fp)
            {
                memcpy(&fnGetDpiForWindow, &fp, sizeof(fnGetDpiForWindow));
            }
        }
        checked = true;
    }

    if (fnGetDpiForWindow) {
        return fnGetDpiForWindow(hwnd);
    }

    // Fallback: System DPI
    HDC hdc = GetDC(hwnd);
    if (!hdc) return BASE_DPI;
    int dpiY = GetDeviceCaps(hdc, LOGPIXELSY);
    ReleaseDC(hwnd, hdc);
    return (dpiY > 0) ? (UINT)dpiY : BASE_DPI;
}

// Initialize the layout context.
LayoutCtx layoutInit(UINT dpi, int x, int y, int width)
{
    LayoutCtx ctx = {
        .dpi = dpi,
        .x = x,
        .y = y,
        .width = width,
        .gapItem = scale(GAP_small, dpi)
    };
    return ctx;
}

// Place a control and advance the cursor 'Y'.
void layoutPlace(LayoutCtx* ctx, HWND hwnd, int h)
{
    MoveWindow(hwnd, ctx->x, ctx->y, ctx->width, h, TRUE);
    ctx->y += (h + ctx->gapItem);
}

// Calculate the required size for the text in a control.
SIZE calculateControlTextSize(HWND hwnd)
{
    SIZE retVal = { };

    // Calculate buffer size.
    int len = GetWindowTextLength(hwnd);
    if (!len) return retVal;

    // wcscpy_s might need to add a null termination '\0'.
    len += 1;

    // Allocate heap.
    wchar_t* buffer = (wchar_t*)malloc(sizeof(wchar_t) * len);
    if (!buffer) return retVal; // Out of memory.

    GetWindowTextW(hwnd, buffer, len);

    // Get the font currently used by the control
    HFONT hFont = (HFONT)SendMessage(hwnd, WM_GETFONT, 0, 0);
    if (!hFont) return retVal;

    // Prepare the DC for measurement
    HDC hdc = GetDC(hwnd);
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

    RECT rc = { };
    
    // Calculate the rectangle containing text (DT_CALCRECT).
    DrawTextW(hdc, buffer, len, &rc, DT_CALCRECT);

    // Cleanup
    SelectObject(hdc, hOldFont);
    ReleaseDC(hwnd, hdc);

    free(buffer);

    retVal.cx = rc.right;
    retVal.cy = rc.bottom;

    return retVal;
}

// Place a static control with the text of a section. Autosizes based on content.
void layoutLabel(LayoutCtx* ctx, HWND hwnd)
{
    // Calculate exact size required in physical (scaled) pixels.
    SIZE sz = calculateControlTextSize(hwnd);
    
    MoveWindow(hwnd, ctx->x, ctx->y, sz.cx, sz.cy, TRUE);

    // Move Y past the label
    ctx->y += sz.cy; 
}

// Manually add vertical space.
void layoutSpace(LayoutCtx* ctx, int spacePhysical)
{
    ctx->y += spacePhysical;
}

// Autosize and center editPathStrip control.
void layoutEditStrip(StateGUI* pStateGUI, LayoutCtx* ctx)
{
    // All controls except the rich edits share the same font. Get the height.
    SIZE sz = { };
    sz = calculateControlTextSize(pStateGUI->hwnds[radioButtonStart]);

    int hText = sz.cy;

    if (!hText) return;

    // editPathStrip should be centered in staticBackgroundForEditControl. Get it's position and size relative to the screen.
    RECT rc = { };
    GetWindowRect(pStateGUI->hwnds[staticBackgroundForEditControl], &rc);

    // Make position and size relative to the mainWindow.
    const UINT UPDATE_LEFT_TOP_RIGHT_BOTTOM = 4; // Ask MapWindowPoints to update the 4 rc points.
    MapWindowPoints(NULL, pStateGUI->hwnds[mainWindow], (LPPOINT)&rc, UPDATE_LEFT_TOP_RIGHT_BOTTOM); 

    if (!rc.left) return;

    int wStatic = rc.right - rc.left;
    int hStatic = rc.bottom - rc.top;
    int xStatic = rc.left;
    int yStatic = rc.top;

    int wEdit = wStatic - ctx->gapItem * 2; // Avoid overlap with subjacent static control's border.
    int hEdit = hText;
    int xPosEdit = xStatic + (wStatic - wEdit) / 2;
    int yPosEdit = yStatic + (hStatic - hEdit) / 2;
    
    // Make editPathStrip as high as the text and position it in the center of staticBackgroundForEditControl.
    MoveWindow(pStateGUI->hwnds[editPathStrip], xPosEdit, yPosEdit, wEdit, hEdit, TRUE);
}

// Stores and returns the latest font. Deletes old fonts when dpi changes.
HFONT getDpiAwareFont(UINT dpi)
{
    static HFONT hCurrentFont = NULL;
    static UINT  lastDPI = 0;

    if (hCurrentFont && lastDPI == dpi) {
        return hCurrentFont;
    }

    // 1. Create the NEW font first
    NONCLIENTMETRICSW ncm = { .cbSize = sizeof(NONCLIENTMETRICSW) };
    if (!SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICSW), &ncm, 0)) {
        ncm.cbSize -= sizeof(int); 
        SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, ncm.cbSize, &ncm, 0);
    }

    LOGFONTW lf = ncm.lfMessageFont;
    lf.lfHeight = -MulDiv(9, (int)dpi, 72); 
    lf.lfWidth = 0; 
    lf.lfQuality = CLEARTYPE_NATURAL_QUALITY;

    HFONT hNewFont = CreateFontIndirectW(&lf);

    // 2. Only Delete the OLD font if the new one was successfully created
    if (hNewFont)
    {
        if (hCurrentFont) DeleteObject(hCurrentFont);
        hCurrentFont = hNewFont;
        lastDPI = dpi;
    }

    return hCurrentFont;
}

// Called by WM_SIZE to do the whole child controls layout. 
LRESULT sizeControls(StateGUI* pStateGUI, LPARAM lParam)
{
    // Whole main window in physical (scaled) pixels.
    int wClient = LOWORD(lParam);
    int hClient = HIWORD(lParam);

    // currentDPI is updated inside WM_DPICHANGED.
    UINT dpi = pStateGUI->currentDPI;

    // Calculate physical dimensions.
    int gap = scale(GAP_normal, dpi);
    int smallGap = scale(GAP_small, dpi);
    int wMenu  = scale(W_rightMenu, dpi);
    int wCtrl  = wMenu - (smallGap * 2);
    int wRich = wClient - wMenu - (gap * 3); // Account for left and right rich gap plus right menu gap.
    int hRadio = scale(H_radio, dpi);
    int hButton = scale(H_button, dpi);

    // Calculate 'X' positions.
    int xMenu  = wClient - gap - wMenu;
    int xCtrl  = xMenu + smallGap; // Controls indented inside menu.
    int xRich  = gap;
    
    // Initial 'Y' position of child controls.
    int topMargin = smallGap;

    // Initialize layout struct.
    LayoutCtx ctx = layoutInit(dpi, xRich, topMargin, wRich);

    // Rich Edit Controls.
    {        
        // Account for top and bottom margins.
        int hAvailable = hClient - topMargin - gap;
        
        // Split the remaining space. Each control has one bottom separation gap.
        int numControls = richEditEnd - richEditStart;
        int hRich = hAvailable / numControls - smallGap;

        // Padding calculation. One "gap" all round internal padding.
        RECT rcPad = { gap, gap, wRich - gap, hRich - gap };

        for (uint8_t i = richEditStart; i < richEditEnd; i++)
        {
            layoutPlace(&ctx, pStateGUI->hwnds[i], hRich);

            SendMessage(pStateGUI->hwnds[i], EM_SETRECT, 0, (LPARAM)&rcPad); // Update the padding.
        }
    }

    // Right menu - Section "input".
    {
        ctx = layoutInit(dpi, xCtrl, topMargin, wCtrl);

        layoutLabel(&ctx, pStateGUI->hwnds[staticInputSpacerText]);
        layoutSpace(&ctx, smallGap);

        for (uint8_t i = radioButtonHTML; i <= radioButtonJS; i++)
        {
            layoutPlace(&ctx, pStateGUI->hwnds[i], hRadio);
        }

        layoutPlace(&ctx, pStateGUI->hwnds[buttonFiles], hButton);
        layoutPlace(&ctx, pStateGUI->hwnds[checkboxDefaultToPrev], hRadio);

        // Draw group encasing frame.
        int topFrame = topMargin + gap;
        int hFrame = ctx.y + smallGap - topFrame;
        ctx = layoutInit(dpi, xMenu, topFrame, wMenu);

        layoutPlace(&ctx, pStateGUI->hwnds[staticInputSpacerLine], hFrame);
    }

    // Right menu - Section "output".
    {
        int topOfOutput = ctx.y;

        ctx = layoutInit(dpi, xCtrl, topOfOutput, wCtrl);

        layoutLabel(&ctx, pStateGUI->hwnds[staticOutputSpacerText]);
        layoutSpace(&ctx, smallGap);
        
        layoutPlace(&ctx, pStateGUI->hwnds[checkboxMangle], hRadio);

        // Draw group encasing frame.
        int topFrame = topOfOutput + gap;
        int hFrame = ctx.y + smallGap - topFrame;
        ctx = layoutInit(dpi, xMenu, topFrame, wMenu);

        layoutPlace(&ctx, pStateGUI->hwnds[staticOutputSpacerLine], hFrame);
    }

    // Right menu - Section "out. files".
    {
        int topOfOutFiles = ctx.y;

        ctx = layoutInit(dpi, xCtrl, topOfOutFiles, wCtrl);

        layoutLabel(&ctx, pStateGUI->hwnds[staticOutFileSpacerText]);
        layoutSpace(&ctx, smallGap);

        for (uint8_t i = radioButtonNoOutFile; i <= radioButtonOutFilePath; i++)
        {
            layoutPlace(&ctx, pStateGUI->hwnds[i], hRadio);

            // editPathStrip frame.
            if (i == radioButtonOutFileStrip)
            {
                int yFrameStripCtl = ctx.y;

                // Background and border.
                layoutPlace(&ctx, pStateGUI->hwnds[staticBackgroundForEditControl], hButton);
                ctx.y = yFrameStripCtl; // Restore cursor position to same place.
                layoutPlace(&ctx, pStateGUI->hwnds[staticBorderForEditControl], hButton);

                // Don't draw nested edit control yet.
            }
            
            // buttonOutDir.
            if (i == radioButtonOutFilePath)
            {
                layoutPlace(&ctx, pStateGUI->hwnds[buttonOutDir], hButton);
            }
        }

        // Draw group encasing frame.
        int topFrame = topOfOutFiles + gap;
        int hFrame = ctx.y + smallGap - topFrame;
        ctx = layoutInit(dpi, xMenu, topFrame, wMenu);

        layoutPlace(&ctx, pStateGUI->hwnds[staticOutFileSpacerLine], hFrame);
    }

    // Right menu - buttonGo.
    {
        layoutSpace(&ctx, smallGap);

        layoutPlace(&ctx, pStateGUI->hwnds[buttonGo], hButton);
    }

    // Autosize and position editPathStrip. 
    layoutEditStrip(pStateGUI, &ctx);

    // Repaint
    InvalidateRect(pStateGUI->hwnds[mainWindow], nullptr, TRUE);

    return 0;
}

// Make the correct checkboxes/radio buttons selected and fill in edit controls.
void updateMenuSelections(StateGUI* pStateGUI)
{
    // Update radio buttons selection status. Deselections are automatic.
    PostMessageW(pStateGUI->hwnds[pStateGUI->pMiniCfg->inputType], BM_CLICK, 0, 0);
    PostMessageW(pStateGUI->hwnds[pStateGUI->pMiniCfg->outFile], BM_CLICK, 0, 0);

    // Update checkboxes selection status.
    WPARAM checkState;
    checkState = pStateGUI->pMiniCfg->defaultToPrevFile ? BST_CHECKED : BST_UNCHECKED;
    PostMessageW( pStateGUI->hwnds[checkboxDefaultToPrev], BM_SETCHECK, checkState, 0);
    checkState = pStateGUI->pMiniCfg->mangle ? BST_CHECKED : BST_UNCHECKED;
    PostMessageW( pStateGUI->hwnds[checkboxMangle], BM_SETCHECK, checkState, 0);

    // Update edit controls text.
    SetWindowTextW(pStateGUI->hwnds[editPathStrip], pStateGUI->pMiniCfg->stripSeg);
}