
//
// DEPENDENCIES
//

#include <windows.h>
#include <stdint.h> // int64_t, uint32_t, etc.
#include <stdio.h>
#include <string.h> // sprintf_s, strlen, etc.
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

static const char* const prohibitedNamesJS[] = {"do", "if", "in", "for" };
static constexpr char LETTERS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
static constexpr size_t LETTERS_CNT = sizeof(LETTERS) - 1;
static constexpr char ALPHANUM[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-_";
static constexpr size_t ALPHANUM_CNT = sizeof(ALPHANUM) - 1;

//
// STRUCTS
//

// Hold the shuffled (and non shuffled) alfabet to generate mangled names.
typedef struct NameGenerator
{
    char startWchars[LETTERS_CNT]; // a-z, A-Z
    char otherWchars[ALPHANUM_CNT];   // a-z, A-Z, 0-9, -, _
    char randStartWchars[LETTERS_CNT]; // a-z, A-Z
    char randOtherWchars[ALPHANUM_CNT];   // a-z, A-Z, 0-9, -, _
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
        appLogError("Error: Input file too large (> 2GB) for encoding conversion.");
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
        appLogError("Error: Encoding conversion failed.");
        return nullptr;
    }

    *pLen = (size_t)utf8Len;
    return utf8Data;
}

char* parserCommonGetPointerToUTF8(char* data, size_t* pLen, bool isPath)
{
    if (!pLen) return nullptr;

    void* rawBuffer = nullptr;
    UINT codePage = CP_UTF8; // Default assumption for forwarded data.

    // Scenario 1: "data" is a file path in the stack. We must read it.
    if (isPath)
    {
        char buffer[MAX_PATH] = { };
        GetFullPathNameA(data, MAX_PATH, buffer, nullptr);
        if (!fileUtilsReadFromFile(buffer, &codePage, &rawBuffer, pLen))
        {
            appLogPrint("Couldn't open specified input file.", APP_LOG_TO_CONSOLE);
            return nullptr;
        }
    }
    // Scenario 2: "data" is the actual content (forwarded from the UI).
    else
    {
        return data;
    }

    // Convert (or pass through) to UTF-8
    // internalConvertToUTF8 takes ownership of rawBuffer.
    return internalConvertToUTF8(rawBuffer, pLen, codePage);
}

