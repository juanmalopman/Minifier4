
//
// DEPENDENCIES
//

#include <windows.h>
#include <stdint.h> // int64_t, uint32_t, etc.
#include <wchar.h> // swprintf_s, wcslen, etc.
#include <shellapi.h> // Required for CommandLineToArgvW
#include "minify_config.h"
#include "app_logging.h"
#include "main_window.h"

//
// CONFIGURATION CONSTANTS
//

static const wchar_t* const prohibitedNamesJS[] = {L"do", L"if", L"in", L"for" };
static constexpr wchar_t LETTERS[] = L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
static constexpr size_t LETTERS_CNT = _countof(LETTERS) - 1;
static constexpr wchar_t ALPHANUM[] = L"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-_";
static constexpr size_t ALPHANUM_CNT = _countof(ALPHANUM) - 1;

//
// FUNCTIONS
//

void miniCfgInit(MiniCfg* pMiniCfg, StateGUI* pStateGUI)
{
	// TODO: Load last used settings.
	pMiniCfg->alreadyPresentGUI = false;
    pMiniCfg->flagNoGUI = false;
    pMiniCfg->prevPath[0] = 0;
    pMiniCfg->inPath[0] = 0;
    pMiniCfg->inputType = radioButtonHTML;
    pMiniCfg->defaultToPrevFile = true;
    pMiniCfg->mangle = true;
    pMiniCfg->outFile = radioButtonOutFileStrip;
    pMiniCfg->stripSeg[0] = 0;
    pMiniCfg->outPath[0] = 0;


    pStateGUI->pMiniCfg = pMiniCfg;
}

