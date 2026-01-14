
//
// DEPENDENCIES
//

#include <windows.h>
#include <stdint.h> // int64_t, uint32_t, etc.
#include <stdio.h>
#include <string.h> // sprintf_s, strlen, etc.
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
    char* pD = parserCommonGetPointerToUTF8(args->data, &args->len, args->isPath);
    if (!pD)
    {
        if (args->mainParsingThread) parserCommonFinished(args->pStateGUI, NULL, 0);
        free(args);
        return 0;
    }

    // If we are the MAIN parsing thread (Standalone mode).
    if (args->mainParsingThread)
    {
        parserCommonFinished(args->pStateGUI, pD, args->len);
        
        // Cleanup.
        free(args);
    } 
    else
    {
        args->data = pD;
    }

    return 0;
}