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

void cssSpawnThread(_In_ StateGUI* pStateGUI, _In_ bool mainParsingThread, _In_ wchar_t* data, _In_ int64_t len);