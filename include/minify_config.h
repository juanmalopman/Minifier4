#pragma once

//
// DEPENDENCIES
//

#include <windows.h>
#include <wchar.h> // swprintf_s, wchar_t, etc.

//
// FORWARD DECLARATIONS
//

typedef struct StateGUI StateGUI;

//
// STRUCTS
//

typedef struct MiniCfg
{
    int64_t buildUniqueID;
	bool alreadyPresentGUI;
	bool flagHeadless;
	wchar_t prevPath[MAX_PATH];
	wchar_t inPath[MAX_PATH];
    int inputType;
    bool fallbackToPrevFile;
    bool mangle;
    bool randomMangle;
    int outOpt;
    wchar_t stripSeg[MAX_PATH];
    wchar_t outPath[MAX_PATH];
    bool outFilename;
    wchar_t outFile[MAX_PATH];
    bool currentlyParsing;
} MiniCfg;

//
// FUNCTION PROTOTYPES
//

void miniCfgSave(_In_ MiniCfg* pMiniCfg);
void miniCfgLoad(_Out_ MiniCfg* pMiniCfg, _Out_ StateGUI* pStateGUI);
bool miniCfgParseCLI(_In_ MiniCfg* pMiniCfg, _In_ StateGUI* pStateGUI, _In_opt_z_ PWSTR forwardedArgs);
