#pragma once

//
// DEFINES
//

#define STR_LEN(str) ((sizeof(str) / sizeof(str[0])) - 1)

#define CONSOLE_PREFIX_NUMBER L"99999" // Visual limit to line numbering, then loops.
#define CONSOLE_PREFIX_CHARS L": "
#define CONSOLE_PREFIX CONSOLE_PREFIX_NUMBER CONSOLE_PREFIX_CHARS

#define CONSOLE_NUMBER_LEN STR_LEN(CONSOLE_PREFIX_NUMBER)
#define CONSOLE_PREFIX_LEN STR_LEN(CONSOLE_PREFIX)

//
// GLOBAL VARIABLES
//


//
// FUNCTIONS
//

void print(HWND, const wchar_t*, UINT);
void printInteger(HWND, int64_t, UINT);
void printLineNumbering();
void setRichEditFormatting(HWND,UINT);
void alertPopup(LPCWSTR);
void errorPopup(LPCWSTR);