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
// FUNCTION PROTOTYPES
//

DWORD WINAPI cssSpawnThread(_Inout_ LPVOID lpParam);