static void internalSpawnParsingThread(_In_ StateGUI* pStateGUI, _In_ int extension, _In_ bool mainParsingThread, _In_ char* data, _In_ size_t len, _In_ bool isPath)
{
    // Determine based on extension what worker thread we need.
    typedef DWORD WINAPI (*WorkerSpawnThread)(LPVOID lpParam);
    WorkerSpawnThread selectedFunc = nullptr;

    if (extension == fExtHTML)      selectedFunc = htmlSpawnThread;
    else if (extension == fExtCSS)  selectedFunc = cssSpawnThread;
    else if (extension == fExtJS)   selectedFunc = jsSpawnThread;

    // Allocate memory for the argument on the heap.
    ParsingThreadArgs* args = (ParsingThreadArgs*)malloc(sizeof(ParsingThreadArgs));
    
    if (!args)
    {
        appLogError("Couldn't allocate memory for the \"ParsingThreadArgs\" struct with malloc() to spawn a thread.");
        return;
    }

    args->pStateGUI = pStateGUI;
    args->mainParsingThread = mainParsingThread;
    args->data = data;
    args->len = len;
    args->isPath = isPath;

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

static bool internalSelectFileParser(_In_ StateGUI* pStateGUI)
{
    MiniCfg* pMiniCfg = pStateGUI->pMiniCfg;

    char inputPathOnly[MAX_PATH];
    strcpy_s(inputPathOnly, MAX_PATH, pMiniCfg->inPath);
    PathRemoveFileSpecA(inputPathOnly); // Destructively splices inputPathOnly.
    char* inputFilename = PathFindFileNameA(pMiniCfg->inPath); // Returns a pointer to an index of pMiniCfg->inPath.
    char* inputExtension = PathFindExtensionA(inputFilename); // Returns a pointer to an index of pMiniCfg->inPath.

    if (!inputPathOnly[0] || !inputFilename[0] || !inputExtension[0]) return false;

    int extension = fExtInvalid;
    if (!_stricmp(inputExtension, ".html")) extension = fExtHTML;
    else if (!_stricmp(inputExtension, ".css")) extension = fExtCSS;
    else if (!_stricmp(inputExtension, ".js")) extension = fExtJS;

    // See if extension matches radio button selection HTML vs CSS vs JS vs auto
    if (pMiniCfg->inputType == radioButtonAutodetect || pMiniCfg->inputType == extension)
    {
        // Change working dir to specified path.
        SetCurrentDirectoryA(inputPathOnly);

        internalSpawnParsingThread(pStateGUI, extension, true, (char*)pMiniCfg->inPath, 0, true);
        return true;
    }
    
    appLogPrint("Input console path file extension not valid.", APP_LOG_TO_CONSOLE);
    return false;
}

// Helper: Checks if a string is non-null and not empty.
static inline bool internalIsValidString(_In_ const char* str)
{
    return (str != nullptr && str[0] != L'\0');
}

// Helper: Safely removes a specific directory segment from a path.
// Returns true if the segment was found and removed, false otherwise.
// Note: 'dest' and 'src' must NOT overlap.
static bool internalStripPathSegment(_Out_ char* dest, _In_ size_t destSize, _In_ const char* src, _In_ const char* segment)
{
    if (!dest || !src || !segment || !destSize) return false;

    // Safety: Protect against overlapping buffers which memcpy_s does not support.
    if (dest == src) return false;

    size_t segLen = strlen(segment);
    const char* pMatch = src;
    
    // Iterate through occurrences of 'segment' to find a whole-word match.
    while ((pMatch = strstr(pMatch, segment)) != nullptr)
    {
        // 1. Validate Preceding Character (Start of string or Path Separator).
        bool startOk = (pMatch == src) || (pMatch[-1] == L'\\') || (pMatch[-1] == L'/');

        // 2. Validate Following Character (End of string or Path Separator).
        char nextChar = pMatch[segLen];
        bool endOk = (nextChar == L'\0') || (nextChar == L'\\') || (nextChar == L'/');

        if (startOk && endOk)
        {
            size_t prefixLen = pMatch - src;
            
            if (prefixLen >= destSize) return false;

            // Copy the prefix.
            if (memcpy_s(dest, destSize, src, prefixLen) != 0) return false;
            dest[prefixLen] = L'\0';

            const char* rest = pMatch + segLen;

            // Logic to remove double separators
            if (prefixLen > 0 && (dest[prefixLen - 1] == L'\\' || dest[prefixLen - 1] == L'/') && 
               (*rest == L'\\' || *rest == L'/'))
            {
                rest++; 
            }
            else if (prefixLen == 0 && (*rest == L'\\' || *rest == L'/'))
            {
                rest++;
            }

            return (strcat_s(dest, destSize, rest) == 0);
        }
        pMatch++;
    }

    return false;
}

static void internalSaveAsFile(_In_ MiniCfg* pMiniCfg, _In_ char* minified, _In_ size_t len)
{
    // 1. Output Strategy Check.
    if (pMiniCfg->outOpt == radioButtonNoOutFile) return;

    char outputFullPath[MAX_PATH] = { };
    char tempPath[MAX_PATH] = { };

    // 2. Handle Directory Logic.
    if (pMiniCfg->outOpt == radioButtonOutFilePath)
    {
        // Must have a valid custom output path.
        if (!internalIsValidString(pMiniCfg->outPath))
        {
            appLogPrint("Custom output path null or invalid.", APP_LOG_TO_CONSOLE);
            return;
        }
        strcpy_s(outputFullPath, MAX_PATH, pMiniCfg->outPath);
    }
    else if (pMiniCfg->outOpt == radioButtonOutFileStrip)
    {
        if (!internalIsValidString(pMiniCfg->stripSeg))
        {
            appLogPrint("Segment to strip from output path null or invalid.", APP_LOG_TO_CONSOLE);
            return;
        }

        // Use temp buffer for manipulation.
        strcpy_s(tempPath, MAX_PATH, pMiniCfg->inPath);
        
        // Remove filename destructively to get the directory.
        PathRemoveFileSpecA(tempPath);

        // Attempt to strip the segment.
        if (!internalStripPathSegment(outputFullPath, MAX_PATH, tempPath, pMiniCfg->stripSeg))
        {
            appLogPrint("Failed to find the segment to strip from output path.", APP_LOG_TO_CONSOLE);
            return;
        }
    }

    // 3. Handle Filename Logic.
    char fileNameToUse[MAX_PATH];

    if (pMiniCfg->outFilename)
    {
        if (!internalIsValidString(pMiniCfg->outFile)) 
        {
            appLogPrint("Custom filename null or invalid.", APP_LOG_TO_CONSOLE);
            return;
        }
        strcpy_s(fileNameToUse, MAX_PATH, pMiniCfg->outFile);
    }
    else
    {
        strcpy_s(fileNameToUse, MAX_PATH, PathFindFileNameA(pMiniCfg->inPath));
    }

    // Combine Directory + Filename.
    PathCombineA(outputFullPath, outputFullPath, fileNameToUse);

    // Save file.
    fileUtilsSaveToFile(outputFullPath, minified, len);
}

void parserCommonFinished(StateGUI* pStateGUI, char* minified, size_t len)
{
    MiniCfg* pMiniCfg = pStateGUI->pMiniCfg;
    pMiniCfg->currentlyParsing = false;
    mainWindowEnableControls(pStateGUI, true); // Reenable right menu controls.
    appLogPrint("Minification finished.", APP_LOG_TO_CONSOLE);

    // If headless, do the outputting and terminate the app.
    if (pMiniCfg->flagHeadless)
    {
        if (minified)
        {
            internalSaveAsFile(pMiniCfg, minified, len);
            free(minified);
        }
        PostMessageA(pStateGUI->hwnds[mainWindow], WM_CLOSE, 0, 0); 
    }
    else
    {
        if (minified)
        {
            // Update fallback path.
            strcpy_s(pMiniCfg->prevPath, MAX_PATH, pMiniCfg->inPath);
            
            // Print to the output rich edit control.
            mainWindowReplaceRichText(pStateGUI->hwnds[richEditOutput], minified);

            // Do the outputting.
            internalSaveAsFile(pMiniCfg, minified, len);

            free(minified);
        }
    }
}

void parserCommonRun(StateGUI* pStateGUI)
{
    MiniCfg* pMiniCfg = pStateGUI->pMiniCfg;
    appLogPrint("Minification started.", APP_LOG_TO_CONSOLE);

    // See if this is headless.
    if (pMiniCfg->flagHeadless)
    {
        if (!pMiniCfg->inPath[0])
        {
            appLogError("Headless mode requested without input file. Aborting minification.");
            return;
        }
        internalSelectFileParser(pStateGUI);
        return;
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
            appLogError("No --input path specified. Aborting minification.");
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
        char* richInputContent = nullptr;
        do
        {
            // Allocate memory to get rich edit input contents.
            richInputContent = malloc(len + 1);

            if (!richInputContent)
            {
                appLogError("Error allocating memory for input rich edit control content with malloc().");
                break;
            }

            SendMessage(hInputRichEdit, WM_GETTEXT, (WPARAM)(len + 1), (LPARAM)richInputContent);

            // Remove starting and trailing whitespaces and return characters.
            StrTrimA(richInputContent, " \t\r\n");

            if (strlen(richInputContent) > MAX_PATH) break; // Process richInputContent as raw console content.
            
            // See if it's malformed to be a valid path.
            if (!strpbrk(richInputContent, "<>\"|?*\n\r"))
            {
                 if (GetFullPathNameA(richInputContent, 0, richInputContent, NULL)) validPath = true;
            }

            if (!validPath) break; // Process richInputContent as raw console content.

            strcpy_s(pMiniCfg->inPath, MAX_PATH, richInputContent);
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
                appLogPrint("Auto-detect option works for file paths only. Defaulting to HTML.", APP_LOG_TO_CONSOLE);
                extension = radioButtonHTML;
            }
            

            internalSpawnParsingThread(pStateGUI, extension, true, richInputContent, len, false);
            return; // No fallback for raw processing.
        }        
    }
    else
    {
        appLogPrint("No content on input control.", APP_LOG_TO_CONSOLE);
    }

    // See if defaulting is ok.
    if(pMiniCfg->fallbackToPrevFile && pMiniCfg->prevPath[0])
    {
        appLogPrint("Attempting fallback.", APP_LOG_TO_CONSOLE);
        sprintf_s(pMiniCfg->inPath, MAX_PATH, pMiniCfg->prevPath);
        if (internalSelectFileParser(pStateGUI)) return;
    }
    
    // If fallback is not configured, or was tried and failed, return disabled controls to normal.
    parserCommonFinished(pStateGUI, 0, 0);
}

