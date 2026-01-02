
//
// DEPENDENCIES
//

#include <windows.h>
#include <stdint.h> // int64_t, uint32_t, etc.
#include <wchar.h> // swprintf_s, wcslen, etc.
#include <stdio.h> // wprintf
#include <shellapi.h> // Required for CommandLineToArgvW
#include <shlwapi.h> // Required for PathFindFileNameW
#include "minify_config.h"
#include "app_logging.h"
#include "main_window.h"
#include "file_utils.h"

//
// ENUMS
//

typedef enum enumOfArgs : int
{
    ARG_default,
    ARG_d,
    ARG_help,
    ARG_h,
    ARG_input,
    ARG_i,
    ARG_fallback,
    ARG_f,
    ARG_no_fallback,
    ARG_nf,
    ARG_mangle,
    ARG_m,
    ARG_no_mangle,
    ARG_nm,
    ARG_random_mangle,
    ARG_rm,
    ARG_no_random_mangle,
    ARG_nrm,
    ARG_html,
    ARG_css,
    ARG_js,
    ARG_auto_detect,
    ARG_ad,
    ARG_no_out_file,
    ARG_nof,
    ARG_out_strip,
    ARG_os,
    ARG_out_path,
    ARG_op,
    ARG_out_file,
    ARG_of,
    ARG_load,
    ARG_l,
    ARG_save,
    ARG_s,
    ARG_close,
    ARG_c,
    ARG_headless,
    ARG_hd,
    ARG_run,
    ARG_r,

    countOfArgs
}enumOfArgs;
static_assert(  ARG_default < 2 && ARG_d < 2,
                "ERROR: ARG_default and ARG_d need to be at the top of enumOfArgs! Because when parsed it erases all previous config!" );
static_assert(  ARG_r >= countOfArgs - 2 && ARG_run >= countOfArgs - 2 && ARG_hd >= countOfArgs - 4 && ARG_headless >= countOfArgs - 4,
                "ERROR:  ARG_r, ARG_run, ARG_hd and ARG_headless need to be at the bottom of enumOfArgs!"
                "Because when parsed they start the minification process with current config!" );

//
// TYPE DEFINITIONS
//

typedef unsigned _BitInt(countOfArgs) ArgsBitField;

//
// CONFIGURATION CONSTANTS
//

static const wchar_t* const argStrs[] = {
    L"--default",
    L"-d",
    L"--help",
    L"-h",
    L"--input",
    L"-i",
    L"--fallback",
    L"-f",
    L"--no-fallback",
    L"-nf",
    L"--mangle",
    L"-m",
    L"--no-mangle",
    L"-nm",
    L"--random-mangle",
    L"-rm",
    L"--no-random-mangle",
    L"-nrm",
    L"--HTML",
    L"--CSS",
    L"--JS",
    L"--auto-detect",
    L"-ad",
    L"--no-out-file",
    L"-nof",
    L"--out-strip",
    L"-os",
    L"--out-path",
    L"-op",
    L"--out-file",
    L"-of",
    L"--load",
    L"-l",
    L"--save",
    L"-s",
    L"--close",
    L"-c",
    L"--headless",
    L"-hd",
    L"--run",
    L"-r"
};
static_assert( _countof(argStrs) == countOfArgs, "Count mismatch: Match argStrs[] and enumOfArgs!" );

static constexpr wchar_t CONFIG_FILENAME[] = L"CONFIG";
static constexpr unsigned char HELP_INFO[] = { 
    #embed "cli_help.txt" limit(3000) // 3000 characters max. File with Windows-1252 encoding.
 }; 
static constexpr unsigned char buildIDBytes[] = { 
    #embed CMAKE_PATH_BUILD_ID_BIN limit(8) // 8 bytes, ie. 64 bits.
 }; 

//
// FUNCTIONS
//

