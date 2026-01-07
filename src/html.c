
//
// DEPENDENCIES
//

#include <windows.h>
#include <stdint.h> // int64_t, uint32_t, etc.
#include <stdio.h>
#include <string.h> // sprintf_s, strlen, etc.
#include "html.h"
#include "parser_common.h"
#include "main_window.h"
#include "minify_config.h"
#include "app_logging.h"
#include "css.h"
#include "js.h"

//
// STRUCTS
//

typedef struct ChildThreads
{
    ParsingThreadArgs parsingThreadArgs;
    bool isCSS;
    HANDLE hThread;
}ChildThreads;


typedef struct ContextHTML
{
    int iChildThread;
    ChildThreads* pChildThreads;
    size_t iCSS;
    size_t iFold;
    size_t iJS;
    size_t iError[2];
    size_t o;
}ContextHTML;

//
// CONFIGURATION CONSTANTS
//

static constexpr int nThreadBlock = 99; // How many thread stucts to allocate in memory at a time.

//
// FUNCTIONS
//

static bool internalAllocateChildThreadStructMem(_In_ int iChildThread, _Inout_ ChildThreads** pChildThreads, _In_ int nThreadBlock)
{
    // If there's no more memory to store child thread data, allocate.
    if (iChildThread % (nThreadBlock + 1) == nThreadBlock)
    {
        ChildThreads* temp = realloc(*pChildThreads, nThreadBlock * ((iChildThread / nThreadBlock) + 1) * sizeof(ChildThreads));
        if (!temp)
        {
            appLogError("Failed to allocate memory for helper CSS or JS thread with malloc().");
            return false;
        }
        *pChildThreads = temp;
    }
    return true;
}

static void internalQueueHelperThread(_Inout_ int* iChildThread, _Out_ ChildThreads* pChildThreads, _In_ bool isPath, _In_ bool isCSS, _In_ char* buffer, _In_ int bufferLen)
{
    pChildThreads[*iChildThread].parsingThreadArgs.pStateGUI = nullptr;
    pChildThreads[*iChildThread].parsingThreadArgs.mainParsingThread = false;
    pChildThreads[*iChildThread].parsingThreadArgs.data = buffer;
    pChildThreads[*iChildThread].parsingThreadArgs.len = bufferLen;
    pChildThreads[*iChildThread].parsingThreadArgs.isPath = isPath;
    pChildThreads[*iChildThread].isCSS = isCSS;
    (*iChildThread)++;
}

static bool internalSpawnHelperThread(_In_ int iChildThread, _Inout_ ChildThreads* pChildThreads)
{
    // Determine what helper thread we need.
    typedef DWORD WINAPI (*WorkerSpawnThread)(LPVOID lpParam);
    WorkerSpawnThread selectedFunc = nullptr;

    selectedFunc = pChildThreads[iChildThread].isCSS ? cssSpawnThread : jsSpawnThread;
    pChildThreads[iChildThread].hThread = CreateThread(
        NULL,                               // Default security attributes.
        0,                                  // Default stack size.
        selectedFunc,                       // The thread to spwan.
        &(pChildThreads->parsingThreadArgs),// The argument struct.
        0,                                  // Default creation flags.
        NULL                                // Don't need the thread ID.
    );

    if (!pChildThreads[iChildThread].hThread)
    {
        appLogError("Failed to spawn helper thread.");
        return false;
    }

    return true;
}

