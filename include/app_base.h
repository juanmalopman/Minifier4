#pragma once

//
// DEFINES
//

#define _WIN32_WINNT_WIN10 0x0A00
#define _WIN32_WINNT _WIN32_WINNT_WIN10 // Expose modern APIs like GetDpiForSystem by targeting Windows 10.
#define WIN32_LEAN_AND_MEAN // No winsock.h (v1), wincrypt.h, shellapi.h and other rarely used parts of windows.h.
#define NOMINMAX // Avoid int max = 10; triggering an unwanted macro.
#define STRICT // Avoid accepting a brush handle if the function requests a window handle and so on.

#define CINTERFACE // Required for C style interface while creating common item dialogs like folder pickers.
#define COBJMACROS // Macros to make calling COM methods prettier in C like when creating common item dialogs like folder pickers.

//
// INCLUDES
//

#include <windows.h>
#include <stdint.h> // The C standard library.
#include <wchar.h> // swprintf_s etc.