// Initialize the generator by shuffling the alphabets and storing them in a static NameGenerator stuct.
static NameGenerator* internalInitGenerator()
{
    static NameGenerator gen = { };
    LARGE_INTEGER li;
    QueryPerformanceCounter(&li); // Enough to get different mangling every run of Minifier 4.
    uint64_t seed = (uint64_t)li.QuadPart;
    
    memcpy(gen.startWchars, LETTERS, LETTERS_CNT);
    memcpy(gen.otherWchars, ALPHANUM, ALPHANUM_CNT);
    memcpy(gen.randStartWchars, LETTERS, LETTERS_CNT);
    memcpy(gen.randOtherWchars, ALPHANUM, ALPHANUM_CNT);

    // Shuffle start wchars.
    for (uint8_t i = 0; i < LETTERS_CNT; i++)
    {
        seed ^= seed << 13; seed ^= seed >> 7; seed ^= seed << 17; // Xorshift
        uint8_t r = seed % LETTERS_CNT;
        char temp = gen.randStartWchars[i];
        gen.randStartWchars[i] = gen.randStartWchars[r];
        gen.randStartWchars[r] = temp;
    }

    // Shuffle other wchars.
    for (uint8_t i = 0; i < ALPHANUM_CNT; i++) {
        seed ^= seed << 13; seed ^= seed >> 7; seed ^= seed << 17;
        uint8_t r = seed % ALPHANUM_CNT;
        char temp = gen.randOtherWchars[i];
        gen.randOtherWchars[i] = gen.randOtherWchars[r];
        gen.randOtherWchars[r] = temp;
    }

    return &gen;
}

