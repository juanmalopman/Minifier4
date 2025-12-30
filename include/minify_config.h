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
	bool alreadyPresentGUI;
	bool flagNoGUI;
	wchar_t prevPath[MAX_PATH];
	wchar_t inPath[MAX_PATH];
    int inputType;
    bool defaultToPrevFile;
    bool mangle;
    int outFile;
    wchar_t stripSeg[MAX_PATH];
    wchar_t outPath[MAX_PATH];
} MiniCfg;

// Context structure to hold the shuffled alfabet to generate mangled names.
typedef struct NameGenerator
{
    wchar_t startWchars[52]; // a-z, A-Z
    wchar_t otherWchars[64];   // a-z, A-Z, 0-9, -, _
} NameGenerator;

//
// FUNCTION PROTOTYPES
//

void miniCfgInit(_Out_ MiniCfg* pMiniCfg, _Out_ StateGUI* pStateGUI);
bool miniCfgParseCLI(_In_ MiniCfg* pMiniCfg, _In_ StateGUI* pStateGUI, _In_opt_z_ PWSTR forwardedArgs);
void miniCfgGetMangled(_In_ int index, _Out_ wchar_t* buffer);