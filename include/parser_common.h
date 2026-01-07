#pragma once

//
// DEPENDENCIES
//

#include <windows.h>
#include <stdint.h> // int64_t, uint32_t, etc.
#include <stdio.h>
#include <string.h> // sprintf_s, char, etc.
#include "main_window.h"

//
// STRUCTS
//

typedef struct ParsingThreadArgs{
    StateGUI* pStateGUI;
    bool mainParsingThread;
    char* data;
    size_t len;
    bool isPath;
} ParsingThreadArgs;

//
// FUNCTION PROTOTYPES
//

char* parserCommonGetPointerToUTF8( _Inout_ char* data, _Inout_ size_t* pLen, _In_ bool isPath);
void parserCommonFinished(_Inout_ StateGUI* pStateGUI, _In_ char* minified, _In_ size_t len);
void parserCommonRun(_Inout_ StateGUI* pStateGUI);
void parserCommonGetMangled(_In_ int index, _Out_ char* buffer,_In_ bool rand);