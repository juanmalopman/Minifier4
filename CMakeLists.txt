# CHANGED: CMake 3.28+ recommended for VS 2026 / Presets v8 support.
cmake_minimum_required(VERSION 3.28)

# --- Project Information ---
project(Minifier4 
    VERSION 4.0.0 
    DESCRIPTION "High-performance code minifier" 
    LANGUAGES C RC
)

# --- Set C Standard ---
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)

# --- Source Discovery ---
file(GLOB_RECURSE SRC_FILES "src/*.c")
file(GLOB_RECURSE INC_FILES "include/*.h")
file(GLOB_RECURSE RC_FILES "res/*.rc")

# --- Options ---
option(BUILD_HEADERS_ONLY "Check syntax of headers only" OFF)

# --- Generate Build ID ---
set(CONFIG_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/include/minify_config.h")
set(BUILD_ID_FILE "${CMAKE_CURRENT_BINARY_DIR}/miniCfgID.bin")

# Optimized Powershell command for speed
add_custom_command(
    OUTPUT "${BUILD_ID_FILE}"
    DEPENDS "${CONFIG_HEADER}"
    COMMAND powershell -noprofile -ExecutionPolicy Bypass -Command 
        "$h = [System.Security.Cryptography.SHA256]::Create().ComputeHash([System.IO.File]::ReadAllBytes('${CONFIG_HEADER}')); [IO.File]::WriteAllBytes('${BUILD_ID_FILE}', [byte[]]$h[0..3])"
    COMMENT "Generating hash of minify_config.h..."
    VERBATIM
)
add_custom_target(GenerateBuildID DEPENDS "${BUILD_ID_FILE}")

set_source_files_properties(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/minify_config.c"
    PROPERTIES OBJECT_DEPENDS "${BUILD_ID_FILE}"
)

# --- Define the Target ---
if(BUILD_HEADERS_ONLY)
    message(STATUS "Mode: Header Syntax Check Only")
    set_source_files_properties(${INC_FILES} PROPERTIES LANGUAGE C)
    add_library(${PROJECT_NAME} OBJECT ${INC_FILES})
else()
    add_executable(${PROJECT_NAME} WIN32 
        ${SRC_FILES}
        ${INC_FILES}
        ${RC_FILES}
        "${BUILD_ID_FILE}"
    )
    add_dependencies(${PROJECT_NAME} GenerateBuildID) 
endif()

# --- Include Directories ---
target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

# --- Libraries ---
target_link_libraries(${PROJECT_NAME} dwmapi uxtheme comctl32 shcore shlwapi shell32)

# --- Definitions ---
file(TO_CMAKE_PATH "${BUILD_ID_FILE}" BUILD_ID_PATH_SAFE)
target_compile_definitions(${PROJECT_NAME} PRIVATE
    _WIN32_WINNT=0x0A00
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    STRICT
    CINTERFACE
    COBJMACROS
    CMAKE_PATH_MINI_CFG_ID_BIN=\"${BUILD_ID_PATH_SAFE}\"
)

# --- Compiler Options ---
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
        /utf-8 /W4 /MP /D_CRT_SECURE_NO_WARNINGS
        $<$<CONFIG:Release>:/O2>
        /std:clatest /we5262
    )
    if(BUILD_HEADERS_ONLY)
        target_compile_options(${PROJECT_NAME} PRIVATE /Zs /wd4505 /wd4100 /wd4189)
    endif()
elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wall -Wextra -pedantic -std=c23
        $<$<CONFIG:Release>:-O3>
        $<$<CONFIG:Debug>:-g -O0>
    )
    if(BUILD_HEADERS_ONLY)
        target_compile_options(${PROJECT_NAME} PRIVATE -fsyntax-only -Wno-unused -Wno-pragma-once-outside-header)
    else()
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
    file(MAKE_DIRECTORY "${TEST_ROOT}/integration/temp")
    file(GLOB TEST_CASES "${TEST_ROOT}/integration/cases/*")

    foreach(CASE_FILE ${TEST_CASES})
        get_filename_component(CASE_NAME "${CASE_FILE}" NAME)
        set(EXPECTED_FILE "${TEST_ROOT}/integration/expected/${CASE_NAME}")
        if(EXISTS "${EXPECTED_FILE}")
            add_test(NAME "Integration.${CASE_NAME}"
                COMMAND ${CMAKE_COMMAND}
                -DEXE=$<TARGET_FILE:${PROJECT_NAME}>
                -DIN=${CASE_FILE}
                -DOUT_DIR=${TEST_ROOT}/integration/temp
                -DOUT_NAME=${CASE_NAME}
                -DEXP=${EXPECTED_FILE}
                -P "${TEST_SCRIPT}"
            )
        endif()
    endforeach()
endif()