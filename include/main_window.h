#pragma once

//
// DEPENDENCIES
//

#include <windows.h>
#include <wchar.h> // swprintf_s, wcslen, etc.

//
// FORWARD DECLARATIONS
//

typedef struct MiniCfg MiniCfg;

//
// ENUMS
//

typedef enum PreferredAppMode : int
{
   PreferredAppMode_Default,
   PreferredAppMode_AllowDark,
   PreferredAppMode_ForceDark,
   PreferredAppMode_ForceLight,
   PreferredAppMode_Max
} PreferredAppMode;

typedef enum enumOfHwnds : int // To keep track of all window handles.
{
    mainWindow,

    staticStart,
        staticInputSpacerLine   = staticStart, // Fix index
        staticInputSpacerText,
        staticOutputSpacerLine,
        staticOutputSpacerText,
        staticOutFileSpacerLine,
        staticOutFileSpacerText,
        staticSettingsSpacerLine,
        staticSettingsSpacerText,
        staticFilenameBorder,
        staticFilenameBkgnd,
        staticPathStripBorder,
        staticPathStripBkgnd,
        staticOutDirBorder,
        staticOutDirBkgnd,
    staticEnd,

    richEditStart               = staticEnd, // Fix index
        richEditInput           = staticEnd, // Fix index
        richEditOutput,
        richEditConsole,
    richEditEnd,

    radioButtonStart            = richEditEnd, // Fix index
        radioButtonHTML         = richEditEnd, // Fix index
        radioButtonCSS,
        radioButtonJS,
        radioButtonAutodetect,
        radioButtonNoOutFile,
        radioButtonOutFileStrip,
        radioButtonOutFilePath,
    radioButtonEnd,

    buttonStart                 = radioButtonEnd, // Fix index
        buttonFiles             = radioButtonEnd, // Fix index
        buttonOutDir,
        buttonLoad,
        buttonSave,
        buttonGo,
    buttonEnd,

    checkboxStart               = buttonEnd, // Fix index
        checkboxFallbackToPrev   = buttonEnd, // Fix index
        checkboxMangle,
        checkboxRandomMangle,
        checkboxFilename,
    checkboxEnd,

    editStart                   = checkboxEnd, // Fix index
        editFilename            = checkboxEnd, // Fix index
        editPathStrip,
        editOutDir,
    editEnd,
    
    // This must always be last. It tells us the size of the array.
    countOfHwnd                 = editEnd // Fix index
} enumOfHwnds;

static_assert(staticStart       == mainWindow + 1, "ERROR: Mangled enum, detected gap after mainWindow");
static_assert(richEditStart     == staticEnd,      "ERROR: Mangled enum, detected gap between Static and RichEdit");
static_assert(radioButtonStart  == richEditEnd,    "ERROR: Mangled enum, detected gap between RichEdit and RadioButton");
static_assert(buttonStart       == radioButtonEnd, "ERROR: Mangled enum, detected gap between RadioButton and Button");
static_assert(checkboxStart     == buttonEnd,      "ERROR: Mangled enum, detected gap between Button and Checkbox");
static_assert(editStart         == checkboxEnd,    "ERROR: Mangled enum, detected gap between Checkbox and Edit");
static_assert(countOfHwnd       == editEnd,        "ERROR: Mangled enum, detected gap between Edit and Count");

//
// CONFIGURATION CONSTANTS
//

static constexpr wchar_t MAIN_WINDOW_NAME[] = L"Minifier 4";
static constexpr COLORREF MAIN_WINDOW_GRAY_BKG = RGB(32, 32, 32);
static constexpr COLORREF MAIN_WINDOW_WHITE_TXT = RGB(248, 248, 242);
static constexpr LRESULT MAIN_WINDOW_CDRF_NOTHANDLED = -1; // Custom LRETURN for mainWindowRadioBtnCustomDraw.

//
// TYPEDEFS
//

// uxtheme.dll dark mode functions.
typedef BOOL (WINAPI *PFN_SetPreferredAppMode)(PreferredAppMode);
typedef BOOL (WINAPI *PFN_AllowDarkMode)(HWND, BOOL);

//
// STRUCTS
//

typedef struct StateGUI
{
    HWND hwnds[countOfHwnd];
    PFN_AllowDarkMode darkModeApplied;
    UINT currentDPI;
    MiniCfg* pMiniCfg;
} StateGUI;

typedef struct LayoutCtx
{
    UINT dpi;       // Current DPI.
    HFONT hFont;    // Current font.
    int x;          // Current X position.
    int y;          // Current Y position.
    int width;      // Standard width for controls in a given column.
    int gapItem;    // Gap between two consecutive controls.
} LayoutCtx;

//
// FUNCTION PROTOTYPES
//

bool mainWindowCheckForOtherInstance();
bool mainWindowInit(_In_ HINSTANCE hInstance, _Inout_ StateGUI* pStateGUI);
LRESULT mainWindowSizing(_In_ StateGUI* pStateGUI, _In_ LPARAM lParam);
HFONT mainWindowGetFont(_In_ UINT dpi);
void mainWindowReplaceRichText(_In_ HWND hWnd, _In_ wchar_t* message);
void mainWindowUpdateControls(_In_ StateGUI* pStateGUI);
void mainWindowEnableControls(_In_ StateGUI* pStateGUI, _In_ bool enable);
LRESULT mainWindowRadioBtnCustomDraw(_Inout_ LPARAM lParam, _In_ StateGUI* pStateGUI);
LRESULT mainWindowHandleGetMinMaxInfo(_Out_ LPARAM lParam, _In_ StateGUI* pStateGUI);
bool mainWindowMsgOnlyWindowInit(_In_ HINSTANCE hInstance,_Inout_ StateGUI* pStateGUI);
