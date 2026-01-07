#pragma once

//
// DEPENDENCIES
//

#include <windows.h>
#include <stdio.h>
#include <string.h> // sprintf_s, char, etc.

//
// FORWARD DECLARATIONS
//

typedef struct StateGUI StateGUI;

//
// STRUCTS
//

typedef struct MiniCfg
{
    int miniCfgIDBytes;
	bool alreadyPresentGUI;
	bool flagHeadless;
	char prevPath[MAX_PATH];
	char inPath[MAX_PATH];
    int inputType;
    bool fallbackToPrevFile;
    bool mangle;
    bool randomMangle;
    int outOpt;
    char stripSeg[MAX_PATH];
    char outPath[MAX_PATH];
    bool outFilename;
    char outFile[MAX_PATH];
    bool currentlyParsing;
} MiniCfg;

//
// FUNCTION PROTOTYPES
//

void miniCfgSave(_In_ MiniCfg* pMiniCfg);
void miniCfgLoad(_Out_ MiniCfg* pMiniCfg, _Out_ StateGUI* pStateGUI);
bool miniCfgParseCLI(_In_ MiniCfg* pMiniCfg, _In_ StateGUI* pStateGUI, _In_opt_z_ PWSTR forwardedArgs);
