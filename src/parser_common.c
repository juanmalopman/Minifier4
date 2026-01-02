
//
// DEPENDENCIES
//

#include <windows.h>
#include <stdint.h> // int64_t, uint32_t, etc.
#include <wchar.h> // swprintf_s, wcslen, etc.
#include <shlwapi.h> // StrTrimW
#include "parser_common.h"
#include "app_logging.h"
#include "main_window.h"
#include "minify_config.h"
#include "file_utils.h"
#include "html.h"
#include "css.h"
#include "js.h"


//
// CONFIGURATION CONSTANTS
//

static const wchar_t* const prohibitedNamesJS[] = {L"do", L"if", L"in", L"for" };
static constexpr wchar_t LETTERS[] = L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
static constexpr size_t LETTERS_CNT = _countof(LETTERS) - 1;
static constexpr wchar_t ALPHANUM[] = L"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-_";
static constexpr size_t ALPHANUM_CNT = _countof(ALPHANUM) - 1;

//
// STRUCTS
//

// Hold the shuffled (and non shuffled) alfabet to generate mangled names.
typedef struct NameGenerator
{
    wchar_t startWchars[LETTERS_CNT]; // a-z, A-Z
    wchar_t otherWchars[ALPHANUM_CNT];   // a-z, A-Z, 0-9, -, _
    wchar_t randStartWchars[LETTERS_CNT]; // a-z, A-Z
    wchar_t randOtherWchars[ALPHANUM_CNT];   // a-z, A-Z, 0-9, -, _
} NameGenerator;

//
// ENUMS
//

typedef enum fileExtension : int
{
    fExtInvalid,
    fExtHTML = radioButtonHTML,
    fExtCSS = radioButtonCSS,
    fExtJS = radioButtonJS
   
} fileExtension;

//
// FUNCTIONS
//

static char* internalConvertToUTF8(_Inout_ void* rawData, _Inout_ size_t* pLen, _In_ UINT codePage)
{
    if (!rawData || !pLen) return nullptr;

    // If already UTF-8, check if we have to skip the BOM (Byte Order Mark).
    if (codePage == CP_UTF8)
    {
        unsigned char* bytes = (unsigned char*)rawData;
        // Check for UTF-8 BOM (0xEF, 0xBB, 0xBF).
        if (*pLen >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF)
        {
            // Fast in-place shift. 
            // We overwrite the BOM by moving the rest of the data 3 bytes to the left.
            size_t newLen = *pLen - 3;
            memmove(bytes, bytes + 3, newLen);
            // Null-terminate the string on the new bytes at the end (that newLen doesn't count).
            bytes[newLen] = 0;
            *pLen = newLen;
        }

        return (char*)rawData;
    }

    // WinAPI Limitation: The file must not be larger than 2GB (INT_MAX) for MultiByteToWideChar.
    if (*pLen > (size_t)INT_MAX)
    {
        appLogError(L"Error: Input file too large (> 2GB) for encoding conversion.");
        free(rawData);
        return nullptr;
    }

    // Handle UTF-16BE by swapping in-place. Then treat it as standard Windows UTF-16LE.
    if (codePage == CP_UTF16BE)
    {
        uint16_t* ptr = (uint16_t*)rawData;
        size_t sizeInBytes = *pLen;
        size_t count = sizeInBytes / sizeof(uint16_t);
        
        for (size_t i = 0; i < count; i++)
        {
            // Swap logic (Big Endian <-> Little Endian) for UTF-16 BE files.
            uint16_t x = ptr[i];
            ptr[i] = (x << 8) | (x >> 8);
        }
        codePage = CP_UTF16LE; 
    }

    char* utf8Data = nullptr;
    size_t utf8Len = 0;

    // PATH A: Source is UTF-16LE (Native Windows Wide Char).
    if (codePage == CP_UTF16LE)
    {
        wchar_t* wideBuf = (wchar_t*)rawData;
        int wideLen = (int)(*pLen / sizeof(wchar_t));

        // Detect and skip BOM (Byte Order Mark).
        if (wideLen > 0 && wideBuf[0] == 0xFEFF)
        {
            wideBuf++;  // Advance pointer past the BOM
            wideLen--;  // Decrease the length to process
        }

        // Get required buffer size.
        utf8Len = WideCharToMultiByte(CP_UTF8, 0, wideBuf, wideLen, NULL, 0, NULL, NULL);
        
        if (utf8Len)
        {
            utf8Data = (char*)malloc(utf8Len + 1);
            if (utf8Data)
            {
                WideCharToMultiByte(CP_UTF8, 0, wideBuf, wideLen, utf8Data, utf8Len, NULL, NULL);
                utf8Data[utf8Len] = 0; // Null terminate
            }
        }
    }
    // PATH B: Source is ANSI / MultiByte (CP_ACP, Shift-JIS, etc.).
    else 
    {
        // 1. ANSI -> UTF-16
        int wideLen = MultiByteToWideChar(codePage, 0, (char*)rawData, (int)*pLen, NULL, 0);
        
        if (wideLen > 0)
        {
            wchar_t* tempWide = (wchar_t*)malloc((wideLen + 1) * sizeof(wchar_t));
            
            if (tempWide)
            {
                MultiByteToWideChar(codePage, 0, (char*)rawData, (int)*pLen, tempWide, wideLen);
                tempWide[wideLen] = 0;

                // 2. UTF-16 -> UTF-8
                utf8Len = WideCharToMultiByte(CP_UTF8, 0, tempWide, wideLen, NULL, 0, NULL, NULL);
                
                if (utf8Len > 0)
                {
                    utf8Data = (char*)malloc((size_t)utf8Len + 1);
                    if (utf8Data)
                    {
                        WideCharToMultiByte(CP_UTF8, 0, tempWide, wideLen, utf8Data, utf8Len, NULL, NULL);
                        utf8Data[utf8Len] = 0;
                    }
                }
                free(tempWide);
            }
        }
    }

    // Always free the original buffer, as we either failed or created a new UTF-8 one. (Unless CP_UTF8, which was returned early at the top).
    free(rawData);

    if (!utf8Data)
    {
        appLogError(L"Error: Encoding conversion failed.");
        return nullptr;
    }

    *pLen = (size_t)utf8Len;
    return utf8Data;
}

