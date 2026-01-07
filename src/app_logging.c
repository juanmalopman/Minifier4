
//
// DEPENDENCIES
//

#include <windows.h>
#include <stdint.h> // int64_t, uint32_t, etc.
#include <stdio.h>
#include <string.h> // sprintf_s, strlen, etc.
#include <richedit.h> // For the rich edit control used as a console.
#include "app_logging.h"
#include "window_messages.h"
#include "main_window.h"

//
// STRUCTS
//

typedef struct PendingMessages
{
    char* heap;
    UINT dstControl; 
}PendingMessages;

//
// VARIABLES
//

static constexpr int PENDING_MAX = 100;
static PendingMessages pendingMessages[PENDING_MAX] = { };
static volatile int pendingMessagesIndex = 0;

//
// FUNCTIONS
//

void appLogAlertPop(PCSTR message)
{
    MessageBoxA(NULL, message, MAIN_WINDOW_NAME, MB_OK | MB_ICONINFORMATION);
}

static HWND mainWindowHWND = nullptr;
void appLogPrintSetup(HWND hMain)
{
    mainWindowHWND = hMain;
}

// Sends a string to the UI thread.
static void internalPostToMainWindow(_In_ char* buffer, _In_ UINT whichControl)
{
    if (!buffer) { return; }

    // Use PostMessage (Asynchronous).
    BOOL result = PostMessageA(mainWindowHWND, whichControl, 0, (LPARAM)buffer);

    // If the message wasn't posted, free heap memory here.
    if (!result) { free(buffer); }

    return;
}


static SRWLOCK appLogPrintRWLock = SRWLOCK_INIT; // Read-write lock.
void appLogPrint(const char* message, UINT whichControl)
{
    if (!whichControl) return;

    // No more buffer while waiting for mainWindowHWND. Discard message.
    if (!mainWindowHWND && pendingMessagesIndex == 100) return;

    // If there are pending messages and a mainWindowHWND available or inHeadlessMode is set.
    {
        bool pending = false;
        AcquireSRWLockShared(&appLogPrintRWLock);
        if (pendingMessagesIndex) pending = true;
        ReleaseSRWLockShared(&appLogPrintRWLock);

        if (pending && mainWindowHWND)
        {
            // Request write lock to mofify pendingMessagesIndex and send messages.
            AcquireSRWLockExclusive(&appLogPrintRWLock);

            // Check if another thread got the lock and did the job before.
            if (pendingMessagesIndex)
            {
                // Send them all.
                for (int i = 0; i < pendingMessagesIndex; i++)
                {
                    internalPostToMainWindow(pendingMessages[i].heap, pendingMessages[i].dstControl);                    
                }
                pendingMessagesIndex = 0;
            }
            ReleaseSRWLockExclusive(&appLogPrintRWLock);

            // Continue processing the message that motivated this appLogPrint call.
        }
    }

    // This return is after the pending messages flush to be able to trigger it with an empty call.
    if (!message) return;

    // Calculate buffer size.
    size_t len = strlen(message);

    if (!len) return;

    // sprintf_s and strcpy_s might need to add a null termination '\0', 1 chars.
    len += 1;

    // For richEditConsole, we will also add "99999: ".
    if (whichControl == APP_LOG_TO_CONSOLE) len += strlen(APP_LOG_CONSOLE_PREFIX);

    // ALLOCATION STRATEGY: Asynchronous Ownership Transfer.
    // The heap-allocated buffer is passed to the UI thread via PostMessage (lParam).
    // The receiving window procedure (windowMessagesCallback) assumes full ownership
    // and is responsible for calling free() after processing.
    char* buffer = malloc(len);
    if (!buffer) return; // Out of memory

    // For richEditConsole, we will also add "99999: \r\n", 9 chars.
    if (whichControl == APP_LOG_TO_CONSOLE)
    {
        sprintf_s(buffer, len, "%s%s", APP_LOG_CONSOLE_PREFIX, message);
    }
    else
    {
        strcpy_s(buffer, len, message);
    }

    // If mainWindowHWND is ready to receive messages.
    if (mainWindowHWND)
    {
       internalPostToMainWindow(buffer, whichControl); 
    }
    else
    {
        // Get write lock.
        AcquireSRWLockExclusive(&appLogPrintRWLock);
        
        // Check if another thread got the lock before and took care of everything.
        if (mainWindowHWND && !pendingMessagesIndex)
        {
            ReleaseSRWLockExclusive(&appLogPrintRWLock);
            internalPostToMainWindow(buffer, whichControl);
            return;
        }

        // Check if another thread got the lock before and occupied the last buffer index.
        if (pendingMessagesIndex == 100)
        {
            ReleaseSRWLockExclusive(&appLogPrintRWLock);
            return;
        }

        pendingMessages[pendingMessagesIndex].heap = buffer;
        pendingMessages[pendingMessagesIndex].dstControl = whichControl;
        pendingMessagesIndex++;

        ReleaseSRWLockExclusive(&appLogPrintRWLock);
    }
        
}

