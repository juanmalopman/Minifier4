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
// CONFIGURATION CONSTANTS
//

static constexpr char idNextComment[] = "/*ID-NEXT*/";
static constexpr char classNextComment[] = "/*CLASS-NEXT*/";

//
// FUNCTION PROTOTYPES
//

DWORD WINAPI jsSpawnThread(_Inout_ LPVOID lpParam);