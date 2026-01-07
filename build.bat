@echo off
setlocal

:: --- 0: Save Script Location ---
set "SCRIPT_ROOT=%~dp0"

:: --- 1: Defaults ---
set "BUILD_TYPE=Debug"
set "COMPILER_MODE=GCC"
set "TARGET_PRESET=gcc-debug"
set "CMAKE_EXTRA_ARGS=-DBUILD_HEADERS_ONLY=OFF"
set "RUN_TESTS=OFF"
set "CLEAN_MODE=OFF"

:: --- 2: Argument Parsing Loop ---
:PARSE_ARGS
if "%~1"=="" goto :ARGS_DONE

:: -- Compilers
if /I "%~1"=="--gcc"   set "COMPILER_MODE=GCC" & shift & goto :PARSE_ARGS
if /I "%~1"=="--clang" set "COMPILER_MODE=CLANG" & shift & goto :PARSE_ARGS
if /I "%~1"=="--msvc"  set "COMPILER_MODE=MSVC" & shift & goto :PARSE_ARGS

:: -- Configurations
if /I "%~1"=="--release" set "BUILD_TYPE=Release" & shift & goto :PARSE_ARGS
if /I "%~1"=="--debug"   set "BUILD_TYPE=Debug" & shift & goto :PARSE_ARGS

:: -- Actions
if /I "%~1"=="--headers" set "CMAKE_EXTRA_ARGS=-DBUILD_HEADERS_ONLY=ON" & shift & goto :PARSE_ARGS
if /I "%~1"=="--test"    set "RUN_TESTS=ON" & shift & goto :PARSE_ARGS
if /I "%~1"=="--clean"      set "CLEAN_MODE=REBUILD" & shift & goto :PARSE_ARGS
if /I "%~1"=="--clean-only" set "CLEAN_MODE=ONLY" & shift & goto :PARSE_ARGS

shift
goto :PARSE_ARGS
:ARGS_DONE

set "CMAKE_EXTRA_ARGS=%CMAKE_EXTRA_ARGS% -DCMAKE_BUILD_TYPE=%BUILD_TYPE%"

:: --- 3: Determine CMake Preset ---
if "%COMPILER_MODE%"=="MSVC" (
    if "%BUILD_TYPE%"=="Debug" set "TARGET_PRESET=msvc-debug"
    if "%BUILD_TYPE%"=="Release" set "TARGET_PRESET=msvc-release"
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

:: --- Find CMake STRATEGY 1: Global PATH ---
where cmake >nul 2>&1
if %errorlevel% equ 0 (
    echo [Info] Found CMake in global PATH.
    goto :FOUND_CMAKE
)

:: --- Find CMake STRATEGY 2: Active Environment ---
if defined VSINSTALLDIR (
    echo [Info] Detected active Visual Studio Environment: "%VSINSTALLDIR%"
    set "VS_ROOT=%VSINSTALLDIR%"
    goto :CHECK_BUNDLED
)

:: --- Find CMake STRATEGY 3: Use vswhere (VS 2026) ---
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo [Error] vswhere not found.
    exit /b 1
)

set "VS_ROOT="
:: Target VS 2026 [18.0, 19.0)
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -version [18.0^,19.0^) -latest -products * -requires Microsoft.VisualStudio.Component.VC.CMake.Project -property installationPath`) do (
    set "VS_ROOT=%%i"
)

:: Fallback to VS 2022
if "%VS_ROOT%"=="" (
    echo [Info] VS 2026 not found, checking for VS 2022...
    for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -version [17.0^,18.0^) -latest -products * -requires Microsoft.VisualStudio.Component.VC.CMake.Project -property installationPath`) do (
        set "VS_ROOT=%%i"
    )
)

if "%VS_ROOT%"=="" (
    echo [Error] Could not find VS 2026 or VS 2022 installation with CMake.
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
echo [1/3] Configuring Project (%COMPILER_MODE%)...
"%CMAKE_EXE%" --preset %CONFIG_PRESET% %CMAKE_EXTRA_ARGS%
if %errorlevel% neq 0 exit /b %errorlevel%

:: --- Step 3: Clean (Optional) ---
if "%CLEAN_MODE%"=="OFF" goto :SKIP_CLEAN

echo.
echo [Clean] Cleaning build artifacts...
"%CMAKE_EXE%" --build --preset %TARGET_PRESET% --target clean
if %errorlevel% neq 0 exit /b %errorlevel%

if "%CLEAN_MODE%"=="ONLY" (
    echo [Info] Clean finished. Exiting.
    goto :SUCCESS
)
:SKIP_CLEAN

:: --- Step 4: Build ---
echo.
echo [2/3] Compiling...
"%CMAKE_EXE%" --build --preset %TARGET_PRESET%
if %errorlevel% neq 0 exit /b %errorlevel%

:: --- Step 5: Test ---
if "%RUN_TESTS%"=="ON" goto :RUN_TESTS
goto :SUCCESS

:RUN_TESTS
echo.
echo [Test] Running Integration Tests...
pushd "%SCRIPT_ROOT%"
if not exist "build\%CONFIG_PRESET%" (
    echo [Error] Build directory not found.
    popd & exit /b 1
)
cd "build\%CONFIG_PRESET%"
set "CTEST_EXE=ctest"
if not "%CMAKE_EXE%"=="cmake.exe" (
    for %%F in ("%CMAKE_EXE%") do set "CTEST_EXE=%%~dpFctest.exe"
)
"%CTEST_EXE%" -C %BUILD_TYPE% --output-on-failure
set "TEST_ERROR=%errorlevel%"
popd
if %TEST_ERROR% neq 0 exit /b %TEST_ERROR%

:SUCCESS
echo.
echo [Success]