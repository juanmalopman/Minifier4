
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


//
// FUNCTIONS
//

DWORD WINAPI htmlSpawnThread(LPVOID lpParam)
{
    ParsingThreadArgs* args = (ParsingThreadArgs*)lpParam;
    StateGUI* pStateGUI = args->pStateGUI;
    bool mainParsingThread = args->mainParsingThread;
    wchar_t* data = args->data;
    size_t len = args->len;
    free(args);

    if (!mainParsingThread) appLogPrint(L"Not mainParsingThread", APP_LOG_TO_CONSOLE);

    // MiniCfg* pMiniCfg = pStateGUI->pMiniCfg;

    // Get the pointer we need, to data forwarded or by opening a file, already in UTF-8.
    char* pD = parserCommonGetPointerToUTF8(data, &len);
    if (!pD)
    {
        appLogPrint(L"HTML thead failed to get a pointer to valid data to parse.", APP_LOG_TO_CONSOLE);
        parserCommonFinished(pStateGUI, 0, 0);
    }

    // Indexes to inject code later or aid parsing.
    size_t iCSS = 0;
    size_t iFold = 0;
    size_t iJS = 0;
    size_t iError[2] = { };
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

                        // Check against special tags. // TODO: just check for </head> and </body> to skip being so reliant in tags.
                        if (!_strnicmp(&pD[commentStart], htmlTagCSS, sizeof(htmlTagCSS) - 1))
                        {
                            iCSS = o;
                        }
                        else if (!_strnicmp(&pD[commentStart], htmlTagFold, sizeof(htmlTagFold) - 1))
                        {
                            iFold = o;
                        }
                        else if (!_strnicmp(&pD[commentStart], htmlTagJS, sizeof(htmlTagJS) - 1))
                        {
                            iJS = o;
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
                        i += sizeof(styleEndTag) - 1;
                        break;
                    }
                    else if (!_strnicmp(&pD[i], scriptEndTag, sizeof(scriptEndTag) - 1))
                    {
                        bufferLen = i - startIndex;
                        i += sizeof(scriptEndTag) - 1;
                        isCSS = false;
                        break;
                    }
                }
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

            // TODO: Spawn the right worker thread. And erase the code under here.
            int requiredSize = MultiByteToWideChar(CP_UTF8, 0, buffer, -1, nullptr, 0);
            if (requiredSize > 0)
            {
                wchar_t* wideBuffer = (wchar_t*)malloc(requiredSize * sizeof(wchar_t) - 1);
                if (wideBuffer)
                {
                    MultiByteToWideChar(CP_UTF8, 0, buffer, -1, wideBuffer, requiredSize);
                    // Use the wide string
                    if (isCSS) appLogPrint(wideBuffer, APP_LOG_TO_CONSOLE);
                    free(wideBuffer);
                }
            }
            free(buffer);

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

    // Ensure null termination without incrementing the index, to avoid written files to be null terminated.
    if (o < len) pD[o] = '\0';
     

    if (iCSS && iFold && iJS) appLogPrint(L"Avoid unused warnings. Erase this line.", APP_LOG_TO_CONSOLE);
    parserCommonFinished(pStateGUI, pD, o);
    return 0;
}