char* parserCommonGetPointerToUTF8(wchar_t* data, size_t* pLen)
{
    // TODO: Document clearly that the programmer must pass "data" to "free()" and pLen == 0; or the opposite.
    // data pointing to the stack and pLen != 0 will crash the app.

    if (!pLen) return nullptr;

    void* rawBuffer = nullptr;
    UINT codePage = CP_UTF16LE; // Default assumption for forwarded data (Windows L"internal string").

    // Scenario 1: "data" is a file path in the stack (pLen is 0). We must read it.
    if (*pLen == 0)
    {
        if (!fileUtilsReadFromFile(data, &codePage, &rawBuffer, pLen))
        {
            appLogPrint(L"Couldn't open specified input file.", APP_LOG_TO_CONSOLE);
            return nullptr;
        }
    }
    // Scenario 2: "data" is the actual content (forwarded from the UI).
    else
    {
        rawBuffer = (void*)data;
    }

    // Convert (or pass through) to UTF-8
    // internalConvertToUTF8 takes ownership of rawBuffer.
    return internalConvertToUTF8(rawBuffer, pLen, codePage);
}

void parserCommonSpawnParsingThread(StateGUI* pStateGUI, int extension, bool mainParsingThread, wchar_t* data, size_t len)
{
    // Determine based on extension what function we need.
    typedef DWORD WINAPI (*WorkerSpawnThread)(LPVOID lpParam);
    WorkerSpawnThread selectedFunc = nullptr;

    if (extension == fExtHTML)      selectedFunc = htmlSpawnThread;
    else if (extension == fExtCSS)  selectedFunc = cssSpawnThread;
    else if (extension == fExtJS)   selectedFunc = jsSpawnThread;

    // Allocate memory for the argument on the heap.
    ParsingThreadArgs* args = (ParsingThreadArgs*)malloc(sizeof(ParsingThreadArgs));
    
    if (args)
    {
        args->pStateGUI = pStateGUI;
        args->mainParsingThread = mainParsingThread;
        args->data = data;
        args->len = len;

        HANDLE hThread = CreateThread(
            NULL,               // Default security attributes.
            0,                  // Default stack size.
            selectedFunc,       // The thread to spwan.
            args,               // The argument struct.
            0,                  // Default creation flags.
            NULL                // Don't need the thread ID.
        );

        if (hThread) {
            CloseHandle(hThread); // We don't need to keep the handle open
        } else {
            free(args); // Thread creation failed, clean up.
        }
    }
    else
    {
        appLogError(L"Couldn't allocate memory for the \"ParsingThreadArgs\" struct with malloc() to spawn a thread.");
    }
}

