#pragma once

//
// DEPENDENCIES
//

#include <windows.h>
#include <stdint.h> // int64_t, uint32_t, etc.
#include <wchar.h> // swprintf_s, wcslen, etc.
#include <window_messages.h>

//
// CONFIGURATION CONSTANTS
//

#define RAW_PREFIX_NUM   L"99999"
#define RAW_PREFIX_CHARS L": "
static constexpr wchar_t APP_LOG_CONSOLE_PREFIX[] = RAW_PREFIX_NUM RAW_PREFIX_CHARS;
static constexpr wchar_t APP_LOG_CONSOLE_PREFIX_NUMBER[] = RAW_PREFIX_NUM;
static constexpr wchar_t APP_LOG_CONSOLE_PREFIX_CHARS[]  = RAW_PREFIX_CHARS;
#undef RAW_PREFIX_NUM
#undef RAW_PREFIX_CHARS
static constexpr size_t APP_LOG_CONSOLE_NUMBER_DIGIT_COUNT = _countof(APP_LOG_CONSOLE_PREFIX_NUMBER) - 1;
static constexpr UINT APP_LOG_TO_INPUT = MSGCUSTOM_PRINTINPUT;
static constexpr UINT APP_LOG_TO_OUTPUT = MSGCUSTOM_PRINTOUTPUT;
static constexpr UINT APP_LOG_TO_CONSOLE = MSGCUSTOM_PRINTCONSOLE;

//
// FUNCTION PROTOTYPES
//

void appLogPrintSetup(_In_ HWND hMain);
void appLogPrint(_In_z_ const wchar_t* message, _In_ UINT whichControl);
void appLogPrintInt(_In_ int64_t number, _In_ UINT whichControl);
void appLogSetFormatting(_In_ HWND richEditControl);

void appLogErrorSetup(_In_ bool consoleAvailableArg);
void appLogAlertPop(_In_z_ LPCWSTR message);
void appLogError(_In_z_ LPCWSTR message);