bool miniCfgParseCLI(MiniCfg* pMiniCfg, StateGUI* pStateGUI, PWSTR forwardedArgs)
{
	// Split argument string into the individual constituent arguments. 
    int argc = 0;
    PWSTR* argv;
    if (forwardedArgs != NULL)
    {
        argv = CommandLineToArgvW(forwardedArgs, &argc);
    }
    else
    {
        argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    }

    // If called with no arguments, do render a GUI (return 1). Don't trigger a conversion.
    if (argc <= 1)
    {
        LocalFree(argv); // Free the memory allocated by CommandLineToArgvW.
        return true;
    }

    bool goNow = 0;

    // The executable PATH is always the first argument of GetCommandLineW() return value. Skip it with i = 1. 
    for (int i = 1; i < argc; ++i)
    {
    	if (_wcsicmp(argv[i], L"--help") == 0 || _wcsicmp(argv[i], L"-h") == 0)
        {
            // Print help info.
        }
    	if (_wcsicmp(argv[i], L"--goNow") == 0)
        {
            goNow = true;
        }
        else if (_wcsicmp(argv[i], L"--noGUI") == 0)
        {
            pMiniCfg->flagNoGUI = true;
        }
        else if (_wcsicmp(argv[i], L"--input") == 0 && i + 1 < argc)
        {
            // Get the path to the file to minify.
            wcscpy_s(pMiniCfg->inPath, MAX_PATH, argv[++i]);
        }
        else if (_wcsicmp(argv[i], L"--dfltToPrev") == 0)
        {
            pMiniCfg->defaultToPrevFile = true;
        }
        else if (_wcsicmp(argv[i], L"--noDfltToPrev") == 0)
        {
            pMiniCfg->defaultToPrevFile = false;
        }
        else if (_wcsicmp(argv[i], L"--mangle") == 0)
        {
            pMiniCfg->mangle = true;
        }
        else if (_wcsicmp(argv[i], L"--noMangle") == 0)
        {
            pMiniCfg->mangle = false;
        }
        else if (_wcsicmp(argv[i], L"--HTML") == 0)
        {
            pMiniCfg->inputType = radioButtonHTML;
        }
        else if (_wcsicmp(argv[i], L"--JS") == 0)
        {
            pMiniCfg->inputType = radioButtonJS;
        }
        else if (_wcsicmp(argv[i], L"--CSS") == 0)
        {
            pMiniCfg->inputType = radioButtonCSS;
        }
        else if (_wcsicmp(argv[i], L"--noOutFile") == 0)
        {
        	// No output file to create.
            pMiniCfg->outFile = radioButtonNoOutFile;
        }
        else if (_wcsicmp(argv[i], L"--outStrip") == 0 && i + 1 < argc)
        {
        	// PATH segment to stip specified.
            pMiniCfg->outFile = radioButtonOutFileStrip;
            wcscpy_s(pMiniCfg->stripSeg, MAX_PATH, argv[++i]);
        }
        else if (_wcsicmp(argv[i], L"--outPath") == 0 && i + 1 < argc)
        {
            // Complete output PATH specified.
            pMiniCfg->outFile = radioButtonOutFilePath;
            wcscpy_s(pMiniCfg->outPath, MAX_PATH, argv[++i]);
        }
        else
        {
        	wchar_t unrecognizedArg[MAX_PATH];
        	swprintf_s(unrecognizedArg, MAX_PATH, L"ERROR: Unrecognized argument: %s", argv[i]);
        	appLogErrorPop(unrecognizedArg);
        }
    }

    // Free the memory allocated by CommandLineToArgvW
    LocalFree(argv);

    // See if there's a GUI already.
    if (pStateGUI && pStateGUI->hwnds[countOfHwnd - 1]) pMiniCfg->alreadyPresentGUI = true;

    if (pMiniCfg->flagNoGUI && !(pMiniCfg->alreadyPresentGUI))
    {
	    // Trigger a minification right away. No use of checking --goNow in a --noGUI execution.
        // minify(html)

        // Don't render a GUI, just terminate the process.
        return false;
    }

    // Apply changes dictated by the just updated minifier options.
    if (pStateGUI) mainWindowUpdateControls(pStateGUI);

    if (goNow)
    {
    	// Trigger a delayed minification. Post to wndProc and wait for windows to be ready somehow? Checking hwnds[countOfHwnds - 1] maybe.
        // PostMessageW(MSGCUSTOM_MINIFY);
    }

    return true;
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

    // Shuffle start wchars.
    for (uint8_t i = 0; i < LETTERS_CNT; i++)
    {
        seed ^= seed << 13; seed ^= seed >> 7; seed ^= seed << 17; // Xorshift
        uint8_t r = seed % LETTERS_CNT;
        wchar_t temp = gen.startWchars[i];
        gen.startWchars[i] = gen.startWchars[r];
        gen.startWchars[r] = temp;
    }

    // Shuffle other wchars.
    for (uint8_t i = 0; i < ALPHANUM_CNT; i++) {
        seed ^= seed << 13; seed ^= seed >> 7; seed ^= seed << 17;
        uint8_t r = seed % ALPHANUM_CNT;
        wchar_t temp = gen.otherWchars[i];
        gen.otherWchars[i] = gen.otherWchars[r];
        gen.otherWchars[r] = temp;
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
static SRWLOCK getMangledNameRWLock = SRWLOCK_INIT; // Read-write lock.
static INIT_ONCE onceFlag = INIT_ONCE_STATIC_INIT;
// This runs exactly once to set up the generator and the mutex.
static BOOL CALLBACK internalGetMangledHelpRunOnce([[maybe_unused]] _In_opt_ PINIT_ONCE InitOnce, [[maybe_unused]] _In_opt_ PVOID Parameter, [[maybe_unused]] _In_opt_ PVOID* Context)
{
    gen = internalInitGenerator();
    return TRUE;
}
void miniCfgGetMangled(int index, wchar_t* buffer)
{
    // Ensure gen is initialized in a thread-safe way.
    InitOnceExecuteOnce(&onceFlag, internalGetMangledHelpRunOnce, NULL, NULL);

    while (1)
    {
        uint64_t i = index;

        AcquireSRWLockShared(&getMangledNameRWLock);
        for (int j = 0; j < namesSkippedCount; j++)
        {
            if (index >= skippedNameIndexes[j]) i++;
        }
        ReleaseSRWLockShared(&getMangledNameRWLock);

        int pos = 0;

        // Logic: First wchar comes from startWchars.
        // Subsequent wchars come from otherWchars.
        
        // Determine first wcharacter.
        buffer[pos++] = gen->startWchars[i % LETTERS_CNT];
        i /= LETTERS_CNT;

        // Determine subsequent wcharacters
        while (i > 0)
        {
            i--; // Adjust for 0-index overlap in base conversion.
            buffer[pos++] = gen->otherWchars[i % ALPHANUM_CNT];
            i /= ALPHANUM_CNT;
        }
        
        buffer[pos] = '\0'; // Null terminate.

        // If this generated name is a not a reserved keyword, exit.
        if (internalNameIsAllowedInJS(buffer)) return;

        // If it is reserved, get a different one.
        AcquireSRWLockExclusive(&getMangledNameRWLock);
        // A different thread could have skipped this exact same index while we were processing.
        bool alreadySkipped = false;
        for (int j = 0; j < namesSkippedCount; j++)
        {
            if (index == skippedNameIndexes[j])
            {
                alreadySkipped = true;
                break;
            }
        }
        if (!alreadySkipped)
        {
            if (namesSkippedCount == _countof(prohibitedNamesJS))
            {
                appLogErrorPop(L"ERROR: skippedNameIndexes[namesSkippedCount] out of bounds inside getMangledNameByIndex().");
                index++;
                continue;
            }
            skippedNameIndexes[namesSkippedCount] = index; // This index must be skipped.
            namesSkippedCount++;
        }
        ReleaseSRWLockExclusive(&getMangledNameRWLock);

        // We now test the following index.
        index++;    
    }
}