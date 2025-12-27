
//
// INCLUDES
//

#include "framework.h"
#include "printRichEdit.h"
#include "callbackWNDPROC.h"
#include "GUI.h"

//
// GLOBAL VARIABLES
//


//
// FUNCTIONS
//


// Sends a fully formatted string to the UI thread.
void postToMainWindow(HWND hMain, wchar_t* buffer, UINT whichControl)
{
    if (!buffer) { return; }

    // Use PostMessage (Asynchronous).
    BOOL result = PostMessageW(hMain, whichControl, 0, (LPARAM)buffer);

    // If the message wasn't posted, free heap memory here.
    if (!result) { free(buffer); }

    return;
}

void print(HWND hMain, const wchar_t* message, UINT whichControl)
{
	if (!message) return;

    // Calculate buffer size.
    size_t len = wcslen(message);

    if (!len) return;

    // swprintf_s and wcscpy_s might need to add a null termination '\0', 1 wchars.
    len += 1;

    // For richEditConsole, we will also add "99999: ".
    if (whichControl == TO_CONSOLE) len += wcslen(CONSOLE_PREFIX);

    // Allocate heap.
    wchar_t* buffer = (wchar_t*)malloc(sizeof(wchar_t) * len);
    if (!buffer) return; // Out of memory

    // For richEditConsole, we will also add "99999: \r\n", 9 wchars.
    if (whichControl == TO_CONSOLE)
    {
        swprintf_s(buffer, len, L"%s%s", CONSOLE_PREFIX, message);
    }
    else
    {
    	wcscpy_s(buffer, len, message);
    }
	postToMainWindow(hMain, buffer, whichControl);    
}

void printInteger(HWND hMain, int64_t number, UINT whichControl)
{
	// Max we can have is "-9223372036854775808\0", 21 wchars.
	wchar_t integerMaxBuffer[21] = { };
	swprintf_s(integerMaxBuffer, _countof(integerMaxBuffer), L"%lld", number);
	print(hMain, (const wchar_t*)integerMaxBuffer, whichControl);
}


void setRichEditFormatting(HWND richEditControl)
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

void alertPopup(LPCWSTR message)
{
	MessageBoxW(NULL, message, mainWindowName, MB_OK | MB_ICONINFORMATION);
}

void errorPopup(LPCWSTR message)
{
    MessageBoxW(NULL, message, mainWindowName, MB_OK | MB_ICONERROR);
}

