#pragma once

//
// DEPENDENCIES
//

#include <windows.h>
#include <wchar.h> // swprintf_s, wchar_t, etc.
#include "main_window.h"

//
// STRUCTS
//

// Context structure to hold the shuffled alfabet to generate mangled names. // TODO: Can't it be local?
typedef struct NameGenerator
{
    wchar_t startWchars[52]; // a-z, A-Z
    wchar_t otherWchars[64];   // a-z, A-Z, 0-9, -, _
} NameGenerator;

//
// FUNCTION PROTOTYPES
//

void parserCommonRun(_Inout_ StateGUI* pStateGUI);
void parserCommonGetMangled(_In_ int index, _Out_ wchar_t* buffer);