void miniCfgSave(MiniCfg* pMiniCfg)
{
    wchar_t path[MAX_PATH] = { };
    if (fileUtilsGetAppDataPath(path))
    {
        wchar_t fullPath[MAX_PATH];
        PathCombineW(fullPath, path, CONFIG_FILENAME);
        if (fileUtilsSaveToFile(fullPath, (LPCVOID)pMiniCfg, sizeof(MiniCfg)))
        {
            appLogPrint(L"Settings saved.", APP_LOG_TO_CONSOLE);
            return;
        }
    }

    appLogPrint(L"Failed to save settings.", APP_LOG_TO_CONSOLE);    
}

static void internalLoadDefaults(_Out_ MiniCfg* pMiniCfg, _In_ wchar_t* fullPath)
{
    pMiniCfg->flagHeadless = false;
    pMiniCfg->prevPath[0] = L'\0';
    pMiniCfg->inputType = radioButtonHTML;
    pMiniCfg->fallbackToPrevFile = false;
    pMiniCfg->mangle = true;
    pMiniCfg->randomMangle = true;
    pMiniCfg->outOpt = radioButtonOutFileStrip;
    pMiniCfg->stripSeg[0] = L'\0';
    pMiniCfg->outPath[0] = L'\0';
    

    if (fullPath)
    {
        wcscpy_s(pMiniCfg->inPath, MAX_PATH, fullPath);

        pMiniCfg->outFilename = true;
        // Get pointer to the start of the filename (e.g. index.html").
        wchar_t* nameStart = PathFindFileNameW(fullPath);

        // Get pointer to the start of the extension (e.g., ".html").
        wchar_t* extStart = PathFindExtensionW(nameStart);

        // Calculate length of the name excluding the extension.
        int nameLen = (int)(extStart - nameStart);

        // Construct the default filename string using "%.*s".
        //    %.*s takes two arguments: the length to print, and the string.
        swprintf_s(pMiniCfg->outFile, MAX_PATH, L"%.*s_min%s", nameLen, nameStart, extStart);
    }
    else
    {
        pMiniCfg->inPath[0] = L'\0';
        pMiniCfg->outFilename = false;
        pMiniCfg->outFile[0] = L'\0';
    }
}

