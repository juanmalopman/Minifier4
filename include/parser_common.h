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


//
// FUNCTION PROTOTYPES
//

void parserCommonRun(_Inout_ StateGUI* pStateGUI);
void parserCommonGetMangled(_In_ int index, _Out_ wchar_t* buffer,_In_ bool rand);