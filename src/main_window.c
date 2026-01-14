
//
// DEPENDENCIES
//

#include <windows.h>
#include <stdint.h> // int64_t, uint32_t, etc.
#include <stdio.h>
#include <string.h> // sprintf_s, strlen, etc.
#include <dwmapi.h> // For dark mode title bars. (Library added to CMakeLists.txt).
#include <uxtheme.h> // Dark mode scroll bars and buttons. (Library added to CMakeLists.txt).
#include <richedit.h> // For the rich edit controls.
#include <shellscalingapi.h> // Allows getting monitor DPI to scale the GUI. ("shcore.lib" library added to CMakeLists.txt).
#include "main_window.h"
#include "resource.h"
#include "window_messages.h"
#include "app_logging.h"
#include "minify_config.h"

//
// CONFIGURATION CONSTANTS
//

static constexpr char mainWindowClass[] = "mainWindowClass";
static constexpr UINT BASE_DPI = 96;
// In logical pixels that will get scaled:
static constexpr int W_MIN_mainWindow = 700;
static constexpr int H_MIN_mainWindow = 670;
static constexpr int W_rightMenu = 150;
static constexpr int H_radio = 20;
static constexpr int H_button = 26;
static constexpr int GAP_normal = 10;
static constexpr int GAP_small = 4;

//
// FUNCTIONS
//

bool mainWindowCheckForOtherInstance()
{
    // This instance hasn't got a window yet. If FindWindowExW returns a HWND there's another instance running.
    HWND readilyRunningInstance = FindWindowExA(NULL, NULL, mainWindowClass, MAIN_WINDOW_NAME);
    if (!readilyRunningInstance) return false;


    
    // Send WM_COPYDATA with this instance's arguments in look for a "TRUE" as response.
    COPYDATASTRUCT payload = { };

    PWSTR cmdLine = GetCommandLineW(); 

    // + 1 because of the null terminator. sizeof(wchar_t) because cbData expects byte count.
    payload.cbData = (wcslen(cmdLine) + 1) * sizeof(wchar_t);
    payload.lpData = cmdLine;

    // Try to send WM_COPYDATA.
    for (uint8_t i = 0; i < 50 ; i++)
    {
        if (SendMessageW(readilyRunningInstance, WM_COPYDATA, (WPARAM)NULL, (LPARAM)&payload)) return true; // Forwarded.

        // Wait between attempts.
        Sleep(5);
    }

    appLogError("ERROR: Couldn't forward arguments to a detected readily running instance.");

    // Avoid spamming new instances, close this one.
    return true;
}

static bool internalRegClass(_In_ HINSTANCE hInstance, _Out_ WNDCLASSEXA* pWc)
{
    pWc->cbSize = sizeof(WNDCLASSEXA);
    pWc->hCursor = LoadCursorA(NULL, IDC_ARROW);
    pWc->lpfnWndProc = windowMessagesCallback;
    pWc->hInstance = hInstance;
    pWc->lpszClassName = mainWindowClass;
    pWc->hbrBackground = NULL; // This is handled in WM_ERASEBKGND to do without CreateSolidBrush(); and DeleteObject();.
    pWc->hIcon = pWc->hIconSm = LoadIconA(GetModuleHandleA(NULL), MAKEINTRESOURCEA(IDI_APP_ICON)); // App icon from resources.rc and resource.h

    // Register the window class
    if (!RegisterClassExA(pWc)) { appLogError("Window Registration Failed!"); return 0; } 

    return 1;
}

static void internalGetMonitorDPI(_Inout_ StateGUI* pStateGUI)
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

// Scale logical pixels to physical device pixels.
static inline int internalScale(_In_ int val, _In_ UINT dpi)
{
    return MulDiv(val, (int)dpi, BASE_DPI);
}

static bool internalCreateMainWindow(_In_ HINSTANCE hInstance, _Inout_ StateGUI* pStateGUI)
{
    int centerHorizontally = (GetSystemMetrics(SM_CXSCREEN) - W_MIN_mainWindow) / 2;
    int centerVertically = (GetSystemMetrics(SM_CYSCREEN) - H_MIN_mainWindow) / 2;
    if (!centerHorizontally) { centerHorizontally = centerVertically = CW_USEDEFAULT; } // Fallback.
    if (!(pStateGUI->hwnds[mainWindow] = CreateWindowA(
        mainWindowClass,
        MAIN_WINDOW_NAME,
        WS_OVERLAPPEDWINDOW,
        centerHorizontally,
        centerVertically,
        internalScale(W_MIN_mainWindow, pStateGUI->currentDPI),
        internalScale(H_MIN_mainWindow, pStateGUI->currentDPI),
        NULL, NULL, hInstance,
        pStateGUI // The wndProc will get access to pStateGUI without making the struct or the hwnds global variables. 
    ))) { appLogError("Main window creation failed!"); return 0; }

    // Make title bar dark.
    BOOL useDarkMode = TRUE;
    DwmSetWindowAttribute(pStateGUI->hwnds[mainWindow], 20, &useDarkMode, sizeof(useDarkMode));

    return 1;
}

