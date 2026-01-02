
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

void parserCommonSpawnParsingThread(StateGUI* pStateGUI, int extension, bool mainParsingThread, wchar_t* data, int64_t len)
{
    // Spawn the right thread. If len == 0 , data holds a path.
    if (extension == fExtHTML) htmlSpawnThread(pStateGUI, mainParsingThread, data, len);
    else if (extension == fExtCSS) cssSpawnThread(pStateGUI, mainParsingThread, data, len);
    else if (extension == fExtJS) jsSpawnThread(pStateGUI, mainParsingThread, data, len);
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

void parserCommonFinished(StateGUI* pStateGUI, wchar_t* minified)
{
    MiniCfg* pMiniCfg = pStateGUI->pMiniCfg;
    pMiniCfg->currentlyParsing = false;
    mainWindowEnableControls(pStateGUI, true); // Reenable right menu controls.
    appLogPrint(L"Minification finished.", APP_LOG_TO_CONSOLE);

    // If headless, terminate the app.
    if (pMiniCfg->flagHeadless)
    {
        free(minified);
        PostQuitMessage(0);
    }
    else
    {
        if (minified)
        {
            // Update fallback path.
            wcscpy_s(pMiniCfg->prevPath, MAX_PATH, pMiniCfg->inPath);
             
            // Update output rich edit control. // TODO: You'll be working with UTF-8 most of the time. Parse in UTF-8 and convert files that are not to it. 
            size_t size_needed = MultiByteToWideChar(CP_UTF8, 0, (char*)minified, -1, NULL, 0);

            if (size_needed == 0)
            {
                return;
                free(minified);
            }

            wchar_t *dest = (wchar_t *)malloc((size_needed + 1) * sizeof(wchar_t));

            if (!dest)
            {
                return;
                free(minified);
            }

            MultiByteToWideChar(CP_ACP, 0, (char*)minified, -1, dest, size_needed);

            mainWindowReplaceRichText(pStateGUI->hwnds[richEditOutput], dest);
            free(dest);
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

    
    LRESULT len = SendMessage(hInputRichEdit, WM_GETTEXTLENGTH, 0, 0);
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

            swprintf_s(pMiniCfg->inPath, MAX_PATH, richInputContent);
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