// Helper to check reserved JS words.
static bool internalNameIsAllowedInJS(_In_ const char* mangledName)
{
    bool nameAllowed = true;
    for (uint16_t i = 0; i < sizeof(prohibitedNamesJS); i++)
    {
        if (!strcmp(mangledName, prohibitedNamesJS[i])) nameAllowed = false;
    }
    return nameAllowed;
}

// Generates a deterministic short name for the given index.
// PRECONDITION: For new entries, 'index' must be the next sequential number (no gaps).
// Reason is some executions add an offset by means of skippedNameIndexes to future calls.
// Previously assigned indices may be requested at any time to retrieve the original result.
static NameGenerator* gen = nullptr;
static int skippedNameIndexes[sizeof(prohibitedNamesJS)] = { };
static int namesSkippedCount = 0;
static int skippedNameIndexesRand[sizeof(prohibitedNamesJS)] = { };
static int namesSkippedCountRand = 0;
static SRWLOCK getMangledNameRWLock = SRWLOCK_INIT; // Read-write lock.
static INIT_ONCE onceFlag = INIT_ONCE_STATIC_INIT;
// This runs exactly once to set up the generator and the mutex.
static BOOL CALLBACK internalGetMangledHelpRunOnce([[maybe_unused]] _In_opt_ PINIT_ONCE InitOnce, [[maybe_unused]] _In_opt_ PVOID Parameter, [[maybe_unused]] _In_opt_ PVOID* Context)
{
    gen = internalInitGenerator();
    return TRUE;
}
void parserCommonGetMangled(int index, char* buffer, bool rand)
{
    // Ensure gen is initialized in a thread-safe way.
    InitOnceExecuteOnce(&onceFlag, internalGetMangledHelpRunOnce, NULL, NULL);

    int* activeSkippedNameIndexes = skippedNameIndexes;
    int* activepNamesSkippedCount = &namesSkippedCount;
    char* activeStartWchars = gen->startWchars;
    char* activeOtherWchars = gen->otherWchars;

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
            if ((*activepNamesSkippedCount) == sizeof(prohibitedNamesJS))
            {
                appLogError("ERROR: skippedNameIndexes[namesSkippedCount] out of bounds inside getMangledNameByIndex().");
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