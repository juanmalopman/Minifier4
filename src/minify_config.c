
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
#include "file_utils.h"

//
// CONFIGURATION CONSTANTS
//

static const wchar_t* const prohibitedNamesJS[] = {L"do", L"if", L"in", L"for" };
static constexpr wchar_t LETTERS[] = L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
static constexpr size_t LETTERS_CNT = _countof(LETTERS) - 1;
static constexpr wchar_t ALPHANUM[] = L"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-_";
static constexpr size_t ALPHANUM_CNT = _countof(ALPHANUM) - 1;
static constexpr wchar_t CONFIG_FILENAME[] = L"CONFIG";

//
// FUNCTIONS
//

void miniCfgSave(MiniCfg* pMiniCfg)
{
    wchar_t path[MAX_PATH] = { };
    if (fileUtilsGetAppDataPath(path))
    {
        if (fileUtilsSaveToFile(path, CONFIG_FILENAME, (LPCVOID)pMiniCfg, sizeof(MiniCfg)))
        {
            appLogPrint(L"Settings saved.", APP_LOG_TO_CONSOLE);
            return;
        }
    }

    appLogPrint(L"Failed to save settings.", APP_LOG_TO_CONSOLE);    
}

void miniCfgLoad(MiniCfg* pMiniCfg, StateGUI* pStateGUI)
{
    bool isStartup = false;
    if (!(pStateGUI->pMiniCfg))  isStartup = true;

    // Make stateGUI struct hold the pointer pMiniCfg.
    pStateGUI->pMiniCfg = pMiniCfg;

    bool loadedSuccessfully = false;
    wchar_t path[MAX_PATH] = { };
    if (fileUtilsGetAppDataPath(path))
    {
        void* buffer = nullptr;
        size_t len = 0;
        if (fileUtilsReadFromFile(path, CONFIG_FILENAME, &buffer, &len))
        {
            if (buffer && len == sizeof(MiniCfg))
            {
                // Copy the raw bytes from the read buffer into the struct.
                memcpy(pMiniCfg, buffer, sizeof(MiniCfg));
                loadedSuccessfully = true;
            }

            if (buffer)
            {
                free(buffer);
            }
        }
    }


    if (loadedSuccessfully && isStartup) return;

    if (loadedSuccessfully)
    {
        appLogPrint(L"Settings loaded successfully.", APP_LOG_TO_CONSOLE);
        mainWindowUpdateControls(pStateGUI);
        return;
    }

    if (!isStartup)
    {
        appLogPrint(L"Failed to load settings.", APP_LOG_TO_CONSOLE);
        return;
    }

	pMiniCfg->alreadyPresentGUI = false;
    pMiniCfg->flagHeadless = false;
    pMiniCfg->prevPath[0] = 0;
    pMiniCfg->inPath[0] = 0;
    pMiniCfg->inputType = radioButtonHTML;
    pMiniCfg->defaultToPrevFile = true;
    pMiniCfg->mangle = true;
    pMiniCfg->outOpt = radioButtonOutFileStrip;
    pMiniCfg->stripSeg[0] = 0;
    pMiniCfg->outPath[0] = 0;

    mainWindowUpdateControls(pStateGUI);
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

    bool run = 0;

    // The executable PATH is always the first argument of GetCommandLineW() return value. Skip it with i = 1. 
    for (int i = 1; i < argc; ++i)
    {
    	if (!_wcsicmp(argv[i], L"--help") || !_wcsicmp(argv[i], L"-h"))
        {
            // TODO: Print help info.
        }
    	if (!_wcsicmp(argv[i], L"--run"))
        {
            run = true;
        }
        else if (!_wcsicmp(argv[i], L"--headless"))
        {
            pMiniCfg->flagHeadless = true;
        }
        else if (!_wcsicmp(argv[i], L"--input") && i + 1 < argc)
        {
            // Get the path to the file to minify.
            wcscpy_s(pMiniCfg->inPath, MAX_PATH, argv[++i]);
        }
        else if (!_wcsicmp(argv[i], L"--defaults"))
        {
            pMiniCfg->defaultToPrevFile = true;
        }
        else if (!_wcsicmp(argv[i], L"--no-defaults"))
        {
            pMiniCfg->defaultToPrevFile = false;
        }
        else if (!_wcsicmp(argv[i], L"--mangle"))
        {
            pMiniCfg->mangle = true;
        }
        else if (!_wcsicmp(argv[i], L"--no-mangle"))
        {
            pMiniCfg->mangle = false;
        }
        else if (!_wcsicmp(argv[i], L"--HTML"))
        {
            pMiniCfg->inputType = radioButtonHTML;
        }
        else if (!_wcsicmp(argv[i], L"--JS"))
        {
            pMiniCfg->inputType = radioButtonJS;
        }
        else if (!_wcsicmp(argv[i], L"--CSS"))
        {
            pMiniCfg->inputType = radioButtonCSS;
        }
        else if (!_wcsicmp(argv[i], L"--no-out-file"))
        {
        	// No output file to create.
            pMiniCfg->outOpt = radioButtonNoOutFile;
        }
        else if (!_wcsicmp(argv[i], L"--out-strip") && i + 1 < argc)
        {
        	// PATH segment to stip specified.
            pMiniCfg->outOpt = radioButtonOutFileStrip;
            wcscpy_s(pMiniCfg->stripSeg, MAX_PATH, argv[++i]);
        }
        else if (!_wcsicmp(argv[i], L"--out-path") && i + 1 < argc)
        {
            // Complete output PATH specified.
            pMiniCfg->outOpt = radioButtonOutFilePath;
            wcscpy_s(pMiniCfg->outPath, MAX_PATH, argv[++i]);
        }
        else if (!_wcsicmp(argv[i], L"--out-file") && i + 1 < argc)
        {
            // Custom output filename specified.
            pMiniCfg->outFileName = true;
            wcscpy_s(pMiniCfg->outFile, MAX_PATH, argv[++i]);
        }
        else if (!_wcsicmp(argv[i], L"--load"))
        {
            // Load last saved settings.
            miniCfgLoad(pMiniCfg, pStateGUI);
        }
        else if (!_wcsicmp(argv[i], L"--save"))
        {
            // Save current settings.
            miniCfgSave(pMiniCfg);
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

    if (pMiniCfg->flagHeadless && !(pMiniCfg->alreadyPresentGUI))
    {
	    // Trigger a minification right away. No use of checking --run in a --noGUI execution.
        // minify(html)

        // Don't render a GUI, just terminate the process.
        return false;
    }

    // Apply changes dictated by the just updated minifier options.
    if (pStateGUI) mainWindowUpdateControls(pStateGUI);

    if (run)
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