
//
// INCLUDES
//

#include "framework.h"
#include "GUI.h"
#include "resource.h"
#include "callbackWNDPROC.h"
#include "printRichEdit.h"

//
// GLOBAL VARIABLES
//



//
// FUNCTIONS
//

int initializeGUI(_In_ HINSTANCE hInstance, _In_ StateGUI* pStateGUI)
{
	// Enable dark theme/dark mode.
    pStateGUI->failedToApplyDarkTheme = 1; // Cleared if all windows get properly themed.

    // The method and ordinals are stable since Windows 10 (1809) and are used by major open-source projects like Notepad++.
    // The GUI falls back to light colors seamlessly
    HMODULE hUxtheme = LoadLibraryExW(L"uxtheme.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);

    typedef BOOL (WINAPI *PFN_SetPreferredAppMode)(PreferredAppMode);
    typedef BOOL (WINAPI *PFN_AllowDarkMode)(HWND, BOOL);

    PFN_SetPreferredAppMode setPreferredAppMode = NULL; // "Undocumented" function pointer declaration.
    PFN_AllowDarkMode allowDarkMode = NULL; // "Undocumented" function pointer declaration.

    if (hUxtheme)
    {
        FARPROC fp;
        fp = GetProcAddress(hUxtheme, MAKEINTRESOURCEA(135)); // "Undocumented" function pointer definition.
        memcpy(&setPreferredAppMode, &fp, sizeof(fp));
        fp = GetProcAddress(hUxtheme, MAKEINTRESOURCEA(133)); // "Undocumented" function pointer definition.
        memcpy(&allowDarkMode, &fp, sizeof(fp));
    }
    if (setPreferredAppMode) { setPreferredAppMode(PreferredAppMode_AllowDark); }


    // Create window class
    WNDCLASSEXW wc = { };
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = callbackWNDPROC;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"SimpleWindow";
    wc.hbrBackground = NULL; // This is handled in WM_ERASEBKGND to do without CreateSolidBrush(); and DeleteObject();.
    wc.hIcon = wc.hIconSm = LoadIconW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(APP_ICON)); // App icon from resources.rc and resource.h

    // Register the window class
    if (!RegisterClassExW(&wc)) { MessageBoxW(NULL, L"Window Registration Failed!", L"Error", MB_OK | MB_ICONERROR); return 0; }

    // Create parent window
    const int windowWidth = MAIN_WIN_MIN_WIDTH, windowHeight = MAIN_WIN_MIN_HEIGHT;
    int centerHorizontally = (GetSystemMetrics(SM_CXSCREEN) - windowWidth) / 2;
    int centerVertically = (GetSystemMetrics(SM_CYSCREEN) - windowHeight) / 2;
    if (!centerHorizontally) { centerHorizontally = CW_USEDEFAULT; } // Fallback.
    if (!(pStateGUI->hwnds[mainWindow] = CreateWindowW(
        L"SimpleWindow",
        L"Minifier 4",
        WS_OVERLAPPEDWINDOW,
        centerHorizontally,
        centerVertically,
        windowWidth,
        windowHeight,
        NULL, NULL, hInstance,
        pStateGUI // The wndProc will get access to pStateGUI without making the struct or the hwnds global variables. 
    ))) { MessageBoxW(NULL, L"Main window creation failed!", L"Error", MB_OK | MB_ICONERROR); return 0; }

    // Make title bar dark.
    BOOL useDarkMode = TRUE;
    DwmSetWindowAttribute(pStateGUI->hwnds[mainWindow], 20, &useDarkMode, sizeof(useDarkMode));
   
    // Create rich edit controls. Sizing logic for the rich edit controls and buttons is inside the WM_SIZE message handling.
    if (!LoadLibraryW(L"Msftedit.dll")) { MessageBoxW(NULL, L"Rich edit control library failed to load!", L"Error", MB_OK | MB_ICONERROR); return 0; }
    {
        DWORD richEditStyle = WS_CHILD | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | WS_VISIBLE;
        for (uint8_t i = richEditStart; i < richEditEnd; i++)
        {
            if (i == richEditInput) { richEditStyle &= ~ES_READONLY; } else { richEditStyle |= ES_READONLY; }
            if (!(pStateGUI->hwnds[i] = CreateWindowW(L"RICHEDIT50W", NULL, richEditStyle, 0, 0, 0, 0, pStateGUI->hwnds[mainWindow], NULL, hInstance, NULL)))
            { MessageBoxW(NULL, L"Rich edit control CreateWindowW failed!", L"Error", MB_OK | MB_ICONERROR); return 0; }
            SetWindowTheme(pStateGUI->hwnds[i], L"DarkMode_Explorer", NULL);
            SendMessageW(pStateGUI->hwnds[i], EM_SETBKGNDCOLOR, 0, (LPARAM)RGB(23, 23, 23));
            setRichEditFormatting(pStateGUI->hwnds[i]);
        }
    }

    // Create static controls. Sets HMENU (the id of each) to the corresponding enum value. Sizing logic is inside the WM_SIZE message handling.
    {
        static const wchar_t* staticText[] = { L"", L"Input", L"", L"Output", L"", L"Out. File", L"", L"" };
        static_assert( _countof(staticText) == (staticEnd - staticStart), "Count mismatch: Update the staticText array!");
        DWORD staticStyle =  WS_CHILD | WS_VISIBLE;
        for (uint8_t i = staticStart; i < staticEnd; i++)
        {
            if ((i - staticStart) % 2)
            {
                staticStyle &= ~SS_ETCHEDHORZ; // Half the static controls are text.
                staticStyle |= SS_CENTER; // Background color erasing any line behind.
            }
            else
            {
                staticStyle |= SS_ETCHEDHORZ; // The other half are frames with lines on the perimeter.
                staticStyle &= ~SS_CENTER; // No background color inside.
            }
            if (!(pStateGUI->hwnds[i] = CreateWindowW(L"STATIC", staticText[i - staticStart], staticStyle, 0, 0, 0, 0, pStateGUI->hwnds[mainWindow], (HMENU)(uintptr_t)i, hInstance, NULL)))
            { MessageBoxW(NULL, L"Static control CreateWindowW failed!", L"Error", MB_OK | MB_ICONERROR); return 0; }
            SendMessage(pStateGUI->hwnds[i], WM_SETFONT, (WPARAM)getDefaultUIFont(), TRUE);
        }
    }
    // Swap Z order, staticBorderForEditControl needs to be on top of staticBackgroundForEditControl.
    SetWindowPos(pStateGUI->hwnds[staticBackgroundForEditControl], pStateGUI->hwnds[staticBorderForEditControl - 1], 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

    // Create radio buttons. Sets HMENU (the id of each) to the corresponding enum value. Sizing logic is inside the WM_SIZE message handling.
    {
        static const wchar_t* radioButtonText[] = { L"HTML", L"CSS", L"JS", L"Mangle", L"Keep Names", L"No out. file", L"On stripped path", L"On custom path"};
        static_assert( _countof(radioButtonText) == (radioButtonEnd - radioButtonStart), "Count mismatch: Update the radioButtonText array!" );
        DWORD radioButtonStyle =  WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON;
        for (uint8_t i = radioButtonStart; i < radioButtonEnd; i++)
        {
            if (i == radioButtonHTML || i == radioButtonMangle || i == radioButtonNoOutFile)
            {
                radioButtonStyle |= WS_GROUP; // Groups radio buttons for selection to be mutually exclusive.
            }
            else
            {
                radioButtonStyle &= ~WS_GROUP;
            }
            if (!(pStateGUI->hwnds[i] = CreateWindowW(L"BUTTON", radioButtonText[i - radioButtonStart], radioButtonStyle, 0, 0, 0, 0, pStateGUI->hwnds[mainWindow], (HMENU)(uintptr_t)i, hInstance, NULL)))
            { MessageBoxW(NULL, L"Radio button control CreateWindowW failed!", L"Error", MB_OK | MB_ICONERROR); return 0; }
            if (i == radioButtonHTML || i == radioButtonMangle || i == radioButtonOutFileStrip)
            {
                SendMessage( pStateGUI->hwnds[i], BM_SETCHECK, BST_CHECKED, 0); // Makes the radiobutton selected.
            }
            SendMessage(pStateGUI->hwnds[i], WM_SETFONT, (WPARAM)getDefaultUIFont(), TRUE);        
        }
    }

    // Create buttons. Sets HMENU (the id of each) to the corresponding enum value. Sizing logic is inside the WM_SIZE message handling.
    {
        static const wchar_t* buttonText[] = { L"Files...", L"Out. Dir.", L"GO !"};
        static_assert( _countof(buttonText) == (buttonEnd - buttonStart), "Count mismatch: Update the buttonText array!");
        for (uint8_t i = buttonStart; i < buttonEnd; i++)
        {
            if (!(pStateGUI->hwnds[i] = CreateWindowW(L"BUTTON", buttonText[i - buttonStart], WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, pStateGUI->hwnds[mainWindow], (HMENU)(uintptr_t)i, hInstance, NULL)))
            { MessageBoxW(NULL, L"Button control CreateWindowW failed!", L"Error", MB_OK | MB_ICONERROR); return 0; }
            SendMessage(pStateGUI->hwnds[i], WM_SETFONT, (WPARAM)getDefaultUIFont(), TRUE);
        }
    }

    // Create checkboxes. Sets HMENU (the id of each) to the corresponding enum value. Sizing logic is inside the WM_SIZE message handling.
    {
        static const wchar_t* checkboxText[] = { L"Default to prev. HTML"};
        static_assert( _countof(checkboxText) == (checkboxEnd - checkboxStart), "Count mismatch: Update the checkboxText array!" );
        DWORD checkboxStyle =  WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX;
        for (uint8_t i = checkboxStart; i < checkboxEnd; i++)
        {
            if (!(pStateGUI->hwnds[i] = CreateWindowW(L"BUTTON", checkboxText[i - checkboxStart], checkboxStyle, 0, 0, 0, 0, pStateGUI->hwnds[mainWindow], (HMENU)(uintptr_t)i, hInstance, NULL)))
            { MessageBoxW(NULL, L"Checkbox control CreateWindowW failed!", L"Error", MB_OK | MB_ICONERROR); return 0; }
            SendMessage( pStateGUI->hwnds[i], BM_SETCHECK, BST_CHECKED, 0); // Makes the cheackbox selected.
            SendMessage(pStateGUI->hwnds[i], WM_SETFONT, (WPARAM)getDefaultUIFont(), TRUE);
        }
    }

    // Create edit controls. Sets HMENU (the id of each) to the corresponding enum value. Sizing logic is inside the WM_SIZE message handling.
    if (!LoadLibraryW(L"comctl32.dll")) { MessageBoxW(NULL, L"LoadLibraryW comctl32.dll failed! Can't standard edit controls!", L"Error", MB_OK | MB_ICONERROR); return 0; }
    {
        // "You cannot set a cue banner on a multiline edit control or on a rich edit control."
        static const wchar_t* editControlCueBanner[] = { PathStrip_TXT };
        static_assert( _countof(editControlCueBanner) == (editEnd- editStart), "Count mismatch: Update the editControlCueBanner array!");
        for (uint8_t i = editStart; i < editEnd; i++)
        {
            DWORD editControlStyle = WS_CHILD | WS_VISIBLE | ES_CENTER;
            if (!(pStateGUI->hwnds[i] = CreateWindowW(L"EDIT", L"", editControlStyle, 0, 0, 0, 0, pStateGUI->hwnds[mainWindow], (HMENU)(uintptr_t)i, hInstance, NULL)))
            { MessageBoxW(NULL, L"Edit control CreateWindowW failed!", L"Error", MB_OK | MB_ICONERROR); return 0; }
            SendMessageW(pStateGUI->hwnds[i], WM_SETFONT, (WPARAM)getDefaultUIFont(), TRUE);
            Edit_SetCueBannerText(pStateGUI->hwnds[i], L"Stip Path Segment");
        }
    }

    // Apply dark mode to everything. Not necessary in all environments.
    if (allowDarkMode)
    {
        {
            for (uint8_t i = mainWindow; i < countOfHwnd; i++)
            {
                SetWindowTheme(pStateGUI->hwnds[i], L"DarkMode_Explorer", NULL);
                allowDarkMode(pStateGUI->hwnds[i], true);
                SendMessageW(pStateGUI->hwnds[i], WM_THEMECHANGED, 0, 0);
            }
        }
        pStateGUI->failedToApplyDarkTheme = 0;
    }

    // Hide focus rectangle if clicking elements and not navigating with the keyboard.
    SendMessage(pStateGUI->hwnds[mainWindow], WM_CHANGEUISTATE, MAKEWPARAM(UIS_SET, UISF_HIDEFOCUS), 0);


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

LRESULT sizeControls(StateGUI* pStateGUI, LPARAM lParam)
{
        // TODO: Make DPI aware and tidy.
    
        int clientAreaWidth = LOWORD(lParam);
        int clientAreaHeight = HIWORD(lParam);

        const int H_RADIO  = 20; // Standard compact height for radio/check
        const int H_BUTTON = 26; // Standard height for buttons
        const int GAP_ITEM = 2;  // Gap between items in the same group
        const int GAP_SECT = 12; // Gap between major sections
        const int W_MENU   = RIGHT_MENU_WIDTH;
        const int X_MENU   = clientAreaWidth - 10 - W_MENU;
        const int X_CTRL   = X_MENU + 5; // Indent controls slightly inside the 'group'
        const int W_CTRL   = W_MENU - 10;
        

        // 1. Size and position rich edit controls (Left Side)
        int richEditHeight = (clientAreaHeight - 36) / 3;
        int topMargin = 5;
        RECT richEditPadding = { 10, 10 , clientAreaWidth - 20, richEditHeight - 20}; // Padding
        for (uint8_t i = richEditStart; i < richEditEnd; i++)
        {
            MoveWindow(pStateGUI->hwnds[i], 10, topMargin + (i - richEditStart) * richEditHeight, clientAreaWidth - 20 - W_MENU - 10, richEditHeight, TRUE);
            SendMessage(pStateGUI->hwnds[i], EM_SETRECT, 0, (LPARAM)&richEditPadding);
            topMargin += 10;
        }

        // 2. Right Menu Layout
        topMargin = 5; // Reset top for right menu

        
        // --- SECTION 1: INPUT (Radios + Files Button + Checkbox) ---
        {
            int lineTop   = topMargin + 8; // Line starts slightly below text
            
            // 1a. Header Text
            MoveWindow(pStateGUI->hwnds[staticInputSpacerText], X_CTRL, topMargin, 42, 15, TRUE);
            topMargin += 18; // Move past header

            // 1b. Radio Buttons (HTML, CSS, JS)
            for (uint8_t i = radioButtonHTML; i <= radioButtonJS; i++)
            {
                MoveWindow(pStateGUI->hwnds[i], X_CTRL, topMargin, W_CTRL, H_RADIO, TRUE);
                topMargin += (H_RADIO + GAP_ITEM);
            }

            // 1c. Button: Files
            MoveWindow(pStateGUI->hwnds[buttonFiles], X_CTRL, topMargin, W_CTRL, H_BUTTON, TRUE);
            topMargin += (H_BUTTON + GAP_ITEM);

            // 1d. Checkbox: Default to Prev
            MoveWindow(pStateGUI->hwnds[checkboxDefaultToPrev], X_CTRL, topMargin, W_CTRL, H_RADIO, TRUE);
            topMargin += (H_RADIO + GAP_ITEM);

            // 1e. Draw the Container Line (Now that we know final topMargin)
            // Height is current top minus start of line, plus a little padding at bottom
            int lineHeight = (topMargin - lineTop) + 4; 
            MoveWindow(pStateGUI->hwnds[staticInputSpacerLine], X_MENU, lineTop, W_MENU, lineHeight, TRUE);
        }

        topMargin += GAP_SECT;


        // -- Section 2: Output Options --
        int countOutput = (radioButtonKeepNames - radioButtonMangle) + 1;
        int heightOutputLine = (countOutput * (H_RADIO + GAP_ITEM)) + 12;

        MoveWindow(pStateGUI->hwnds[staticOutputSpacerLine], X_MENU, topMargin + 8, W_MENU, heightOutputLine, TRUE);
        MoveWindow(pStateGUI->hwnds[staticOutputSpacerText], X_CTRL, topMargin, 52, 15, TRUE);
        
        topMargin += 15;

        for (uint8_t i = radioButtonMangle; i <= radioButtonKeepNames; i++)
        {
            MoveWindow(pStateGUI->hwnds[i], X_CTRL, topMargin, W_CTRL, H_RADIO, TRUE);
            topMargin += (H_RADIO + GAP_ITEM);
        }

        topMargin += GAP_SECT;

        // -- Section 3: Out File Options --
        
        int section3StartY = topMargin;
        
        // Position Header Text (Line is done after we know the height)
        MoveWindow(pStateGUI->hwnds[staticOutFileSpacerText], X_CTRL, topMargin, 52, 15, TRUE);
        topMargin += 15;

        for (uint8_t i = radioButtonNoOutFile; i <= radioButtonOutFilePath; i++)
        {
            MoveWindow(pStateGUI->hwnds[i], X_CTRL, topMargin, W_CTRL, H_RADIO, TRUE);
            topMargin += (H_RADIO + GAP_ITEM);

            if (i == radioButtonOutFileStrip)
            {
                // Indent the strip edit control slightly or keep aligned
                MoveWindow(pStateGUI->hwnds[staticBackgroundForEditControl], X_CTRL, topMargin, W_CTRL, PathStrip_HEIGHT, TRUE);
                MoveWindow(pStateGUI->hwnds[staticBorderForEditControl], X_CTRL, topMargin, W_CTRL, PathStrip_HEIGHT, TRUE);
                MoveWindow(pStateGUI->hwnds[editPathStrip], X_CTRL + 2, topMargin + 7, W_CTRL - 4, PathStrip_HEIGHT - 14, TRUE);
                topMargin += (PathStrip_HEIGHT + GAP_ITEM);
            }
            if (i == radioButtonOutFilePath)
            {
                // Output Directory button
                MoveWindow(pStateGUI->hwnds[buttonOutDir], X_CTRL, topMargin, W_CTRL, H_BUTTON, TRUE);
                topMargin += (H_BUTTON + GAP_ITEM);
            }
        }

        // Now retroactively draw the line for Section 3 based on how much topMargin grew
        int heightOutFileLine = (topMargin - section3StartY) - 5; // -5 to pull bottom up slightly
        MoveWindow(pStateGUI->hwnds[staticOutFileSpacerLine], X_MENU, section3StartY + 8, W_MENU, heightOutFileLine, TRUE);

        topMargin += GAP_SECT;

        // -- Main Action Button --
        MoveWindow(pStateGUI->hwnds[buttonGo], X_MENU, topMargin, W_MENU, 40, TRUE);

        // Repaint window
        InvalidateRect(pStateGUI->hwnds[mainWindow], NULL, TRUE);

    return 0;
}

HFONT getDefaultUIFont()
{
    static HFONT hSystemFont = NULL; // Don't use more than one GDI object for the same font ever.

    if (hSystemFont) { return hSystemFont; }

    // Get system-wide used font. Typically Segoe UI 9pt.
    NONCLIENTMETRICS ncm = { }; 
    ncm.cbSize = sizeof(NONCLIENTMETRICS);

    bool spiSucceeded = SystemParametersInfo(SPI_GETNONCLIENTMETRICS, ncm.cbSize, &ncm, 0); // Vista and later.

    if (!spiSucceeded)
    {
        ncm.cbSize -= sizeof(int); // Retry with smaller sizeof(NONCLIENTMETRICS) for pre-Vista OS.
        spiSucceeded = SystemParametersInfo(SPI_GETNONCLIENTMETRICS, ncm.cbSize, &ncm, 0);
    }

    if (spiSucceeded)
    {
        hSystemFont = CreateFontIndirect(&ncm.lfMessageFont); // Create as a GDI object the same system-wide used font.
    }

    if (!hSystemFont) 
    { 
        hSystemFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT); // Fallback.
    }

    return hSystemFont;
}