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
set(CMAKE_C_EXTENSIONS OFF) 

# --- Source Discovery ---
file(GLOB_RECURSE SRC_FILES "src/*.c")
file(GLOB_RECURSE INC_FILES "include/*.h")
file(GLOB_RECURSE RC_FILES "rc/*.rc")

# --- Define the Executable ---
add_executable(${PROJECT_NAME} WIN32 
    ${SRC_FILES}
    ${INC_FILES}
    ${RC_FILES}
)

# --- Include Directories ---
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")

# --- Compiler Options ---
# dwmapi, uxtheme and comctl32 are for dark theme styling controls
target_link_libraries(${PROJECT_NAME} dwmapi uxtheme comctl32)

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
elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wall
        -Wextra
        -pedantic
        $<$<CONFIG:Release>:-O3>
        # Attempt C23 support
        -std=c2x 
    )
endif()

# --- Output Settings ---
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/bin/Debug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/bin/Release"
)