void appLogPrintInt(int64_t number, UINT whichControl)
{
    if (!whichControl) return;

    if (!mainWindowHWND) return;

	// Max we can have is "-9223372036854775808\0", 21 chars.
	char integerMaxBuffer[21] = { };
	sprintf_s(integerMaxBuffer, sizeof(integerMaxBuffer), "%lld", number);
	appLogPrint((const char*)integerMaxBuffer, whichControl);
}

void appLogSetFormatting(HWND hRichEditControl)
{
	CHARFORMATA monospaceCustomFont = { };
	monospaceCustomFont.cbSize = sizeof(CHARFORMATA);
	monospaceCustomFont.dwMask = CFM_COLOR | CFM_FACE | CFM_SIZE;
	monospaceCustomFont.yHeight = 200;
	monospaceCustomFont.crTextColor = MAIN_WINDOW_WHITE_TXT;
	const char *szFaceName = "Consolas";
	strcpy_s(monospaceCustomFont.szFaceName, sizeof(monospaceCustomFont.szFaceName), szFaceName);
    SendMessageA(hRichEditControl, EM_SETCHARFORMAT, (WPARAM)SCF_SELECTION, (LPARAM)&monospaceCustomFont);
}


static bool consoleAvailable = true;
void appLogErrorSetup(bool consoleAvailableArg)
{
    consoleAvailable = consoleAvailableArg;
}

static char* internalGetSysErrorString(DWORD errorCode)
{
    if (errorCode == 0) return NULL;
    char* buffer = NULL;
    FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (PSTR)&buffer, 0, NULL
    );
    return buffer;
}

void appLogError(PCSTR message)
{
    DWORD errCode = GetLastError(); 
    char* sysMsg = internalGetSysErrorString(errCode);

    if (consoleAvailable)
    {
        fprintf(stderr, "[ERROR] %s", message);
        if (sysMsg) fprintf(stderr, "\n\tSystem Code %lu: %s", errCode, sysMsg);
        fprintf(stderr, "\n");
    }
    else
    {
        char* fullBuf = NULL;
        
        if (sysMsg)
        {
            // 100 wchars is sufficient for the overhead
            size_t len = strlen(message) + strlen(sysMsg) + 100; 
            fullBuf = (char*)malloc(len * sizeof(char));
            if (fullBuf) sprintf_s(fullBuf, len, "%s\n\nSystem Error (%lu):\n%s", message, errCode, sysMsg);
        }

        // Display the error.
        if (fullBuf)
        {
            MessageBoxA(NULL, fullBuf, "Minifier 4: ERROR", MB_OK | MB_ICONERROR); // Fixed semicolon
            free(fullBuf);
        } 
        else
        {     
            // Fallback if malloc failed or no system message existed.
            MessageBoxA(NULL, message, "Minifier 4: ERROR", MB_OK | MB_ICONERROR);
        }
    }

    // Free sysMsg in both (consoleAvailable) paths.
    if (sysMsg) 
    {
        LocalFree(sysMsg);
    }
}