static void internalParseHTML(_Inout_ ContextHTML* ctx, _Inout_ char* pD, _In_ size_t len)
{
    size_t lastWasNewline = 0;
    for (size_t i = 0; i < len; i++ )
    {
        switch (pD[i])
        {
        case '\b': // ---------------------------------------------------------------------------------- '\b'
        case '\a': // ---------------------------------------------------------------------------------- '\a'
        {
            // Tabs (\t), backspaces (\b) and alerts (\a) get skipped.

            // If last char was a newline, update lastWasNewline to skip any future whitespaces etc. as if '\b' and '\a' never existed.
            if (i && i - 1 == lastWasNewline) lastWasNewline = i;
            break;
        }

        case '\r': // ---------------------------------------------------------------------------------- '\r'
        case '\n': // ---------------------------------------------------------------------------------- '\n'
        {
            // Newline(\n) and return chars (\r) get skipped.

            // Store the index. Next iteration might find a whitespace and decide to skip it thanks to this info.
            lastWasNewline = i;
            break;
        }
        case '\t': // ---------------------------------------------------------------------------------- '\t'
        case ' ': // ----------------------------------------------------------------------------------- ' '
        {
            // Whitespaces ( ) and tabs (\t) that are next to a newline char get skipped.
            if (i && i - 1 == lastWasNewline)
            {
                lastWasNewline = i;
                break;
            }

            // If not, write them.
            goto doDefault;
            break;
        }
        case '!': // ----------------------------------------------------------------------------------- '!'
        {
            if (!i) goto doDefault; // A file starting with '!'.

            if (!(i + 2 < len)) goto doDefault; // File ends before <!-- fits.

            // Check if it's a comment.
            if (pD[i - 1] == '<' && pD[i + 1] == '-' && pD[i + 2] == '-')
            {
                // Erase last written char because it was part of a comment.
                ctx->o--;

                size_t commentStart = i - 1;

                // Skip the comment but also check if it's a special tag.
                int possibleParseMessageInComment = 1;
                for (; i < len; i++)
                {
                    possibleParseMessageInComment++;
                    if (pD[i] == '>' && pD[i - 1] == '-' && pD[i - 2] == '-')
                    {

                        // Check against special tags.
                        if (!_strnicmp(&pD[commentStart], htmlTagFold, sizeof(htmlTagFold) - 1))
                        {
                            ctx->iFold = ctx->o;
                        }
                        else if (!_strnicmp(&pD[commentStart], htmlTagError, sizeof(htmlTagError) - 1))
                        {
                            // First index for an error page title.
                            if (!ctx->iError[0]) ctx->iError[0] = ctx->o;
                            // Next for an error page banner in body.
                            else ctx->iError[1] = ctx->o;
                        }
                        
                        // Erase whitespaces after comments. User must avoid comments in the middle of strings.
                        lastWasNewline = i;

                        // Exit the loop.
                        break;
                    }
                }
            }
            // If not part of a comment.
            else
            {
                goto doDefault;
            }

            break;
        }
        case '<': // ------------------------------------------------------------------------------------------ '<'
        {
            // Check for style tags with inline CSS or <script> tags with inline JS.
            static constexpr char styleTag[] = "<style>";
            static constexpr char scriptTag[] = "<script>";
            static constexpr char styleEndTag[] = "</style>";
            static constexpr char scriptEndTag[] = "</script>";
            static constexpr char headEndTag[] = "</head>";
            static constexpr char bodyEndTag[] = "</body>";
            size_t startIndex;

            if (!(i + sizeof(scriptTag) - 1 < len)) goto doDefault;

            if (!_strnicmp(&pD[i], styleTag, sizeof(styleTag) - 1))
            {
                appLogPrint("Inline CSS found.", APP_LOG_TO_CONSOLE);
                i += sizeof(styleTag) - 1;
                startIndex = i;
            }
            else if (!_strnicmp(&pD[i], scriptTag, sizeof(scriptTag) - 1))
            {
                appLogPrint("Inline JS found.", APP_LOG_TO_CONSOLE);
                i += sizeof(scriptTag) - 1;
                startIndex = i;
            }
            else if (!_strnicmp(&pD[i], headEndTag, sizeof(headEndTag) - 1))
            {
                appLogPrint("Head end found.", APP_LOG_TO_CONSOLE);
                ctx->iCSS = ctx->o;
                goto doDefault;
            }
            else if (!_strnicmp(&pD[i], bodyEndTag, sizeof(bodyEndTag) - 1))
            {
                appLogPrint("Body end found.", APP_LOG_TO_CONSOLE);
                ctx->iJS = ctx->o;
                goto doDefault;
            }
            else
            {
                goto doDefault;
            }

            // Skip all the content up to next tag.
            bool isCSS = true;
            size_t bufferLen;

            for (; i < len; i++)
            {
                if (pD[i] == '<')
                {
                    if (!_strnicmp(&pD[i], styleEndTag, sizeof(styleEndTag) - 1))
                    {
                        bufferLen = i - startIndex;
                        // -1 because of the null termination and -1 because next iteration of the parsing loop will increment i.
                        i += sizeof(styleEndTag) - 1 - 1;
                        break;
                    }
                    else if (!_strnicmp(&pD[i], scriptEndTag, sizeof(scriptEndTag) - 1))
                    {
                        bufferLen = i - startIndex;
                        // -1 because of the null termination and -1 because next iteration of the parsing loop will increment i.
                        i += sizeof(scriptEndTag) - 1 - 1;
                        isCSS = false;
                        break;
                    }
                }
            }

            if (i == len - 1)
            {
                appLogPrint("ERROR: </style> or </script> tag never found.", APP_LOG_TO_CONSOLE);
                break;
            }

            // Allocate heap and store the CSS or JS.
            char* buffer = nullptr;
            buffer = malloc(bufferLen + 1); // +1 for null termination.
            if (!buffer)
            {
                appLogError("Failed to allocate memory for inline CSS or JS with malloc().");
                break;
            }
            memcpy(buffer, &pD[startIndex], bufferLen);
            buffer[bufferLen] = '\0'; // Null terminate the string.

            if(!internalAllocateChildThreadStructMem(ctx->iChildThread, &ctx->pChildThreads, nThreadBlock))
            {
                free(buffer);
                break;
            }

            internalQueueHelperThread(&ctx->iChildThread, ctx->pChildThreads, false, isCSS, buffer, bufferLen);

            break;
        }
        default: // ------------------------------------------------------------------------------------ default
        doDefault:
        {
            // By default, chars get written into the ouput.
            pD[ctx->o++] = pD[i];
            break;
        }
        }
    } 
}

