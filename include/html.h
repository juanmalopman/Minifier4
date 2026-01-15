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

static constexpr char htmlTagCSS[] = "<!--CSS-->";
static constexpr char htmlTagFold[] = "<!--FOLD-->";
static constexpr char htmlTagJS[] = "<!--JS-->";
static constexpr char htmlTagError[] = "<!--ERROR-->";
static constexpr char footerTagText[] = "footer";

//
// FUNCTION PROTOTYPES
//

DWORD WINAPI htmlSpawnThread(_In_ LPVOID lpParam);