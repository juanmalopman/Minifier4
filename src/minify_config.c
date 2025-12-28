
//
// INCLUDES
//

#include "app_base.h"
#include <shellapi.h> // Required for CommandLineToArgvW
#include "minify_config.h"
#include "app_logging.h"
#include "main_window.h"

//
// GLOBAL VARIABLES
//


//
// FUNCTIONS
//

void loadMinificationSettings(MiniCfg* pMiniCfg, StateGUI* pStateGUI)
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

bool parseArgumentsCLI(StateGUI* pStateGUI, PWSTR forwardedArgs)
{
	// Split argument string into the individual constituent arguments. 
    int argc = 0;
    PWSTR* argv;
    if (forwardedArgs != nullptr)
    {
        argv = CommandLineToArgvW(forwardedArgs, &argc);
    }
    else
    {
        argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    }

    // If called with no arguments, do render a GUI (return 1). Don't trigger a conversion.
    if (argc <= 1) return 1;

    bool goNow = 0;
    auto pMiniCfg = pStateGUI->pMiniCfg;

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
        	errorPopup(unrecognizedArg);
        }
    }

    // Free the memory allocated by CommandLineToArgvW
    LocalFree(argv);

    // See if there's a GUI already.
    if (pStateGUI->hwnds[countOfHwnd - 1]) pMiniCfg->alreadyPresentGUI = true;

    if (pMiniCfg->flagNoGUI && !(pMiniCfg->alreadyPresentGUI))
    {
	    // Trigger a minification right away.
        // minify(html)

        // Don't render a GUI, just terminate the process.
        alertPopup(L"About to terminate");
        return 0;
    }

    // Apply changes dictated by the just updated minifier options.
    updateMenuSelections(pStateGUI);

    if (goNow)
    {
    	// Trigger a delayed minification. Post to wndProc and wait for windows to be ready somehow? Checking hwnds[countOfHwnds - 1] maybe.
        // PostMessageW(MSGCUSTOM_MINIFY);
    }
    
    

    
    return 1;
}