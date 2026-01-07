# 3.21+ is required for C23.
cmake_minimum_required(VERSION 3.21)

# --- Project Information ---
project(Minifier4 
    VERSION 4.0.0 
    DESCRIPTION "High-performance code minifier" 
    LANGUAGES C RC
)

# --- Set C Standard ---
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
# set(CMAKE_C_EXTENSIONS OFF)  # windows.h needs extensions on to work properly.

# --- Source Discovery ---
file(GLOB_RECURSE SRC_FILES "src/*.c")
file(GLOB_RECURSE INC_FILES "include/*.h")
file(GLOB_RECURSE RC_FILES "res/*.rc")

# --- Options ---
option(BUILD_HEADERS_ONLY "Check syntax of headers only" OFF)

# --- Generate Build ID to #embed (Pre-Build Step) ---
# It's a hash of minify_config.h (declares struct MiniCfg) to prevent for new app instances opening old setting files.

set(CONFIG_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/include/minify_config.h")
set(BUILD_ID_FILE "${CMAKE_CURRENT_BINARY_DIR}/miniCfgID.bin")

# 1. Use add_custom_command. This creates a dependency rule:
#    If 'minify_config.h' changes -> Run PowerShell -> Update 'miniCfgID.bin'
add_custom_command(
    OUTPUT "${BUILD_ID_FILE}"
    DEPENDS "${CONFIG_HEADER}"
    
    # 1. Compute SHA-256 (32 bytes).
    # 2. Select index [0..3] (first 4 bytes).
    # 3. Cast to [byte[]] explicitly so WriteAllBytes accepts it.
    COMMAND powershell -noprofile -ExecutionPolicy Bypass -Command 
        "$h = [System.Security.Cryptography.SHA256]::Create().ComputeHash([System.IO.File]::ReadAllBytes('${CONFIG_HEADER}')); [IO.File]::WriteAllBytes('${BUILD_ID_FILE}', [byte[]]$h[0..3])"
    
    COMMENT "Generating hash (int size) of minify_config.h..."
    VERBATIM
)

# 2. Keep the target name 'GenerateBuildID' so the dependencies below work.
#    This target now simply ensures the custom command above has run.
add_custom_target(GenerateBuildID DEPENDS "${BUILD_ID_FILE}")

# Strict compilation dependency.
# This forces 'minify_config.c' to recompile whenever the binary hash file changes.
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
# shell32 is for SHCreateDirectoryExW (Create directories recursively)
target_link_libraries(${PROJECT_NAME} dwmapi uxtheme comctl32 shcore shlwapi shell32)

# --- Convert /miniCfgID.bin path to C-safe format (forward slashes) ---
file(TO_CMAKE_PATH "${BUILD_ID_FILE}" BUILD_ID_PATH_SAFE)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    _WIN32_WINNT=0x0A00   # Targets Windows 10
    WIN32_LEAN_AND_MEAN   # Exclude rare headers
    NOMINMAX              # Prevent min/max macros
    STRICT                # Type safety for handles
    CINTERFACE            # C-style COM interface
    COBJMACROS            # C-style COM macros

    CMAKE_PATH_MINI_CFG_ID_BIN=\"${BUILD_ID_PATH_SAFE}\" # Custom definition to include a binary with #embed
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
        $<$<CONFIG:Debug>:-g -O0>
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
        target_link_options(${PROJECT_NAME} PRIVATE -static -static-libgcc) 
    endif() 
endif()

# --- Output Settings ---
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/bin/Debug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/bin/Release"
)

# --- Testing ---
if(NOT BUILD_HEADERS_ONLY)
    enable_testing()
    
    set(TEST_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    set(TEST_SCRIPT "${TEST_ROOT}/test_runner.cmake")
    
    # Ensure temp directory exists so Minifier has a valid target -op
    file(MAKE_DIRECTORY "${TEST_ROOT}/integration/temp")

    # Find test cases
    file(GLOB TEST_CASES "${TEST_ROOT}/integration/cases/*")

    foreach(CASE_FILE ${TEST_CASES})
        get_filename_component(CASE_NAME "${CASE_FILE}" NAME)
        
        set(EXPECTED_FILE "${TEST_ROOT}/integration/expected/${CASE_NAME}")
        set(TEMP_DIR      "${TEST_ROOT}/integration/temp")
        
        if(EXISTS "${EXPECTED_FILE}")
            add_test(NAME "Integration.${CASE_NAME}"
                COMMAND ${CMAKE_COMMAND}
                -DEXE=$<TARGET_FILE:${PROJECT_NAME}>
                -DIN=${CASE_FILE}
                -DOUT_DIR=${TEMP_DIR}
                -DOUT_NAME=${CASE_NAME}  # Used by script to find the file, not passed to CLI
                -DEXP=${EXPECTED_FILE}
                -P "${TEST_SCRIPT}"
            )
        else()
            message(WARNING "Skipping test case: ${CASE_NAME} (Missing 'expected' file)")
        endif()
    endforeach()
endif()