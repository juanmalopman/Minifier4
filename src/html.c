
//
// DEPENDENCIES
//

#include <windows.h>
#include <stdint.h> // int64_t, uint32_t, etc.
#include <wchar.h> // swprintf_s, wcslen, etc.
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

//
// FUNCTIONS
//

static bool internalAllocateChildThreadStructMem(_In_ int iChildThread, _Inout_ ChildThreads** pChildThreads, _In_ int nThreadBlock)
{
    // If there's no more memory to store child thread data, allocate.
    if (iChildThread % (nThreadBlock + 1) == nThreadBlock)
    {
        ChildThreads* temp = realloc(*pChildThreads, nThreadBlock * (iChildThread / nThreadBlock) * sizeof(ChildThreads));
        if (!temp)
        {
            appLogError(L"Failed to allocate memory for helper CSS or JS thread with malloc().");
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
        appLogError(L"Failed to spawn helper thread.");
        return false;
    }

    return true;
}

DWORD WINAPI htmlSpawnThread(LPVOID lpParam)
{
    ParsingThreadArgs* args = (ParsingThreadArgs*)lpParam;
    StateGUI* pStateGUI = args->pStateGUI;
    bool mainParsingThread = args->mainParsingThread;
    char* data = args->data;
    size_t len = args->len;
    bool isPath = args->isPath;
    free(args);

    if (!mainParsingThread) appLogPrint(L"Not mainParsingThread", APP_LOG_TO_CONSOLE);

    // MiniCfg* pMiniCfg = pStateGUI->pMiniCfg;

    // Get the pointer we need, to data forwarded or by opening a file, already in UTF-8.
    char* pD = parserCommonGetPointerToUTF8(data, &len, isPath);
    if (!pD)
    {
        appLogPrint(L"HTML thead failed to get a pointer to valid data to parse.", APP_LOG_TO_CONSOLE);
        parserCommonFinished(pStateGUI, 0, 0);
    }

    // A list of helper threads to queue.
    int iChildThread = 0;
    static constexpr int nThreadBlock = 99;
    ChildThreads* pChildThreads = malloc(nThreadBlock * sizeof(ChildThreads));

    // Indexes to inject code later or aid parsing.
    size_t iCSS = 0;
    [[maybe_unused]] size_t iFold = 0;
    size_t iJS = 0;
    [[maybe_unused]] size_t iError[2] = { };
    size_t lastWasNewline = 0; // When '\n' is found, store the index.
    size_t o = 0; // Output index.
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
                o--;

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
                            iFold = o;
                        }
                        else if (!_strnicmp(&pD[commentStart], htmlTagError, sizeof(htmlTagError) - 1))
                        {
                            // First index for an error page title.
                            if (!iError[0]) iError[0] = o;
                            // Next for an error page banner in body.
                            else iError[1] = o;
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
                appLogPrint(L"Inline CSS found.", APP_LOG_TO_CONSOLE);
                i += sizeof(styleTag) - 1;
                startIndex = i;
            }
            else if (!_strnicmp(&pD[i], scriptTag, sizeof(scriptTag) - 1))
            {
                appLogPrint(L"Inline JS found.", APP_LOG_TO_CONSOLE);
                i += sizeof(scriptTag) - 1;
                startIndex = i;
            }
            else if (!_strnicmp(&pD[i], headEndTag, sizeof(headEndTag) - 1))
            {
                appLogPrint(L"Head end found.", APP_LOG_TO_CONSOLE);
                iCSS = o;
                goto doDefault;
            }
            else if (!_strnicmp(&pD[i], bodyEndTag, sizeof(bodyEndTag) - 1))
            {
                appLogPrint(L"Body end found.", APP_LOG_TO_CONSOLE);
                iJS = o;
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
                appLogPrint(L"ERROR: </style> or </script> tag never found.", APP_LOG_TO_CONSOLE);
                break;
            }

            // Allocate heap and store the CSS or JS.
            char* buffer = nullptr;
            buffer = malloc(bufferLen + 1); // +1 for null termination.
            if (!buffer)
            {
                appLogError(L"Failed to allocate memory for inline CSS or JS with malloc().");
                break;
            }
            memcpy(buffer, &pD[startIndex], bufferLen);
            buffer[bufferLen] = '\0'; // Null terminate the string.

            if(!internalAllocateChildThreadStructMem(iChildThread, &pChildThreads, nThreadBlock))
            {
                free(buffer);
                break;
            }

            internalQueueHelperThread(&iChildThread, pChildThreads, false, isCSS, buffer, bufferLen);

            break;
        }
        default: // ------------------------------------------------------------------------------------ default
        doDefault:
        {
            // By default, chars get written into the ouput.
            pD[o++] = pD[i];
            break;
        }
        }
    }

    // Ensure null termination (without incrementing the index, to avoid written files to be null terminated).
    if (o < len) pD[o] = '\0';

    // If no helper CSS or JS threads to spawn, we're done.
    if (!iChildThread)
    {
        parserCommonFinished(pStateGUI, pD, o);
        return 0;
    }

    // If there's nowhere valid to inject CSS and JS, terminate.
    if (!iCSS || !iJS || iJS <= iCSS)
    {
        free(pD);
        parserCommonFinished(pStateGUI, nullptr, 0);
        return 0;
    }

    // With all the classes and IDs registered, spawn all queued threads.
    for (int i = 0; i < iChildThread; i++)
    {
        internalSpawnHelperThread(i, pChildThreads);
    }

    // Wait for any helper thread spawned to finish it's work and terminate.
    if (iChildThread)
    {
        HANDLE* handleArray = malloc(iChildThread * sizeof(HANDLE));
        for (int i = 0; i < iChildThread; i++) handleArray[i] = pChildThreads->hThread;
        DWORD waitResult = WaitForMultipleObjects(iChildThread, handleArray, TRUE, 3000);

        if (waitResult == WAIT_TIMEOUT)
        {
            appLogError(L"ERROR: Helper threads not done processing 3 seconds later.");
            // TODO: Terminate them.
        }
    }

    // See total output len.
    size_t totalLen = o;
    for (int i = 0; i < iChildThread; i++) totalLen += pChildThreads->parsingThreadArgs.len;

    // Allocate for the output.
    char* pO = malloc(totalLen);
    if (!pO)
    {
        appLogError(L"Failed to allocate memory for entire output.");
        free(pD);
        parserCommonFinished(pStateGUI, nullptr, 0);
        return 0;
    }

    // Tie all helper threads output together.
    char* pCursor = pO;

    memcpy(pCursor, pD, iCSS); // Copy HTML up to the CSS start.
    pCursor += iCSS;

    // Copy any CSS.
    for (int i = 0; i < iChildThread; i++)
    {
        if (pChildThreads[i].isCSS)
        {
            char* pData = pChildThreads[i].parsingThreadArgs.data;
            size_t len  = (size_t)pChildThreads[i].parsingThreadArgs.len;

            if (pData != nullptr && len > 0)
            {
                memcpy(pCursor, pData, len);
                pCursor += len;
            }
        }
    }

    size_t lenMiddle = iJS - iCSS;
    memcpy(pCursor, pD + iCSS, lenMiddle); // Copy HTML up to the JS start.
    pCursor += lenMiddle;

    // Copy any JS.
    for (int i = 0; i < iChildThread; i++)
    {
        if (!pChildThreads[i].isCSS)
        {
            char* pData = pChildThreads[i].parsingThreadArgs.data;
            size_t len  = (size_t)pChildThreads[i].parsingThreadArgs.len;

            if (pData != nullptr && len > 0)
            {
                memcpy(pCursor, pData, len);
                pCursor += len;
            }
        }
    }

    size_t lenFooter = (size_t)o - iJS;
    memcpy(pCursor, pD + iJS, lenFooter + 1); // Copy HTML up to the end.


    free(pD);
     
    parserCommonFinished(pStateGUI, pO, totalLen);
    return 0;
    
}