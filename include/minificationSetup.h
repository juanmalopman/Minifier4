#pragma once

//
// FORWARD DECLARATIONS
//

typedef struct StateGUI StateGUI;


//
// DEFINES
//


//
// STRUCTS
//

typedef struct MinOpt
{
	bool alreadyPresentGUI;
	bool flagNoGUI;
	wchar_t prevPath[MAX_PATH];
	wchar_t inPath[MAX_PATH];
    uint8_t inputType;
    bool defaultToPrevFile;
    bool mangle;
    uint8_t outFile;
    wchar_t stripSeg[MAX_PATH];
    wchar_t outPath[MAX_PATH];
} MinOpt;

//
// GLOBAL VARIABLES
//


//
// FUNCTIONS
//

void loadMinificationSettings(MinOpt*);
bool parseArgumentsCLI(MinOpt*, StateGUI*);