static bool internalSelectFileParser(_In_ StateGUI* pStateGUI)
{
    MiniCfg* pMiniCfg = pStateGUI->pMiniCfg;

    wchar_t inputPathOnly[MAX_PATH];
    wcscpy_s(inputPathOnly, MAX_PATH, pMiniCfg->inPath);
    PathRemoveFileSpecW(inputPathOnly); // Destructively splices inputPathOnly.
    wchar_t* inputFilename = PathFindFileNameW(pMiniCfg->inPath); // Returns a pointer to an index of pMiniCfg->inPath.
    wchar_t* inputExtension = PathFindExtensionW(inputFilename); // Returns a pointer to an index of pMiniCfg->inPath.

    if (!inputPathOnly[0] || !inputFilename[0] || !inputExtension[0]) return false;

    int extension = fExtInvalid;
    if (!_wcsicmp(inputExtension, L".html")) extension = fExtHTML;
    else if (!_wcsicmp(inputExtension, L".css")) extension = fExtCSS;
    else if (!_wcsicmp(inputExtension, L".js")) extension = fExtJS;

    // See if extension matches radio button selection HTML vs CSS vs JS vs auto
    if (pMiniCfg->inputType == radioButtonAutodetect || pMiniCfg->inputType == extension)
    {
        parserCommonSpawnParsingThread(pStateGUI, extension, true, pMiniCfg->inPath, 0);
        return true;
    }
    
    appLogPrint(L"Input console path file extension not valid.", APP_LOG_TO_CONSOLE);
    return false;
}

void parserCommonFinished(StateGUI* pStateGUI, char* minified)
{
    MiniCfg* pMiniCfg = pStateGUI->pMiniCfg;
    pMiniCfg->currentlyParsing = false;
    mainWindowEnableControls(pStateGUI, true); // Reenable right menu controls.
    appLogPrint(L"Minification finished.", APP_LOG_TO_CONSOLE);

    // If headless, terminate the app.
    if (pMiniCfg->flagHeadless)
    {
        // TODO: Save output file.
        if (minified) free(minified);
        PostQuitMessage(0);
    }
    else
    {
        if (minified)
        {
            // Update fallback path.
            wcscpy_s(pMiniCfg->prevPath, MAX_PATH, pMiniCfg->inPath);
             
            mainWindowReplaceRichTextA(pStateGUI->hwnds[richEditOutput], minified);
            free(minified);
        }
    }
}

