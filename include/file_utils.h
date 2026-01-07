#pragma once

//
// DEPENDENCIES
//

#include <windows.h>

//
// CONFIGURATION CONSTANTS
//

static constexpr UINT CP_UTF16LE = 1200;
static constexpr UINT CP_UTF16BE = 1201;

//
// FUNCTION PROTOTYPES
//

bool fileUtilsGetAppDataPath(_Out_writes_(MAX_PATH) PSTR path);
bool fileUtilsSaveToFile(_In_ PCSTR fullPath, _In_ LPCVOID buffer, _In_ DWORD len);
bool fileUtilsReadFromFile(_In_ PCSTR fullPath, _Out_ UINT* codePage, _Out_ void** outBuffer, _Out_ size_t* outLen);