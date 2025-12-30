#pragma once

//
// DEPENDENCIES
//

#include <windows.h>

//
// FUNCTION PROTOTYPES
//

bool fileUtilsGetAppDataPath(_Out_writes_(MAX_PATH) PWSTR path);
bool fileUtilsSaveToFile(_In_ LPCWSTR filePath, _In_ LPCWSTR filename, _In_ LPCVOID buffer, _In_ DWORD len);
bool fileUtilsReadFromFile(_In_ LPCWSTR filePath, _In_ LPCWSTR filename, _Out_ void** outBuffer, _Out_ size_t* outLen);