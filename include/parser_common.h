#pragma once

//
// DEPENDENCIES
//

#include <windows.h>
#include <stdint.h> // int64_t, uint32_t, etc.
#include <wchar.h> // swprintf_s, wchar_t, etc.
#include "main_window.h"

//
// STRUCTS
//

typedef struct ParsingThreadArgs{
    StateGUI* pStateGUI;
    bool mainParsingThread;
    wchar_t* data;
    size_t len;
} ParsingThreadArgs;

//
// FUNCTION PROTOTYPES
//

char* parserCommonGetPointerToUTF8(wchar_t* data, size_t* pLen);
void parserCommonSpawnParsingThread(_In_ StateGUI* pStateGUI, _In_ int extension, _In_ bool mainParsingThread, _In_ wchar_t* data, _In_ size_t len);
void parserCommonFinished(_Inout_ StateGUI* pStateGUI, _In_ char* minified);
void parserCommonRun(_Inout_ StateGUI* pStateGUI);
void parserCommonGetMangled(_In_ int index, _Out_ wchar_t* buffer,_In_ bool rand);