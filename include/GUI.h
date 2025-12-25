#pragma once

//
// ENUMS
//

typedef enum PreferredAppMode
{
   PreferredAppMode_Default,
   PreferredAppMode_AllowDark,
   PreferredAppMode_ForceDark,
   PreferredAppMode_ForceLight,
   PreferredAppMode_Max
} PreferredAppMode;

typedef enum enumOfHwnds // To keep track of all window handles.
{
    mainWindow,

    staticStart,
        staticInputSpacerLine   = staticStart, // Fix index
        staticInputSpacerText,
        staticOutputSpacerLine,
        staticOutputSpacerText,
        staticOutFileSpacerLine,
        staticOutFileSpacerText,
        staticBorderForEditControl,
        staticBackgroundForEditControl,
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
        radioButtonMangle,
        radioButtonKeepNames,
        radioButtonNoOutFile,
        radioButtonOutFileStrip,
        radioButtonOutFilePath,
    radioButtonEnd,

    buttonStart                 = radioButtonEnd, // Fix index
        buttonFiles             = radioButtonEnd, // Fix index
        buttonOutDir,
        buttonGo,
    buttonEnd,

    checkboxStart               = buttonEnd, // Fix index
        checkboxDefaultToPrev   = buttonEnd, // Fix index
    checkboxEnd,

    editStart                   = checkboxEnd, // Fix index
        editPathStrip           = checkboxEnd, // Fix index
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
// DEFINES
//



#define GRAY_BKG RGB(32, 32, 32)
#define WHITE_TXT RGB(255, 255, 255)
#define PathStrip_TXT L"Stripe PATH Seg."

#define BASE_DPI 96

// In logical pixels that will get scaled:
#define W_MIN_mainWindow 700
#define H_MIN_mainWindow 465
#define W_rightMenu 150
#define H_radio 20
#define H_button 26
#define GAP_normal 10
#define GAP_small 4


//
// STRUCTS
//

typedef struct StateGUI
{
    HWND hwnds[countOfHwnd];
    bool failedToApplyDarkTheme;
    UINT currentDPI;
}StateGUI;

typedef struct LayoutCtx{
    UINT dpi;       // Current DPI.
    HFONT hFont;    // Current font.
    int x;          // Current X position.
    int y;          // Current Y position.
    int width;      // Standard width for controls in a given column.
    int gapItem;    // Gap between two consecutive controls.
} LayoutCtx;

//
// VARIABLES
//


//
// FUNCTIONS
//

int initializeGUI(_In_ HINSTANCE, _In_ StateGUI*);
LRESULT sizeControls(StateGUI*, LPARAM);
UINT getWindowDPI(HWND);
HFONT getDpiAwareFont(UINT);

//
// STATIC INLINE FUNCTIONS
//

// Scale logical pixels to physical device pixels
static inline int scale(int val, UINT dpi)
{
    return MulDiv(val, (int)dpi, BASE_DPI);
}