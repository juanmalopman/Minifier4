
//
// DEPENDENCIES
//

#include <windows.h>
#include <stdint.h> // int64_t, uint32_t, etc.
#include <wchar.h> // swprintf_s, wcslen, etc.
#include "parser_common.h"
#include "main_window.h"
#include "minify_config.h"
#include "app_logging.h"
#include "file_utils.h"
#include <shlwapi.h> // StrTrimW // TODO: Remove once fileUtilsReadFromFile accepts a full path.


//
// FUNCTIONS
//

void htmlSpawnThread(StateGUI* pStateGUI, [[maybe_unused]] bool mainParsingThread, wchar_t* data, int64_t len) // TODO: Delete unused directive.
{
    // TODO: Make it spawn a new thread.

    MiniCfg* pMiniCfg = pStateGUI->pMiniCfg;

    appLogPrint(L"htmlSpawnThread.", APP_LOG_TO_CONSOLE);
    appLogPrintInt(len, APP_LOG_TO_CONSOLE);

    // See if we received raw data or a path to open.
    if (!len)
    {
        wchar_t inputPathOnly[MAX_PATH];
        wcscpy_s(inputPathOnly, MAX_PATH, data);
        PathRemoveFileSpecW(inputPathOnly); // Destructively splices inputPathOnly.

        // Open the file.
        if (!fileUtilsReadFromFile(inputPathOnly, PathFindFileNameW(data), (void **)&data, (size_t*)&len))  // TODO: Make fileUtilsReadFromFile accept a whole path and not parts of it.
                                                                                                            // TODO: Why a pointer to a pointer?
                                                                                                            // TODO: Is size_t the best option? 32-bits sometimes?
        {
            appLogPrint(L"Couldn't open specified input file.", APP_LOG_TO_CONSOLE);
            parserCommonFinished(pStateGUI, 0);
            return;
        }
    }

    if (len)
    {
        // free(data);
    }
    parserCommonFinished(pStateGUI, data);
}