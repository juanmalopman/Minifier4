#pragma once

//
// CONFIGURATION CONSTANTS
//

#define RAW_IDI_APP_ICON 100

// Define things specifically for the .rc file without breaking the C application code.
#ifdef RC_INVOKED
    
    // Avoid linking the whole windows.h in resource.rc.
    // The Resource Compiler (rc.exe) fails to parse it.
    #ifndef RT_MANIFEST
        #define RT_MANIFEST 24 
    #endif
    #ifndef CREATEPROCESS_MANIFEST_RESOURCE_ID
        #define CREATEPROCESS_MANIFEST_RESOURCE_ID 1
    #endif

    // Application Resources (Raw Integers for RC).
    #define IDI_APP_ICON RAW_IDI_APP_ICON

#else
// Constants for the C23 application. <windows.h> will be linked and RT_MANIFEST etc. defined for us.

    static constexpr int IDI_APP_ICON = RAW_IDI_APP_ICON;

    #undef RAW_IDI_APP_ICON

#endif