static bool internalCreateRichEditControls(_In_ HINSTANCE hInstance, _Inout_ StateGUI* pStateGUI)
{
    // Sizing logic for child controls is inside the WM_SIZE message handling.
    if (!LoadLibraryExA("msftedit.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32))
    { 
        appLogError("Rich edit control library failed to load!");
        return 0;
    }
    DWORD richEditStyle = WS_CHILD | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | WS_VISIBLE;
    for (uint8_t i = richEditStart; i < richEditEnd; i++)
    {
        if (i == richEditInput) { richEditStyle &= ~ES_READONLY; } else { richEditStyle |= ES_READONLY; }
        if (!(pStateGUI->hwnds[i] = CreateWindowA("RICHEDIT50W", NULL, richEditStyle, 0, 0, 0, 0, pStateGUI->hwnds[mainWindow], NULL, hInstance, NULL)))
        { appLogError("Rich edit control CreateWindowW failed!"); return 0; }
        SendMessageA(pStateGUI->hwnds[i], EM_SETBKGNDCOLOR, 0, (LPARAM)RGB(23, 23, 23));
        // Text color gets set when written.
    }

    return 1;
}

static bool internalCreateStaticControls(_In_ HINSTANCE hInstance, _Inout_ StateGUI* pStateGUI)
{
    // Sizing logic for child controls is inside the WM_SIZE message handling.
    // Sets HMENU (the id of each) to the corresponding enum value.
    static const char* staticText[] = {"", "Input", "", "Output", "", "Out. File", "", "Settings", "", "", "", "", "", ""};
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
        if (!(pStateGUI->hwnds[i] = CreateWindowA("STATIC", staticText[i - staticStart], staticStyle, 0, 0, 0, 0, pStateGUI->hwnds[mainWindow], (HMENU)(uintptr_t)i, hInstance, NULL)))
        { appLogError("Static control CreateWindowW failed!"); return 0; }
        SendMessageA(pStateGUI->hwnds[i], WM_SETFONT, (WPARAM)mainWindowGetFont(pStateGUI->currentDPI), TRUE);
    }

    // Swap Z order, staticWhateverBorder needs to be on top of staticWhateverpBkgnd.
    SetWindowPos(pStateGUI->hwnds[staticFilenameBkgnd], pStateGUI->hwnds[staticFilenameBorder - 1], 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    SetWindowPos(pStateGUI->hwnds[staticPathStripBkgnd], pStateGUI->hwnds[staticPathStripBorder - 1], 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    SetWindowPos(pStateGUI->hwnds[staticOutDirBkgnd], pStateGUI->hwnds[staticOutDirBorder - 1], 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

    return 1;
}

static bool internalCreateRadioButtonControls(_In_ HINSTANCE hInstance, _Inout_ StateGUI* pStateGUI)
{
    // Sizing logic for child controls is inside the WM_SIZE message handling.
    // Sets HMENU (the id of each) to the corresponding enum value.
    static const char* radioButtonText[] = {".HTML", ".CSS", ".JS", "Auto-detect file ext.", "No out. file", "On stripped path", "On custom path"};
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
        if (!(pStateGUI->hwnds[i] = CreateWindowA("BUTTON", radioButtonText[i - radioButtonStart], radioButtonStyle, 0, 0, 0, 0, pStateGUI->hwnds[mainWindow], (HMENU)(uintptr_t)i, hInstance, NULL)))
        { appLogError("Radio button control CreateWindowW failed!"); return 0; }
        SendMessageA(pStateGUI->hwnds[i], WM_SETFONT, (WPARAM)mainWindowGetFont(pStateGUI->currentDPI), TRUE);        
    }
    return 1;
}

static bool internalCreateButtonControls(_In_ HINSTANCE hInstance, _Inout_ StateGUI* pStateGUI)
{
    // Sizing logic for child controls is inside the WM_SIZE message handling.
    // Sets HMENU (the id of each) to the corresponding enum value.
    static const char* buttonText[] = {"Files...", "Out. Dir.", "Reload", "Save", "GO !"};
    static_assert( _countof(buttonText) == (buttonEnd - buttonStart), "Count mismatch: Update the buttonText array!");
    for (uint8_t i = buttonStart; i < buttonEnd; i++)
    {
        if (!(pStateGUI->hwnds[i] = CreateWindowA("BUTTON", buttonText[i - buttonStart], WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, pStateGUI->hwnds[mainWindow], (HMENU)(uintptr_t)i, hInstance, NULL)))
        { appLogError("Button control CreateWindowW failed!"); return 0; }
        SendMessageA(pStateGUI->hwnds[i], WM_SETFONT, (WPARAM)mainWindowGetFont(pStateGUI->currentDPI), TRUE);
    }

    return 1;
}

static bool internalCreateCheckboxControls(_In_ HINSTANCE hInstance, _Inout_ StateGUI* pStateGUI)
{
    // Sizing logic for child controls is inside the WM_SIZE message handling.
    // Sets HMENU (the id of each) to the corresponding enum value.
    static const char* checkboxText[] = {"Fallback to previous", "Mangle", "Randomize mangling", "Custom filename"};
    static_assert( _countof(checkboxText) == (checkboxEnd - checkboxStart), "Count mismatch: Update the checkboxText array!" );
    DWORD checkboxStyle =  WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX;
    for (uint8_t i = checkboxStart; i < checkboxEnd; i++)
    {
        if (!(pStateGUI->hwnds[i] = CreateWindowA("BUTTON", checkboxText[i - checkboxStart], checkboxStyle, 0, 0, 0, 0, pStateGUI->hwnds[mainWindow], (HMENU)(uintptr_t)i, hInstance, NULL)))
        { appLogError("Checkbox control CreateWindowW failed!"); return 0; }
        SendMessageA(pStateGUI->hwnds[i], WM_SETFONT, (WPARAM)mainWindowGetFont(pStateGUI->currentDPI), TRUE);
    }
    return 1;
}

static bool internalCreateEditControls(_In_ HINSTANCE hInstance, _Inout_ StateGUI* pStateGUI)
{
    // Sizing logic for child controls is inside the WM_SIZE message handling.
    // Sets HMENU (the id of each) to the corresponding enum value.
    // "You cannot set a cue banner on a multiline edit control or on a rich edit control."
    // Edit_SetCueBannerText only supports UTF16/unicode.
    static const wchar_t* editControlCueBanner[] = {L"Out. Filename", L"Stripe PATH Seg.", L"Out. Dir. PATH"};
    static_assert( _countof(editControlCueBanner) == (editEnd- editStart), "Count mismatch: Update the editControlCueBanner array!");
    for (uint8_t i = editStart; i < editEnd; i++)
    {
        DWORD editControlStyle = WS_CHILD | WS_VISIBLE | ES_CENTER | ES_AUTOHSCROLL | WS_TABSTOP;
        if (!(pStateGUI->hwnds[i] = CreateWindowA("EDIT", "", editControlStyle, 0, 0, 0, 0, pStateGUI->hwnds[mainWindow], (HMENU)(uintptr_t)i, hInstance, NULL)))
        { appLogError("Edit control CreateWindowW failed!"); return 0; }
        SendMessageA(pStateGUI->hwnds[i], WM_SETFONT, (WPARAM)mainWindowGetFont(pStateGUI->currentDPI), TRUE);
        Edit_SetCueBannerText(pStateGUI->hwnds[i], editControlCueBanner[i - editStart]);
    }

    return 1;
}

// -----------------------------------------------------------------------------------------------------
// DARK MODE UNDOCUMENTED API LOADER
// -----------------------------------------------------------------------------------------------------
// These ordinals are stable in uxtheme.dll since Windows 10 build 1903 (19H1).
// Microsoft has not officially exported them by name, but they are the standard industry method (used
// by Explorer, Notepad++, etc.) to force Win32 controls into Dark Mode.
//
// Ordinal 135: SetPreferredAppMode(PreferredAppMode appMode)
//              - Replaces the older AllowDarkModeForApp (1809)
//              - Forces the "Dark" classification for the process.
//
// Ordinal 133: AllowDarkModeForWindow(HWND hWnd, BOOL allow)
//              - Applied per-window/control (e.g., Scrollbars, Titlebars).
// -----------------------------------------------------------------------------------------------------
static void internalGetDarkModeFunctions(_In_ StateGUI* pStateGUI, _Out_ HMODULE* hUxtheme)
{
    // The method and ordinals are stable since Windows 10 (1809) and are used by major open-source projects like Notepad++.
    // The GUI falls back to light colors seamlessly
    *hUxtheme = LoadLibraryExA("uxtheme.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
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

static void internalApplyDarkModeIfAvailable(_Inout_ StateGUI* pStateGUI)
{
    if (pStateGUI->darkModeApplied != nullptr)
    {
        for (uint8_t i = mainWindow; i < countOfHwnd; i++)
        {
            SetWindowTheme(pStateGUI->hwnds[i], L"DarkMode_Explorer", NULL);
            pStateGUI->darkModeApplied(pStateGUI->hwnds[i], true);
            SendMessageA(pStateGUI->hwnds[i], WM_THEMECHANGED, 0, 0);
        }
    }
}

bool mainWindowInit(HINSTANCE hInstance, StateGUI* pStateGUI)
{
	// Enable dark theme/dark mode.
    HMODULE hUxtheme = nullptr;
    internalGetDarkModeFunctions(pStateGUI, &hUxtheme);

    // Create window class.
    WNDCLASSEXA wc = { };
    if (!internalRegClass(hInstance, &wc)) return false;
   
    // Determine the initial DPI of the primary monitor.
    internalGetMonitorDPI(pStateGUI);

    // Create main window.
    if (!internalCreateMainWindow(hInstance, pStateGUI)) return false;
   
    // Create rich edit controls.
    if (!internalCreateRichEditControls(hInstance, pStateGUI)) return false;

    // Create static controls.
    if (!internalCreateStaticControls(hInstance, pStateGUI)) return false;

    // Create radio buttons.
    if (!internalCreateRadioButtonControls(hInstance, pStateGUI)) return false;

    // Create buttons.
    if (!internalCreateButtonControls(hInstance, pStateGUI)) return false;

    // Create checkboxes.
    if (!internalCreateCheckboxControls(hInstance, pStateGUI)) return false;

    // Create edit controls.
    if (!internalCreateEditControls(hInstance, pStateGUI)) return false;

    // Apply dark mode to everything.
    internalApplyDarkModeIfAvailable(pStateGUI);

    // Hide focus rectangle if clicking elements and not navigating with the keyboard.
    SendMessageA(pStateGUI->hwnds[mainWindow], WM_CHANGEUISTATE, MAKEWPARAM(UIS_SET, UISF_HIDEFOCUS), 0);

    // Apply changes dictated by the minifier settings.
    mainWindowUpdateControls(pStateGUI);

    // Show main window. All childs have the WS_VISIBLE flag already.
    ShowWindow(pStateGUI->hwnds[mainWindow], SW_SHOW);

    // Done using the dark mode dll.
    if (hUxtheme) { FreeLibrary(hUxtheme); }

    // Tell app_logging.c what the main window handle is.
    appLogPrintSetup(pStateGUI->hwnds[mainWindow]);

    // Flush the pending messages now that there's a valid handle for the main app in the app_logging module. Only way to get the messages up to now.
    appLogPrint(nullptr, APP_LOG_TO_CONSOLE);

    appLogPrint("(Output)", APP_LOG_TO_OUTPUT);

    return true;
}

// Initialize the layout context.
static LayoutCtx internalLayoutInit(_In_ UINT dpi, _In_ int x, _In_ int y, _In_ int width)
{
    LayoutCtx ctx = {
        .dpi = dpi,
        .x = x,
        .y = y,
        .width = width,
        .gapItem = internalScale(GAP_small, dpi)
    };
    return ctx;
}

// Place a control and advance the cursor 'Y'.
static void internalLayoutPlace(_In_ LayoutCtx* pCtx, _In_ HWND hwnd, _In_ int h)
{
    MoveWindow(hwnd, pCtx->x, pCtx->y, pCtx->width, h, TRUE);
    pCtx->y += (h + pCtx->gapItem);
}

// Calculate the required size for the text in a control.
static SIZE internalCalculateControlTextSize(_In_ HWND hwnd)
{
    SIZE retVal = { };

    // Calculate buffer size.
    int len = GetWindowTextLength(hwnd);
    if (!len) return retVal;

    // strcpy_s might need to add a null termination '\0'.
    len += 1;

    // Allocate heap.
    char* buffer = malloc(len);
    if (!buffer) return retVal; // Out of memory.

    GetWindowTextA(hwnd, buffer, len);

    // Get the font currently used by the control
    HFONT hFont = (HFONT)SendMessage(hwnd, WM_GETFONT, 0, 0);
    if (!hFont) return retVal;

    // Prepare the DC for measurement
    HDC hdc = GetDC(hwnd);
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

    RECT rc = { };
    
    // Calculate the rectangle containing text (DT_CALCRECT).
    DrawTextA(hdc, buffer, len, &rc, DT_CALCRECT);

    // Cleanup
    SelectObject(hdc, hOldFont);
    ReleaseDC(hwnd, hdc);

    free(buffer);

    retVal.cx = rc.right;
    retVal.cy = rc.bottom;

    return retVal;
}

// Place a static control with the text of a section. Autosizes based on content.
static void internalLayoutLabel(_In_ LayoutCtx* pCtx, _In_ HWND hwnd)
{
    // Calculate exact size required in physical (scaled) pixels.
    SIZE sz = internalCalculateControlTextSize(hwnd);
    
    MoveWindow(hwnd, pCtx->x, pCtx->y, sz.cx, sz.cy, TRUE);

    // Move Y past the label
    pCtx->y += sz.cy; 
}

// Manually add vertical space.
static void internalLayoutSpace(_In_ LayoutCtx* pCtx, _In_ int spacePhysical)
{
    pCtx->y += spacePhysical;
}

// Autosize and center editPathStrip control.
static void internalLayoutEditControl(_In_ HWND editControl, _In_ HWND backgroundStaticControl, _In_ StateGUI* pStateGUI, _In_ LayoutCtx* pCtx)
{
    // All controls except the rich edits share the same font. Get the height.
    SIZE sz = { };
    sz = internalCalculateControlTextSize(pStateGUI->hwnds[radioButtonStart]);

    int hText = sz.cy;

    if (!hText) return;

    // Edit controls get centered in a static control serving as background. Get it's position and size relative to the screen.
    RECT rc = { };
    GetWindowRect(backgroundStaticControl, &rc);

    // Make position and size relative to the mainWindow.
    const UINT UPDATE_LEFT_TOP_RIGHT_BOTTOM = 4; // Ask MapWindowPoints to update the 4 rc points.
    MapWindowPoints(NULL, pStateGUI->hwnds[mainWindow], (LPPOINT)&rc, UPDATE_LEFT_TOP_RIGHT_BOTTOM); 

    if (!rc.left) return;

    int wStatic = rc.right - rc.left;
    int hStatic = rc.bottom - rc.top;
    int xStatic = rc.left;
    int yStatic = rc.top;

    int wEdit = wStatic - pCtx->gapItem * 2; // Avoid overlap with subjacent static control's border.
    int hEdit = hText;
    int xPosEdit = xStatic + (wStatic - wEdit) / 2;
    int yPosEdit = yStatic + (hStatic - hEdit) / 2;
    
    // Make editPathStrip as high as the text and position it in the center of staticPathStripBkgnd.
    MoveWindow(editControl, xPosEdit, yPosEdit, wEdit, hEdit, TRUE);
}

// Stores and returns the latest font. Deletes old fonts when dpi changes.
HFONT mainWindowGetFont(UINT dpi)
{
    static HFONT hCurrentFont = NULL;
    static UINT  lastDPI = 0;

    if (hCurrentFont && lastDPI == dpi) {
        return hCurrentFont;
    }

    // 1. Create the NEW font first
    NONCLIENTMETRICSA ncm = { .cbSize = sizeof(NONCLIENTMETRICSA) };
    if (!SystemParametersInfoA(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICSA), &ncm, 0)) {
        ncm.cbSize -= sizeof(int); 
        SystemParametersInfoA(SPI_GETNONCLIENTMETRICS, ncm.cbSize, &ncm, 0);
    }

    LOGFONTA lf = ncm.lfMessageFont;
    lf.lfHeight = -MulDiv(9, (int)dpi, 72); 
    lf.lfWidth = 0; 
    lf.lfQuality = CLEARTYPE_NATURAL_QUALITY;

    HFONT hNewFont = CreateFontIndirectA(&lf);

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
LRESULT mainWindowSizing(StateGUI* pStateGUI, LPARAM lParam)
{
    // Whole main window in physical (scaled) pixels.
    int wClient = LOWORD(lParam);
    int hClient = HIWORD(lParam);

    // currentDPI is updated inside WM_DPICHANGED.
    UINT dpi = pStateGUI->currentDPI;

    // Calculate physical dimensions.
    int gap = internalScale(GAP_normal, dpi);
    int smallGap = internalScale(GAP_small, dpi);
    int wMenu  = internalScale(W_rightMenu, dpi);
    int wCtrl  = wMenu - (smallGap * 2);
    int wRich = wClient - wMenu - (gap * 3); // Account for left and right rich gap plus right menu gap.
    int hRadio = internalScale(H_radio, dpi);
    int hButton = internalScale(H_button, dpi);

    // Calculate 'X' positions.
    int xMenu  = wClient - gap - wMenu;
    int xCtrl  = xMenu + smallGap; // Controls indented inside menu.
    int xRich  = gap;
    
    // Initial 'Y' position of child controls.
    int topMargin = smallGap;

    // Initialize layout struct.
    LayoutCtx ctx = internalLayoutInit(dpi, xRich, topMargin, wRich);

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
            internalLayoutPlace(&ctx, pStateGUI->hwnds[i], hRich);

            SendMessage(pStateGUI->hwnds[i], EM_SETRECT, 0, (LPARAM)&rcPad); // Update the padding.
        }
    }

    // Right menu - Section "input".
    {
        ctx = internalLayoutInit(dpi, xCtrl, topMargin, wCtrl);

        internalLayoutLabel(&ctx, pStateGUI->hwnds[staticInputSpacerText]);
        internalLayoutSpace(&ctx, smallGap);

        for (uint8_t i = radioButtonHTML; i <= radioButtonAutodetect; i++)
        {
            internalLayoutPlace(&ctx, pStateGUI->hwnds[i], hRadio);
        }

        internalLayoutPlace(&ctx, pStateGUI->hwnds[buttonFiles], hButton);
        internalLayoutPlace(&ctx, pStateGUI->hwnds[checkboxFallbackToPrev], hRadio);

        // Draw group encasing frame.
        int topFrame = topMargin + gap;
        int hFrame = ctx.y + smallGap - topFrame;
        ctx = internalLayoutInit(dpi, xMenu, topFrame, wMenu);

        internalLayoutPlace(&ctx, pStateGUI->hwnds[staticInputSpacerLine], hFrame);
    }

    // Right menu - Section "output".
    {
        int topOfOutput = ctx.y;

        ctx = internalLayoutInit(dpi, xCtrl, topOfOutput, wCtrl);

        internalLayoutLabel(&ctx, pStateGUI->hwnds[staticOutputSpacerText]);
        internalLayoutSpace(&ctx, smallGap);
        
        internalLayoutPlace(&ctx, pStateGUI->hwnds[checkboxMangle], hRadio);

        internalLayoutPlace(&ctx, pStateGUI->hwnds[checkboxRandomMangle], hRadio);

        // Draw group encasing frame.
        int topFrame = topOfOutput + gap;
        int hFrame = ctx.y + smallGap - topFrame;
        ctx = internalLayoutInit(dpi, xMenu, topFrame, wMenu);

        internalLayoutPlace(&ctx, pStateGUI->hwnds[staticOutputSpacerLine], hFrame);
    }

    // Right menu - Section "out. files".
    {
        int topOfOutFiles = ctx.y;

        ctx = internalLayoutInit(dpi, xCtrl, topOfOutFiles, wCtrl);

        internalLayoutLabel(&ctx, pStateGUI->hwnds[staticOutFileSpacerText]);
        internalLayoutSpace(&ctx, smallGap);

        internalLayoutPlace(&ctx, pStateGUI->hwnds[checkboxFilename], hRadio);

        {
            int yFrameStripCtl = ctx.y;
            // Background and border.
            internalLayoutPlace(&ctx, pStateGUI->hwnds[staticFilenameBkgnd], hButton);
            ctx.y = yFrameStripCtl; // Restore cursor position to same place.
            internalLayoutPlace(&ctx, pStateGUI->hwnds[staticFilenameBorder], hButton);
        }

        for (uint8_t i = radioButtonNoOutFile; i <= radioButtonOutFilePath; i++)
        {
            internalLayoutPlace(&ctx, pStateGUI->hwnds[i], hRadio);

            // editPathStrip frame.
            if (i == radioButtonOutFileStrip)
            {
                int yFrameStripCtl = ctx.y;

                // Background and border.
                internalLayoutPlace(&ctx, pStateGUI->hwnds[staticPathStripBkgnd], hButton);
                ctx.y = yFrameStripCtl; // Restore cursor position to same place.
                internalLayoutPlace(&ctx, pStateGUI->hwnds[staticPathStripBorder], hButton);

                // Don't draw nested edit control yet.
            }
            
            // buttonOutDir.
            if (i == radioButtonOutFilePath)
            {

                int yFrameOutDirCtl = ctx.y;

                // Background and border.
                internalLayoutPlace(&ctx, pStateGUI->hwnds[staticOutDirBkgnd], hButton);
                ctx.y = yFrameOutDirCtl; // Restore cursor position to same place.
                internalLayoutPlace(&ctx, pStateGUI->hwnds[staticOutDirBorder], hButton);

                // Don't draw nested edit control yet.

                internalLayoutPlace(&ctx, pStateGUI->hwnds[buttonOutDir], hButton);
            }
        }

        // Draw group encasing frame.
        int topFrame = topOfOutFiles + gap;
        int hFrame = ctx.y + smallGap - topFrame;
        ctx = internalLayoutInit(dpi, xMenu, topFrame, wMenu);

        internalLayoutPlace(&ctx, pStateGUI->hwnds[staticOutFileSpacerLine], hFrame);
    }

    // Right menu - Section "settings".
    {
        int topOfSettings = ctx.y;

        ctx = internalLayoutInit(dpi, xCtrl, topOfSettings, wCtrl);

        internalLayoutLabel(&ctx, pStateGUI->hwnds[staticSettingsSpacerText]);
        internalLayoutSpace(&ctx, smallGap);

        internalLayoutPlace(&ctx, pStateGUI->hwnds[buttonLoad], hButton);
        internalLayoutPlace(&ctx, pStateGUI->hwnds[buttonSave], hButton);

        // Draw group encasing frame.
        int topFrame = topOfSettings + gap;
        int hFrame = ctx.y + smallGap - topFrame;
        ctx = internalLayoutInit(dpi, xMenu, topFrame, wMenu);

        internalLayoutPlace(&ctx, pStateGUI->hwnds[staticSettingsSpacerLine], hFrame);

    }

    // Right menu - buttonGo.
    {
        internalLayoutSpace(&ctx, smallGap);

        internalLayoutPlace(&ctx, pStateGUI->hwnds[buttonGo], hButton);
    }

    // Autosize and position editFilename. 
    internalLayoutEditControl(pStateGUI->hwnds[editFilename], pStateGUI->hwnds[staticFilenameBkgnd], pStateGUI, &ctx);

    // Autosize and position editPathStrip. 
    internalLayoutEditControl(pStateGUI->hwnds[editPathStrip], pStateGUI->hwnds[staticPathStripBkgnd], pStateGUI, &ctx);

    // Autosize and position editOutDir. 
    internalLayoutEditControl(pStateGUI->hwnds[editOutDir], pStateGUI->hwnds[staticOutDirBkgnd], pStateGUI, &ctx);

    // Repaint
    InvalidateRect(pStateGUI->hwnds[mainWindow], NULL, TRUE);

    return 0;
}

// Replace entire specified rich edit control contents.
void mainWindowReplaceRichText( HWND hWnd, char* message)
{
    if (hWnd)
    {
        // Replace whole content.
        CHARRANGE cr = { 0, -1 };
        SendMessageA(hWnd, EM_EXSETSEL, 0, (LPARAM)&cr);

        appLogSetFormatting(hWnd);
        SendMessageA(hWnd, EM_REPLACESEL, TRUE, (LPARAM)message);
    }
}

// Make the correct checkboxes/radio buttons selected and fill in edit controls.
void mainWindowUpdateControls(StateGUI* pStateGUI)
{
    // Update radio buttons selection status. Deselections are automatic.
    PostMessageA(pStateGUI->hwnds[pStateGUI->pMiniCfg->inputType], BM_CLICK, 0, 0);
    PostMessageA(pStateGUI->hwnds[pStateGUI->pMiniCfg->outOpt], BM_CLICK, 0, 0);

    // Update checkboxes selection status.
    WPARAM checkState;
    checkState = pStateGUI->pMiniCfg->fallbackToPrevFile ? BST_CHECKED : BST_UNCHECKED;
    PostMessageA( pStateGUI->hwnds[checkboxFallbackToPrev], BM_SETCHECK, checkState, 0);
    checkState = pStateGUI->pMiniCfg->mangle ? BST_CHECKED : BST_UNCHECKED;
    PostMessageA( pStateGUI->hwnds[checkboxMangle], BM_SETCHECK, checkState, 0);
    checkState = pStateGUI->pMiniCfg->randomMangle ? BST_CHECKED : BST_UNCHECKED;
    PostMessageA( pStateGUI->hwnds[checkboxRandomMangle], BM_SETCHECK, checkState, 0);
    checkState = pStateGUI->pMiniCfg->outFilename ? BST_CHECKED : BST_UNCHECKED;
    PostMessageA( pStateGUI->hwnds[checkboxFilename], BM_SETCHECK, checkState, 0);

    // Update edit controls text.
    SetWindowTextA(pStateGUI->hwnds[editFilename], pStateGUI->pMiniCfg->outFile);
    SetWindowTextA(pStateGUI->hwnds[editPathStrip], pStateGUI->pMiniCfg->stripSeg);
    SetWindowTextA(pStateGUI->hwnds[editOutDir], pStateGUI->pMiniCfg->outPath);

    // Update input rich edit control.
    if (!pStateGUI->pMiniCfg->inPath[0]) return;
    mainWindowReplaceRichText( pStateGUI->hwnds[richEditInput], pStateGUI->pMiniCfg->inPath);
}

void mainWindowEnableControls(StateGUI* pStateGUI, bool enable)
{
    // When parsing, right menu controls need to be disabled. When done, reenabled.
    for (int i = richEditEnd; i < countOfHwnd; i++)
    {
        // Possibly skip enabling checkboxRandomMangle.
        if (i == checkboxRandomMangle && enable && !IsDlgButtonChecked(pStateGUI->hwnds[mainWindow], checkboxMangle)) continue;
        EnableWindow(GetDlgItem(pStateGUI->hwnds[mainWindow], i), enable);
    }
}

// Changing radio buttons text color while keeping the dark theme can't be done handling WM_CTLCOLORBTN.
// We need to handle CDDS_PREPAINT, do the text painting, and return CDRF_SKIPDEFAULT.
// Some information online suggest CDRF_SKIPDEFAULT would skip drawing the radio button.
// This is either untrue or valid on old versions of windows. Tested on Win 11 25H2.
// If no text is manually drawn in CDDS_PREPAINT before returning CDRF_SKIPDEFAULT, the radio button is not drawn either.
LRESULT mainWindowRadioBtnCustomDraw(LPARAM lParam, StateGUI* pStateGUI)
{
    if (((NMCUSTOMDRAW*)lParam)->dwDrawStage != CDDS_PREPAINT) { return MAIN_WINDOW_CDRF_NOTHANDLED; }
    if (pStateGUI->darkModeApplied == nullptr) { return MAIN_WINDOW_CDRF_NOTHANDLED; }

    NMCUSTOMDRAW* pNMCD = (LPNMCUSTOMDRAW)lParam;
    SetTextColor(pNMCD->hdc, MAIN_WINDOW_WHITE_TXT); // White text
    RECT rc = pNMCD->rc;
    rc.left += internalScale(GAP_normal * 2, pStateGUI->currentDPI);
    CHAR lpString[MAX_PATH];
    GetWindowTextA(pStateGUI->hwnds[((NMHDR*)lParam)->idFrom], lpString, MAX_PATH);
    DrawTextA(pNMCD->hdc, lpString, -1, &rc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    return CDRF_SKIPDEFAULT;
}

// Prevent users from resizing the main window too small.
LRESULT mainWindowHandleGetMinMaxInfo(LPARAM lParam, StateGUI* pStateGUI)
{
    LPMINMAXINFO lpMMI = (LPMINMAXINFO)lParam;
    lpMMI->ptMinTrackSize.x = internalScale(W_MIN_mainWindow, pStateGUI->currentDPI);
    lpMMI->ptMinTrackSize.y = internalScale(H_MIN_mainWindow, pStateGUI->currentDPI);
    return 0; // Return 0 to tell Windows we handled this message
}

// Create a message-only window for headless mode message handling.
bool mainWindowMsgOnlyWindowInit(_In_ HINSTANCE hInstance,_Inout_ StateGUI* pStateGUI)
{
    // Create window class. Shares same function with the normal GUI execution.
    WNDCLASSEXA wc = { };
    if (!internalRegClass(hInstance, &wc)) return false;

    // Create window with HWND_MESSAGE as the parent handle. pStateGUI passed as lParam in the same way the GUI function would do.
    pStateGUI->hwnds[mainWindow] = CreateWindowExA( 0, mainWindowClass, MAIN_WINDOW_NAME, 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, GetModuleHandle(NULL), pStateGUI);

    // Tell app_logging.c what the main window handle is.
    appLogPrintSetup(pStateGUI->hwnds[mainWindow]);

    // Flush the pending messages now that there's a valid handle for the main app in the app_logging module. Only way to get the messages up to now.
    appLogPrint(nullptr, APP_LOG_TO_CONSOLE);

    return true;
}