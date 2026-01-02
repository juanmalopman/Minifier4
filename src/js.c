
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

//
// FUNCTIONS
//

void jsSpawnThread(StateGUI* pStateGUI, bool mainParsingThread, wchar_t* data, int64_t len)
{
    appLogPrintInt(mainParsingThread, APP_LOG_TO_CONSOLE);
    if (len)
    {
        // free(data);
    }
    parserCommonFinished(pStateGUI, data);
}