void parserCommonRun(StateGUI* pStateGUI)
{
    MiniCfg* pMiniCfg = pStateGUI->pMiniCfg;
    appLogPrint(L"Minification started.", APP_LOG_TO_CONSOLE);

    // See if this is headless.
    if (pMiniCfg->flagHeadless)
    {
        if (!pMiniCfg->inPath[0])
        {
            appLogError(L"Headless mode requested without input file. Aborting minification.");
            return;
        }
        internalSelectFileParser(pStateGUI);
    }

    // If this is not headless, see if there's content on top console.
    HWND hInputRichEdit = pStateGUI->hwnds[richEditInput];

    // If "still" headless (but !flagHeadless).
    if (!hInputRichEdit)
    {
        // Only inPath can hold a path.
        if (pMiniCfg->inPath[0])
        {
            internalSelectFileParser(pStateGUI);
        }
        else
        {
            appLogError(L"No --input path specified. Aborting minification.");
            // In this "still" headless session there can't be no prevPath yet.
            return;
        }
    }
    
    // If there's already a GUI.

    
    size_t len = SendMessage(hInputRichEdit, WM_GETTEXTLENGTH, 0, 0);
    if (len)
    {
        // Try to get a valid path first.
        bool validPath = false;
        wchar_t* richInputContent = nullptr;
        do
        {
            // Allocate memory to get rich edit input contents.
            size_t bufferSize = (len + 1) * sizeof(wchar_t);
            richInputContent = (wchar_t*)malloc(bufferSize);

            if (!richInputContent)
            {
                appLogError(L"Error allocating memory for input rich edit control content with malloc().");
                break;
            }

            SendMessage(hInputRichEdit, WM_GETTEXT, (WPARAM)(len + 1), (LPARAM)richInputContent);

            // Remove starting and trailing whitespaces and return characters.
            StrTrimW(richInputContent, L" \t\r\n");

            if (wcslen(richInputContent) > MAX_PATH) break; // Process richInputContent as raw console content.
            
            if (GetFullPathNameW(richInputContent, 0, richInputContent, NULL)) validPath = true; // See if it's malformed to be a valid path.

            if (!validPath) break; // Process richInputContent as raw console content.

            wcscpy_s(pMiniCfg->inPath, MAX_PATH, richInputContent);
            free(richInputContent); // No longer needed.
            
            // Parse the file. If successful, nothing else to do.
            if (internalSelectFileParser(pStateGUI)) return;

        } while(0);
        
        if (!validPath && richInputContent)
        {
            // Parse raw console content.
            int extension = pMiniCfg->inputType;
            if (extension == radioButtonAutodetect)
            {
                appLogPrint(L"Auto-detect option works for file paths only. Defaulting to HTML.", APP_LOG_TO_CONSOLE);
                extension = radioButtonHTML;
            }
            parserCommonSpawnParsingThread(pStateGUI, extension, true, richInputContent, len); // Spawned thread frees the memory when (len == true).
            return; // No fallback for raw processing.
        }        
    }
    else
    {
        appLogPrint(L"No content on input control.", APP_LOG_TO_CONSOLE);
    }

    // See if defaulting is ok.
    if(pMiniCfg->fallbackToPrevFile && pMiniCfg->prevPath[0])
    {
        appLogPrint(L"Attempting fallback.", APP_LOG_TO_CONSOLE);
        swprintf_s(pMiniCfg->inPath, MAX_PATH, pMiniCfg->prevPath);
        if (internalSelectFileParser(pStateGUI)) return;
    }
    
    // If fallback is not configured, or was tried and failed, return disabled controls to normal.
    parserCommonFinished(pStateGUI, 0);
}

// Initialize the generator by shuffling the alphabets and storing them in a static NameGenerator stuct.
static NameGenerator* internalInitGenerator()
{
    static NameGenerator gen = { };
    LARGE_INTEGER li;
    QueryPerformanceCounter(&li); // Enough to get different mangling every run of Minifier 4.
    uint64_t seed = (uint64_t)li.QuadPart;
    
    memcpy(gen.startWchars, LETTERS, LETTERS_CNT * sizeof(wchar_t));
    memcpy(gen.otherWchars, ALPHANUM, ALPHANUM_CNT * sizeof(wchar_t));
    memcpy(gen.randStartWchars, LETTERS, LETTERS_CNT * sizeof(wchar_t));
    memcpy(gen.randOtherWchars, ALPHANUM, ALPHANUM_CNT * sizeof(wchar_t));

    // Shuffle start wchars.
    for (uint8_t i = 0; i < LETTERS_CNT; i++)
    {
        seed ^= seed << 13; seed ^= seed >> 7; seed ^= seed << 17; // Xorshift
        uint8_t r = seed % LETTERS_CNT;
        wchar_t temp = gen.randStartWchars[i];
        gen.randStartWchars[i] = gen.randStartWchars[r];
        gen.randStartWchars[r] = temp;
    }

    // Shuffle other wchars.
    for (uint8_t i = 0; i < ALPHANUM_CNT; i++) {
        seed ^= seed << 13; seed ^= seed >> 7; seed ^= seed << 17;
        uint8_t r = seed % ALPHANUM_CNT;
        wchar_t temp = gen.randOtherWchars[i];
        gen.randOtherWchars[i] = gen.randOtherWchars[r];
        gen.randOtherWchars[r] = temp;
    }

    return &gen;
}

