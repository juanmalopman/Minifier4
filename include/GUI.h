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
#define MAIN_WIN_MIN_WIDTH 700
#define MAIN_WIN_MIN_HEIGHT 465
#define RIGHT_MENU_WIDTH 150
#define PathStrip_TXT L"Stripe PATH Seg."
#define PathStrip_HEIGHT 29
#define GoButton_COLOR 0x00000A68


//
// STRUCTS
//

typedef struct StateGUI
{
    HWND hwnds[countOfHwnd];
    bool failedToApplyDarkTheme;
}StateGUI;


//
// VARIABLES
//


//
// FUNCTIONS
//

int initializeGUI(_In_ HINSTANCE, _In_ StateGUI*);
LRESULT sizeControls(StateGUI*, LPARAM);
HFONT getDefaultUIFont();
