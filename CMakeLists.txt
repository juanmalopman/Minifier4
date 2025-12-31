# For support for C23 standards you need updated tools
cmake_minimum_required(VERSION 4.2)

# --- Project Information ---
project(Minifier4 
    VERSION 4.0.0 
    DESCRIPTION "High-performance code minifier" 
    LANGUAGES C RC
)

# --- Set C Standard ---
# CMake still complains if you set C23 here
# set(CMAKE_C_EXTENSIONS OFF)  # windows.h needs extensions on to work properly.

# --- Source Discovery ---
file(GLOB_RECURSE SRC_FILES "src/*.c")
file(GLOB_RECURSE INC_FILES "include/*.h")
file(GLOB_RECURSE RC_FILES "rc/*.rc")

# --- Options ---
option(BUILD_HEADERS_ONLY "Check syntax of headers only" OFF)

# --- Generate Build ID to #embed (Pre-Build Step) ---

#  Generate the file in the build.
set(BUILD_ID_FILE "${CMAKE_CURRENT_BINARY_DIR}/appUniqueBuildID.bin")

# Custom target forces the script inside to create a fresh new appUniqueBuildID.bin every build.
add_custom_target(GenerateBuildID
    # Tell Ninja this target produces this file to prevent race conditions
    BYPRODUCTS "${BUILD_ID_FILE}"
    
    # Use .Ticks (100-nanosecond precision) to fill all 64 bits
    COMMAND powershell -noprofile -ExecutionPolicy Bypass -Command 
        "[IO.File]::WriteAllBytes('${BUILD_ID_FILE}', [BitConverter]::GetBytes([int64]([DateTime]::UtcNow.Ticks)))"
    
    COMMENT "Generating fresh Build ID (Ticks)..."
    VERBATIM
)

# Strict compilation dependency (Fix any "file not found" race condition).
set_source_files_properties(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/minify_config.c"
    PROPERTIES OBJECT_DEPENDS "${BUILD_ID_FILE}"
)

# --- Define the Target ---
if(BUILD_HEADERS_ONLY)
    message(STATUS "Mode: Header Syntax Check Only")
    
    # 1. Tell CMake to treat .h files as .c files so the compiler processes them
    set_source_files_properties(${INC_FILES} PROPERTIES LANGUAGE C)
    
    # 2. Create an Object library (compiles only, does not link)
    #    We only include INC_FILES here to isolate headers.
    add_library(${PROJECT_NAME} OBJECT ${INC_FILES})

else()
    # Standard build
    add_executable(${PROJECT_NAME} WIN32 
        ${SRC_FILES}
        ${INC_FILES}
        ${RC_FILES}
        "${BUILD_ID_FILE}"
    )

    # Ensures the build ID file to #embed is created before compiling.
    add_dependencies(${PROJECT_NAME} GenerateBuildID) 
endif()

# --- Include Directories ---
target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

# --- Compiler Options ---
# dwmapi, uxtheme and comctl32 are for dark theme styling controls
# shcore is for getting monitor DPI
# shlwapi is for path strings handling and editing
target_link_libraries(${PROJECT_NAME} dwmapi uxtheme comctl32 shcore shlwapi)
add_compile_definitions(UNICODE _UNICODE)

# --- Convert /appUniqueBuildID.bin path to C-safe format (forward slashes) ---
file(TO_CMAKE_PATH "${BUILD_ID_FILE}" BUILD_ID_PATH_SAFE)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    _WIN32_WINNT=0x0A00   # Targets Windows 10
    WIN32_LEAN_AND_MEAN   # Exclude rare headers
    NOMINMAX              # Prevent min/max macros
    STRICT                # Type safety for handles
    CINTERFACE            # C-style COM interface
    COBJMACROS            # C-style COM macros

    CMAKE_PATH_BUILD_ID_BIN=\"${BUILD_ID_PATH_SAFE}\" # Custom definition to include a binary with #embed
)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
        /utf-8 
        /W4
        $<$<CONFIG:Release>:/O2>
        /MP
        /D_CRT_SECURE_NO_WARNINGS
        # Attempt C23 support
        /std:clatest
        /we5262 
    )
    if(BUILD_HEADERS_ONLY)
        target_compile_options(${PROJECT_NAME} PRIVATE 
            /Zs       # Syntax check only
            /wd4505   # Ignore "unreferenced local function"
            /wd4100   # Ignore "unreferenced formal parameter"
            /wd4189   # Ignore "local variable initialized but not referenced"
        )
    endif()

elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wall
        -Wextra
        -pedantic
        $<$<CONFIG:Release>:-O3>
        # Attempt C23 support
        -std=c23
    )
    if(BUILD_HEADERS_ONLY)
        target_compile_options(${PROJECT_NAME} PRIVATE 
            -fsyntax-only
            
            -Wno-pragma-once-outside-header         # Ignore '#pragma once' in main file.
            -Wno-unused                             # Ignore 'someVariable' defined but not used.
            -Werror=implicit-function-declaration   # Ensure missing includes cause a hard error
        )
    else()
        # Headers need no linking.
        target_link_options(${PROJECT_NAME} PRIVATE -municode) 
    endif() 
endif()

# --- Output Settings ---
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/bin/Debug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/bin/Release"
)