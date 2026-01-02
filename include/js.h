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

DWORD WINAPI jsSpawnThread(_Inout_ LPVOID lpParam);