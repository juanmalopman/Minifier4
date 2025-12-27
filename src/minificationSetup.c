
//
// INCLUDES
//

#include "framework.h"
#include "minificationSetup.h"
#include "printRichEdit.h"
#include "GUI.h"

//
// GLOBAL VARIABLES
//


//
// FUNCTIONS
//

void loadMinificationSettings(MinOpt* pMinOpt)
{
	// TODO: Load last used settings.
	pMinOpt->alreadyPresentGUI = false;
    pMinOpt->flagNoGUI = false;
    pMinOpt->prevPath[0] = 0;
    pMinOpt->inPath[0] = 0;
    pMinOpt->inputType = radioButtonHTML;
    pMinOpt->defaultToPrevFile = true;
    pMinOpt->mangle = true;
    pMinOpt->outFile = radioButtonOutFileStrip;
    wcscpy_s(pMinOpt->stripSeg, MAX_PATH, L"dev");
    pMinOpt->outPath[0] = 0;
}

bool parseArgumentsCLI(MinOpt* pMinOpt, StateGUI* pStateGUI)
{
	// Split argument string into the individual constituent arguments. 
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    // If called with no arguments, do render a GUI (return 1). Don't trigger a conversion.
    if (argv <= 1) return 1;

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
            pMinOpt->flagNoGUI = true;
        }
        else if (_wcsicmp(argv[i], L"--input") == 0 && i + 1 < argc)
        {
            // Get the path to the file to minify.
            wcscpy_s(pMinOpt->inPath, MAX_PATH, argv[++i]);
        }
        else if (_wcsicmp(argv[i], L"--dfltToPrev") == 0)
        {
            pMinOpt->defaultToPrevFile = true;
        }
        else if (_wcsicmp(argv[i], L"--noDfltToPrev") == 0)
        {
            pMinOpt->defaultToPrevFile = false;
        }
        else if (_wcsicmp(argv[i], L"--mangle") == 0)
        {
            pMinOpt->mangle = true;
        }
        else if (_wcsicmp(argv[i], L"--noMangle") == 0)
        {
            pMinOpt->mangle = false;
        }
        else if (_wcsicmp(argv[i], L"--HTML") == 0)
        {
            pMinOpt->inputType = radioButtonHTML;
        }
        else if (_wcsicmp(argv[i], L"--JS") == 0)
        {
            pMinOpt->inputType = radioButtonJS;
        }
        else if (_wcsicmp(argv[i], L"--CSS") == 0)
        {
            pMinOpt->inputType = radioButtonCSS;
        }
        else if (_wcsicmp(argv[i], L"--noOutFile") == 0)
        {
        	// No output file to create.
            pMinOpt->outFile = radioButtonNoOutFile;
        }
        else if (_wcsicmp(argv[i], L"--outStrip") == 0 && i + 1 < argc)
        {
        	// PATH segment to stip specified.
            pMinOpt->outFile = radioButtonOutFileStrip;
            wcscpy_s(pMinOpt->stripSeg, MAX_PATH, argv[++i]);
        }
        else if (_wcsicmp(argv[i], L"--outPath") == 0 && i + 1 < argc)
        {
            // Complete output PATH specified.
            pMinOpt->outFile = radioButtonOutFilePath;
            wcscpy_s(pMinOpt->outPath, MAX_PATH, argv[++i]);
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
    if (pStateGUI->hwnds[countOfHwnd - 1])
    {
    	pMinOpt->alreadyPresentGUI = true;
    	// Set the GUI settings accordingly
    }


    if (pMinOpt->flagNoGUI && !(pMinOpt->alreadyPresentGUI))
    {
	    // Trigger a conversion right away.
        // minify(html)

        // Don't render a GUI, just terminate the process.
        alertPopup(L"About to terminate");
        return 0;
    }

    if (goNow)
    {
    	/* code */
    }
    
    
	// Send start command to the loop and wait for windows to be ready somehow? Checking hwnds[countOfHwnds - 1] maybe.
	// PostMessageW(MSGCUSTOM_MINIFY);
    
    return 1;
}