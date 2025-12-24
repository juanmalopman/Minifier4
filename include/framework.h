#pragma once

//
// DEFINES
//

#define WIN32_LEAN_AND_MEAN // No winsock.h (v1), wincrypt.h, shellapi.h and other rarely used parts of windows.h.

//
// INCLUDES
//

#include <windows.h>
#include <stdint.h> // The C standard library.
#include <wchar.h> // swprintf_s etc.
#include <dwmapi.h> // For dark mode title bars. (Library added to CMakeLists.txt).
#include <uxtheme.h> // Dark mode scroll bars and buttons. (Library added to CMakeLists.txt).
#include <richedit.h> // For the rich edit control used as a console.
#include <vsstyle.h> // Required for BP_CHECKBOX (Redrawing radio buttons).
#include <vssym32.h> // Required for CBS_UNCHECKEDNORMAL (Redrawing radio buttons).
#include <commctrl.h> // Allows UI controls to be subclassed and some messages handled to change their graphics. (Library added to CMakeLists.txt).