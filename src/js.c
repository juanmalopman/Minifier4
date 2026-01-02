
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

//
// FUNCTIONS
//

DWORD WINAPI jsSpawnThread(LPVOID lpParam)
{
    ParsingThreadArgs* args = (ParsingThreadArgs*)lpParam;
    StateGUI* pStateGUI = args->pStateGUI;
    bool mainParsingThread = args->mainParsingThread;
    wchar_t* data = args->data;
    size_t len = args->len;
    free(args);

    if (!mainParsingThread) appLogPrint(L"Not mainParsingThread", APP_LOG_TO_CONSOLE);
    
    // MiniCfg* pMiniCfg = pStateGUI->pMiniCfg;


    // Get the pointer we need, forwarded or by opening a file, already in UTF-8.
    char* pD = parserCommonGetPointerToUTF8(data, &len);


    parserCommonFinished(pStateGUI, pD);
    return 0;
}