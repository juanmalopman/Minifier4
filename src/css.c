
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

DWORD WINAPI cssSpawnThread(LPVOID lpParam)
{
    ParsingThreadArgs* args = (ParsingThreadArgs*)lpParam;
    StateGUI* pStateGUI = args->pStateGUI;
    bool mainParsingThread = args->mainParsingThread;
    char* data = args->data;
    size_t len = args->len;
    bool isPath = args->isPath;
    if (mainParsingThread) free(args);

    // MiniCfg* pMiniCfg = pStateGUI->pMiniCfg;


    // Get the pointer we need, forwarded or by opening a file, already in UTF-8.
    char* pD = parserCommonGetPointerToUTF8(data, &len, isPath);

    // If this is the main thread, show the output.
    if (mainParsingThread) parserCommonFinished(pStateGUI, pD, len);

    // If not, update the pointers received and terminate.
    args->data = pD;
    args->len = len;
    return 0;
}