static void internalStitching(_Inout_ ContextHTML* ctx, _Inout_ char* pD, _Out_ char** pO, _Out_ size_t* pTotalLen)
{
    // If there's nowhere valid to inject CSS and JS, terminate.
    if (!ctx->iCSS || !ctx->iJS || ctx->iJS <= ctx->iCSS)
    {
        free(pD);
        *pO = nullptr;
        *pTotalLen = 0;
        return;
    }

    // With all the classes and IDs registered, spawn all queued threads.
    for (int i = 0; i < ctx->iChildThread; i++)
    {
        internalSpawnHelperThread(i, ctx->pChildThreads);
    }

    // Wait for any helper thread spawned to finish it's work and terminate.
    if (ctx->iChildThread)
    {
        HANDLE* handleArray = malloc(ctx->iChildThread * sizeof(HANDLE));
        for (int i = 0; i < ctx->iChildThread; i++) handleArray[i] = ctx->pChildThreads[i].hThread;
        DWORD waitResult = WaitForMultipleObjects(ctx->iChildThread, handleArray, TRUE, 3000);

        if (waitResult == WAIT_TIMEOUT)
        {
            appLogError("ERROR: Helper threads not done processing 3 seconds later.");
            // TODO: Terminate them.
            free(pD);
            *pO = nullptr;
            *pTotalLen = 0;
            return;
        }
    }

    // See total output len.
    *pTotalLen = ctx->o;
    for (int i = 0; i < ctx->iChildThread; i++) *pTotalLen += ctx->pChildThreads[i].parsingThreadArgs.len; 

    // Allocate for the output.
    *pO = malloc(*pTotalLen);
    if (!*pO)
    {
        appLogError("Failed to allocate memory for entire output.");
        free(pD);
        *pO = nullptr;
        *pTotalLen = 0;
        return;
    }

    // Tie all helper threads output together.
    char* pCursor = *pO;

    memcpy(pCursor, pD, ctx->iCSS); // Copy HTML up to the CSS start.
    pCursor += ctx->iCSS;

    // Copy any CSS.
    for (int i = 0; i < ctx->iChildThread; i++)
    {
        if (ctx->pChildThreads[i].isCSS)
        {
            char* pData = ctx->pChildThreads[i].parsingThreadArgs.data;
            size_t len  = ctx->pChildThreads[i].parsingThreadArgs.len;

            if (pData != nullptr && len > 0)
            {
                memcpy(pCursor, pData, len);
                pCursor += len;
            }
        }
    }

    size_t lenMiddle = ctx->iJS - ctx->iCSS;
    memcpy(pCursor, pD + ctx->iCSS, lenMiddle); // Copy HTML up to the JS start.
    pCursor += lenMiddle;

    // Copy any JS.
    for (int i = 0; i < ctx->iChildThread; i++)
    {
        if (!ctx->pChildThreads[i].isCSS)
        {
            char* pData = ctx->pChildThreads[i].parsingThreadArgs.data;
            size_t len  = ctx->pChildThreads[i].parsingThreadArgs.len;

            if (pData != nullptr && len > 0)
            {
                memcpy(pCursor, pData, len);
                pCursor += len;
            }
        }
    }

    size_t lenFooter = ctx->o - ctx->iJS;
    memcpy(pCursor, pD + ctx->iJS, lenFooter + 1); // Copy HTML up to the end.

    free(pD);
}

DWORD WINAPI htmlSpawnThread(LPVOID lpParam)
{
    ParsingThreadArgs argsStack;
    memcpy(&argsStack, lpParam, sizeof(ParsingThreadArgs));
    free(lpParam);
    StateGUI* pStateGUI = argsStack.pStateGUI;
    size_t len = argsStack.len;

    // Get the pointer we need, to data forwarded or by opening a file, already in UTF-8.
    char* pD = parserCommonGetPointerToUTF8(argsStack.data, &len, argsStack.isPath);
    if (!pD)
    {
        appLogPrint("HTML thead failed to get a pointer to valid data to parse.", APP_LOG_TO_CONSOLE);
        parserCommonFinished(pStateGUI, 0, 0);
        return 0;
    }

    ContextHTML contextHTML = { };
    ContextHTML* ctx = &contextHTML;
    ctx->pChildThreads = malloc(nThreadBlock * sizeof(ChildThreads));

    internalParseHTML(ctx, pD, len);

    // Ensure null termination (without incrementing the index, to avoid written files to be null terminated).
    if (ctx->o < len) pD[ctx->o] = '\0';

    // If no helper CSS or JS threads to spawn, we're done.
    if (!ctx->iChildThread)
    {
        parserCommonFinished(pStateGUI, pD, ctx->o);
        return 0;
    }


    size_t totalLen;
    char* pO;
    internalStitching(ctx, pD, &pO, &totalLen);

    parserCommonFinished(pStateGUI, pO, totalLen);
    
    return 0;
}