// Helper to check reserved JS words.
static bool internalNameIsAllowedInJS(_In_ const wchar_t* mangledName)
{
    bool nameAllowed = true;
    for (uint16_t i = 0; i < _countof(prohibitedNamesJS); i++)
    {
        if (!wcscmp(mangledName, prohibitedNamesJS[i])) nameAllowed = false;
    }
    return nameAllowed;
}

// Generates a deterministic short name for the given index.
// PRECONDITION: For new entries, 'index' must be the next sequential number (no gaps).
// Reason is some executions add an offset by means of skippedNameIndexes to future calls.
// Previously assigned indices may be requested at any time to retrieve the original result.
static NameGenerator* gen = nullptr;
static int skippedNameIndexes[_countof(prohibitedNamesJS)] = { };
static int namesSkippedCount = 0;
static int skippedNameIndexesRand[_countof(prohibitedNamesJS)] = { };
static int namesSkippedCountRand = 0;
static SRWLOCK getMangledNameRWLock = SRWLOCK_INIT; // Read-write lock.
static INIT_ONCE onceFlag = INIT_ONCE_STATIC_INIT;
// This runs exactly once to set up the generator and the mutex.
static BOOL CALLBACK internalGetMangledHelpRunOnce([[maybe_unused]] _In_opt_ PINIT_ONCE InitOnce, [[maybe_unused]] _In_opt_ PVOID Parameter, [[maybe_unused]] _In_opt_ PVOID* Context)
{
    gen = internalInitGenerator();
    return TRUE;
}
void parserCommonGetMangled(int index, wchar_t* buffer, bool rand)
{
    // Ensure gen is initialized in a thread-safe way.
    InitOnceExecuteOnce(&onceFlag, internalGetMangledHelpRunOnce, NULL, NULL);

    int* activeSkippedNameIndexes = skippedNameIndexes;
    int* activepNamesSkippedCount = &namesSkippedCount;
    wchar_t* activeStartWchars = gen->startWchars;
    wchar_t* activeOtherWchars = gen->otherWchars;

    if (rand)
    {
        activeSkippedNameIndexes = skippedNameIndexesRand;
        activepNamesSkippedCount = &namesSkippedCountRand;
        activeStartWchars = gen->randStartWchars;
        activeOtherWchars = gen->randOtherWchars;
    }

    while (1)
    {
        uint64_t i = index;

        AcquireSRWLockShared(&getMangledNameRWLock);
        for (int j = 0; j < (*activepNamesSkippedCount); j++)
        {
            if (index >= activeSkippedNameIndexes[j]) i++;
        }
        ReleaseSRWLockShared(&getMangledNameRWLock);

        int pos = 0;

        // Logic: First wchar comes from startWchars.
        // Subsequent wchars come from otherWchars.
        
        // Determine first wcharacter.
        buffer[pos++] = activeStartWchars[i % LETTERS_CNT];
        i /= LETTERS_CNT;

        // Determine subsequent wcharacters
        while (i > 0)
        {
            i--; // Adjust for 0-index overlap in base conversion.
            buffer[pos++] = activeOtherWchars[i % ALPHANUM_CNT];
            i /= ALPHANUM_CNT;
        }
        
        buffer[pos] = '\0'; // Null terminate.

        // If this generated name is a not a reserved keyword, exit.
        if (internalNameIsAllowedInJS(buffer)) return;

        // If it is reserved, get a different one.
        AcquireSRWLockExclusive(&getMangledNameRWLock);
        // A different thread could have skipped this exact same index while we were processing.
        bool alreadySkipped = false;
        for (int j = 0; j < (*activepNamesSkippedCount); j++)
        {
            if (index == activeSkippedNameIndexes[j])
            {
                alreadySkipped = true;
                break;
            }
        }
        if (!alreadySkipped)
        {
            if ((*activepNamesSkippedCount) == _countof(prohibitedNamesJS))
            {
                appLogError(L"ERROR: skippedNameIndexes[namesSkippedCount] out of bounds inside getMangledNameByIndex().");
                index++;
                continue;
            }
            activeSkippedNameIndexes[(*activepNamesSkippedCount)] = index; // This index must be skipped.
            (*activepNamesSkippedCount)++;
        }
        ReleaseSRWLockExclusive(&getMangledNameRWLock);

        // We now test the following index.
        index++;    
    }
}