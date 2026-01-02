#pragma once

//
// DEPENDENCIES
//

#include <windows.h>
#include <stdint.h> // int64_t, uint32_t, etc.
#include <wchar.h> // swprintf_s, wchar_t, etc.
#include "main_window.h"

//
// FUNCTION PROTOTYPES
//

void parserCommonSpawnParsingThread(_In_ StateGUI* pStateGUI, _In_ int extension, _In_ bool mainParsingThread, _In_ wchar_t* data, _In_ int64_t len);
void parserCommonFinished(_Inout_ StateGUI* pStateGUI, _In_ wchar_t* minified);
void parserCommonRun(_Inout_ StateGUI* pStateGUI);
void parserCommonGetMangled(_In_ int index, _Out_ wchar_t* buffer,_In_ bool rand);