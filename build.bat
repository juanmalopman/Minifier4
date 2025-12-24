@echo off
setlocal

:: --- 1: Defaults ---
set "BUILD_TYPE=Debug"
set "COMPILER_MODE=MSVC"
set "TARGET_PRESET=debug"

:: --- 2: Argument Parsing Loop ---
:PARSE_ARGS
if "%~1"=="" goto :ARGS_DONE

:: Check for Compiler Flags
if /I "%~1"=="/gcc"   set "COMPILER_MODE=GCC" & shift & goto :PARSE_ARGS
if /I "%~1"=="/clang" set "COMPILER_MODE=CLANG" & shift & goto :PARSE_ARGS
if /I "%~1"=="/msvc"  set "COMPILER_MODE=MSVC" & shift & goto :PARSE_ARGS

:: Check for Build Type
if /I "%~1"=="release" set "BUILD_TYPE=Release" & shift & goto :PARSE_ARGS
if /I "%~1"=="debug"   set "BUILD_TYPE=Debug" & shift & goto :PARSE_ARGS

shift
goto :PARSE_ARGS
:ARGS_DONE

:: --- 3: Determine CMake Preset based on inputs ---
if "%COMPILER_MODE%"=="MSVC" (
    if "%BUILD_TYPE%"=="Debug" set "TARGET_PRESET=debug"
    if "%BUILD_TYPE%"=="Release" set "TARGET_PRESET=release"
    set "CONFIG_PRESET=msvc-x64"
)
if "%COMPILER_MODE%"=="GCC" (
    if "%BUILD_TYPE%"=="Debug" set "TARGET_PRESET=gcc-debug"
    if "%BUILD_TYPE%"=="Release" set "TARGET_PRESET=gcc-release"
    set "CONFIG_PRESET=gcc"
)
if "%COMPILER_MODE%"=="CLANG" (
    if "%BUILD_TYPE%"=="Debug" set "TARGET_PRESET=clang-debug"
    if "%BUILD_TYPE%"=="Release" set "TARGET_PRESET=clang-release"
    set "CONFIG_PRESET=clang"
)

echo [Process] Mode: %COMPILER_MODE% ^| Config: %BUILD_TYPE% ^| Preset: %TARGET_PRESET%

set "CMAKE_EXE=cmake.exe"

:: --- Find CMake STRATEGY 1: Check Global PATH ---
where cmake >nul 2>&1
if %errorlevel% equ 0 (
    echo [Info] Found CMake in global PATH.
    goto :FOUND_CMAKE
)

:: --- Find CMake STRATEGY 2: Check Active Environment ---
if defined VSINSTALLDIR (
    echo [Info] Detected active Visual Studio Environment: "%VSINSTALLDIR%"
    set "VS_ROOT=%VSINSTALLDIR%"
    goto :CHECK_BUNDLED
)

:: --- Find CMake STRATEGY 3: Use vswhere ---
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo [Error] CMake not in PATH, VSINSTALLDIR missing, vswhere not found.
    exit /b 1
)

set "VS_ROOT="
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.CMake.Project -property installationPath`) do (
    set "VS_ROOT=%%i"
)

if "%VS_ROOT%"=="" (
    echo [Error] Could not find VS installation with CMake.
    exit /b 1
)

:CHECK_BUNDLED
if "%VS_ROOT:~-1%"=="\" set "VS_ROOT=%VS_ROOT:~0,-1%"
set "CMAKE_EXE=%VS_ROOT%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

if not exist "%CMAKE_EXE%" (
    echo [Error] Bundled CMake not found at: "%CMAKE_EXE%"
    exit /b 1
)
echo [Info] Using bundled CMake: "%CMAKE_EXE%"

:FOUND_CMAKE

:: --- Step 2: Configure ---
echo.
echo [1/2] Configuring Project (%COMPILER_MODE%)...
"%CMAKE_EXE%" --preset %CONFIG_PRESET%
if %errorlevel% neq 0 exit /b %errorlevel%

:: --- Step 3: Build ---
echo.
echo [2/2] Compiling...
"%CMAKE_EXE%" --build --preset %TARGET_PRESET%
if %errorlevel% neq 0 exit /b %errorlevel%

echo.
echo [Success]