void miniCfgLoad(MiniCfg* pMiniCfg, StateGUI* pStateGUI)
{
    bool isStartup = false;
    if (!(pStateGUI->pMiniCfg))
    {
        isStartup = true;

        // Make stateGUI struct hold the pointer pMiniCfg.
        pStateGUI->pMiniCfg = pMiniCfg;

        // Hold a unique build ID to avoid loading outdated settings files.
        memcpy(&pMiniCfg->buildUniqueID, buildIDBytes, sizeof(pMiniCfg->buildUniqueID));
    }

    bool loadedSuccessfully = false;
    wchar_t path[MAX_PATH] = { };
    if (fileUtilsGetAppDataPath(path))
    {
        void* buffer = nullptr;
        size_t len = 0;
        wchar_t fullPath[MAX_PATH];
        PathCombineW(fullPath, path, CONFIG_FILENAME);
        if (fileUtilsReadFromFile(fullPath, nullptr, &buffer, &len))
        {
            if (buffer && len == sizeof(MiniCfg))
            {
                // Check the unique build ID of the file to load corresponds to current version.
                if (((MiniCfg*)buffer)->buildUniqueID == pMiniCfg->buildUniqueID)
                {                    
                    // Copy the raw bytes from the read buffer into the struct.
                    memcpy(pMiniCfg, buffer, sizeof(MiniCfg)); // TODO: Ensure padding and alignment or opt for a different saving/loading logic.
                    loadedSuccessfully = true;
                }
                else
                {
                    appLogPrint(L"Settings from an older build detected.", APP_LOG_TO_CONSOLE);
                }
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

	internalLoadDefaults(pMiniCfg, nullptr);

    mainWindowUpdateControls(pStateGUI);
}

typedef struct StringParams
{
    wchar_t paramInPath[MAX_PATH];
    wchar_t paramStripSeg[MAX_PATH];
    wchar_t paramOutPath[MAX_PATH];
    wchar_t paramOutFile[MAX_PATH];
}StringParams;
static bool internalHelperParseCLI( _In_ StateGUI* pStateGUI, _Inout_ MiniCfg* pMiniCfg, _In_ ArgsBitField* pArgsBitField, StringParams* pStringParams)
{

    wchar_t pathValidationBuffer[MAX_PATH];

    // Parse all arguments.
    for (int i = 0; i < countOfArgs; i++)
    {
        // If the bit for a flag is not set, continue.
        if (!(*pArgsBitField & (((ArgsBitField)1) << i))) continue; // Cast 1 to ArgsBitField before possibly shifting it more than 32 places.

        switch (i)
        {
        case ARG_default:
        case ARG_d:
        {
            // If a valid input path was specified, forward it.
            if (pStringParams->paramInPath[0] && GetFullPathNameW(pStringParams->paramInPath, MAX_PATH, pathValidationBuffer, NULL))
            {
                internalLoadDefaults(pMiniCfg, pathValidationBuffer);
            }
            else
            {
                internalLoadDefaults(pMiniCfg, nullptr);
            }
            break;
        }
        case ARG_help:
        case ARG_h:
        {
            printf("%s", HELP_INFO); // Print CLI help.
            return false; // Don't draw the UI. Terminate now.
            break;
        }
        case ARG_input:
        case ARG_i:
        {
            // If a valid input path was specified, store it.
            if (pStringParams->paramInPath[0] && GetFullPathNameW(pStringParams->paramInPath, MAX_PATH, pathValidationBuffer, NULL))
            {
                wcscpy_s(pMiniCfg->inPath, MAX_PATH, pathValidationBuffer);
            }
            break;
        }
        case ARG_fallback:
        case ARG_f:
        {
            pMiniCfg->fallbackToPrevFile = true;
            break;
        }
        case ARG_no_fallback:
        case ARG_nf:
        {
            pMiniCfg->fallbackToPrevFile = false;
            break;
        }
        case ARG_mangle:
        case ARG_m:
        {
            pMiniCfg->mangle = true;
            break;
        }
        case ARG_no_mangle:
        case ARG_nm:
        {
            pMiniCfg->mangle = false;
            break;
        }
        case ARG_random_mangle:
        case ARG_rm:
        {
            pMiniCfg->randomMangle = true;
            break;
        }
        case ARG_no_random_mangle:
        case ARG_nrm:
        {
            pMiniCfg->randomMangle = false;
            break;
        }
        case ARG_html:
        {
            pMiniCfg->inputType = radioButtonHTML;
            break;
        }
        case ARG_css:
        {
            pMiniCfg->inputType = radioButtonCSS;
            break;
        }
        case ARG_js:
        {
            pMiniCfg->inputType = radioButtonJS;
            break;
        }
        case ARG_auto_detect:
        case ARG_ad:
        {
            pMiniCfg->inputType = radioButtonAutodetect;
            break;
        }
        case ARG_no_out_file:
        case ARG_nof:
        {
            pMiniCfg->outOpt = radioButtonNoOutFile;
            break;
        }
        case ARG_out_strip:
        case ARG_os:
        {
            // If a strip segment was specified, store it.
            if (pStringParams->paramStripSeg[0])
            {
                pMiniCfg->outOpt = radioButtonOutFileStrip;
                wcscpy_s(pMiniCfg->stripSeg, MAX_PATH, pStringParams->paramStripSeg);
            }
            break;
        }
        case ARG_out_path:
        case ARG_op:
        {
            // If a valid output path was specified, store it.
            if (pStringParams->paramOutPath[0] && GetFullPathNameW(pStringParams->paramOutPath, MAX_PATH, pathValidationBuffer, NULL))
            {
                pMiniCfg->outOpt = radioButtonOutFilePath;
                wcscpy_s(pMiniCfg->outPath, MAX_PATH, pathValidationBuffer);
            }
            break;
        }
        case ARG_out_file:
        case ARG_of:
        {
            // If an output filename was specified, store it.
            if (pStringParams->paramOutFile[0])
            {
                pMiniCfg->outFilename = true;
                wcscpy_s(pMiniCfg->outFile, MAX_PATH, pStringParams->paramOutFile);
            }
            break;
        }
        case ARG_load:
        case ARG_l:
        {
            miniCfgLoad(pMiniCfg, pStateGUI); // Load last saved settings.
            break;
        }
        case ARG_save:
        case ARG_s:
        {
            miniCfgSave(pMiniCfg); // Save current settings.
            break;
        }
        case ARG_close:
        case ARG_c:
        {
            PostQuitMessage(0);
            return false;
        }
        case ARG_headless:
        case ARG_hd:
        {
            pMiniCfg->flagHeadless = true;
            break;
        }
        case ARG_run:
        case ARG_r:
        {
            break;
        }
        }
    }

    bool run = 0;

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

bool miniCfgParseCLI(MiniCfg* pMiniCfg, StateGUI* pStateGUI, PWSTR forwardedArgs)
{
    if (pMiniCfg->currentlyParsing)
    {
        appLogPrint(L"Forwarded arguments received but ignored. Parsing currently in progress.", APP_LOG_TO_CONSOLE);
    }

	// Split argument string into the individual constituent arguments. 
    int argc = 0;
    PWSTR* originalArgv;

    // The arguments can come from the WM_COPYDATA handling (other instance forwarding them),
    // or from a reall call with arguments (Minifier4.exe --help).
    if (forwardedArgs != NULL)  originalArgv = CommandLineToArgvW(forwardedArgs, &argc);
    else                        originalArgv = CommandLineToArgvW(GetCommandLineW(), &argc);

    // If called with no arguments, do render a GUI (return 1). Don't trigger a conversion.
    if (argc <= 1)
    {
        LocalFree(originalArgv); // Free the memory allocated by CommandLineToArgvW.
        return true;
    }

    // The executable PATH is always the first argument of GetCommandLineW() return value. Skip it.
    PWSTR* argv = originalArgv + 1;
    argc--;

    // Turn all received arguments into a C23 bitfield.
    ArgsBitField argsBitField = 0;

    StringParams stringParams = { };

    // For each argument.
    for (int a = 0; a < argc; a++)
    {   
        bool foundMatch = false;
        // Check against each flag.
        for(int f = 0; f < countOfArgs; f++)
        {
            if (!_wcsicmp(argv[a], argStrs[f]))
            {
                argsBitField |= (((ArgsBitField)1) << f); // Cast 1 to ArgsBitField before possibly shifting it more than 32 places.

                // Sprecial cases. For all flags that require a parameter; consume the next argument.
                if ((f == ARG_input || f == ARG_i) && ++a < argc) 
                {
                    wcscpy_s(stringParams.paramInPath, MAX_PATH, argv[a]); // Input path specified.
                }
                else if ((f == ARG_out_strip || f == ARG_os) && ++a < argc)
                {
                    wcscpy_s(stringParams.paramStripSeg, MAX_PATH, argv[a]); // PATH segment to stip specified.
                }
                else if ((f == ARG_out_path || f == ARG_op) && ++a < argc)
                {
                    wcscpy_s(stringParams.paramOutPath, MAX_PATH, argv[a]); // Complete output PATH specified.
                }
                else if ((f == ARG_out_file || f == ARG_of) && ++a < argc)
                {
                     wcscpy_s(stringParams.paramOutFile, MAX_PATH, argv[a]); // Custom output filename specified.
                }
                foundMatch = true;
                break;
            }
        }

        if (foundMatch) continue;

        wchar_t unrecognizedArg[MAX_PATH];
        swprintf_s(unrecognizedArg, MAX_PATH, L"ERROR: Unrecognized argument: %s", argv[a]);
        appLogError(unrecognizedArg);
        break;
    
    }

    // Free the memory allocated by CommandLineToArgvW.
    LocalFree(originalArgv);

    return internalHelperParseCLI( pStateGUI, pMiniCfg, &argsBitField, &stringParams);
}
