#pragma once

//
// DEPENDENCIES
//

#include <windows.h>

//
// ENUMS
//

typedef enum windowMessagesCustom : UINT
{
	WINDOW_MESSAGES_START_OF_AVAILABLE_MSG_IDS = WM_APP,
	MSGCUSTOM_PRINTINPUT,
	MSGCUSTOM_PRINTOUTPUT,
	MSGCUSTOM_PRINTCONSOLE
}windowMessagesCustom;


//
// FUNCTIONS
//

LRESULT CALLBACK windowMessagesCallback(_In_ HWND hWnd, _In_ UINT uMsg, _In_ WPARAM wParam, _In_ LPARAM lParam);