
//
// DEPENDENCIES
//

#include <windows.h>
#include <stdint.h> // int64_t, uint32_t, etc.
#include <wchar.h> // swprintf_s, wcslen, etc.
#include <richedit.h> // For the rich edit control used as a console.
#include "app_logging.h"
#include "window_messages.h"
#include "main_window.h"

//
// FUNCTIONS
//

static HWND mainWindowHWND = nullptr;
void appLogSetup(HWND hMain)
{
    mainWindowHWND = hMain;
}

// Sends a string to the UI thread.
static void internalPostToMainWindow(_In_ wchar_t* buffer, _In_ UINT whichControl)
{
    if (!buffer) { return; }

    // Use PostMessage (Asynchronous).
    BOOL result = PostMessageW(mainWindowHWND, whichControl, 0, (LPARAM)buffer);

    // If the message wasn't posted, free heap memory here.
    if (!result) { free(buffer); }

    return;
}

void appLogPrint(const wchar_t* message, UINT whichControl)
{
    if (!message) return;

    if (!whichControl) return;

    if (!mainWindowHWND) return;

    // Calculate buffer size.
    size_t len = wcslen(message);

    if (!len) return;

    // swprintf_s and wcscpy_s might need to add a null termination '\0', 1 wchars.
    len += 1;

    // For richEditConsole, we will also add "99999: ".
    if (whichControl == APP_LOG_TO_CONSOLE) len += wcslen(APP_LOG_CONSOLE_PREFIX);

    // ALLOCATION STRATEGY: Asynchronous Ownership Transfer.
    // The heap-allocated buffer is passed to the UI thread via PostMessage (lParam).
    // The receiving window procedure (windowMessagesCallback) assumes full ownership
    // and is responsible for calling free() after processing.
    wchar_t* buffer = (wchar_t*)malloc(sizeof(wchar_t) * len);
    if (!buffer) return; // Out of memory

    // For richEditConsole, we will also add "99999: \r\n", 9 wchars.
    if (whichControl == APP_LOG_TO_CONSOLE)
    {
        swprintf_s(buffer, len, L"%s%s", APP_LOG_CONSOLE_PREFIX, message);
    }
    else
    {
        wcscpy_s(buffer, len, message);
    }
    internalPostToMainWindow(buffer, whichControl);    
}

void appLogPrintInt(int64_t number, UINT whichControl)
{
    if (!whichControl) return;

    if (!mainWindowHWND) return;

	// Max we can have is "-9223372036854775808\0", 21 wchars.
	wchar_t integerMaxBuffer[21] = { };
	swprintf_s(integerMaxBuffer, _countof(integerMaxBuffer), L"%lld", number);
	appLogPrint((const wchar_t*)integerMaxBuffer, whichControl);
}

void appLogSetFormatting(HWND richEditControl)
{
	CHARFORMATW monospaceCustomFont = { };
	monospaceCustomFont.cbSize = sizeof(CHARFORMATW);
	monospaceCustomFont.dwMask = CFM_COLOR | CFM_FACE | CFM_SIZE;
	monospaceCustomFont.yHeight = 200;
	monospaceCustomFont.crTextColor = RGB(248, 248, 242);
	const wchar_t *szFaceName = L"Consolas";
	wcscpy_s(monospaceCustomFont.szFaceName, _countof(monospaceCustomFont.szFaceName), szFaceName);
    SendMessageW(richEditControl, EM_SETCHARFORMAT, (WPARAM)SCF_SELECTION, (LPARAM)&monospaceCustomFont);
}

void appLogAlertPop(LPCWSTR message)
{
	MessageBoxW(NULL, message, MAIN_WINDOW_NAME, MB_OK | MB_ICONINFORMATION);
}

void appLogErrorPop(LPCWSTR message)
{
    MessageBoxW(NULL, message, MAIN_WINDOW_NAME, MB_OK | MB_ICONERROR);
}

