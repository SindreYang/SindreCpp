@echo off
setlocal EnableExtensions EnableDelayedExpansion

rem Quick Release configure, build, and test for Windows.
rem Set OPENBLAS_ROOT when the official OpenBLAS package is not already in
rem CMAKE_PREFIX_PATH, for example:
rem   set OPENBLAS_ROOT=C:\sdk\OpenBLAS
rem   scripts\build.bat

cd /d "%~dp0.."
if not defined BUILD_DIR set "BUILD_DIR=build"

rem Prefer Ninja when it is available.  Some Windows developer terminals do
rem not inherit the user's Ninja installation, so fall back to the MSVC
rem NMake generator instead of failing at CMake configure time.
set "SINDRE_GENERATOR="
where ninja >nul 2>&1
if not errorlevel 1 set "SINDRE_GENERATOR=Ninja"

if not defined SINDRE_GENERATOR (
    where cl >nul 2>&1
    if errorlevel 1 (
        set "SINDRE_VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
        if not exist "!SINDRE_VSWHERE!" set "SINDRE_VSWHERE=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"
        if exist "!SINDRE_VSWHERE!" (
            for /f "usebackq delims=" %%I in (`"!SINDRE_VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
                call "%%I\Common7\Tools\VsDevCmd.bat" -arch=x64 >nul
            )
        )
    )
    where cl >nul 2>&1
    if errorlevel 1 (
        echo [sindre] Ninja was not found and no MSVC developer environment is available.
        echo [sindre] Install Ninja or run this script from a Visual Studio developer prompt.
        exit /b 1
    )
    set "SINDRE_GENERATOR=NMake Makefiles"
)

if exist "%BUILD_DIR%\CMakeCache.txt" (
    for /f "tokens=2 delims==" %%G in ('findstr /b "CMAKE_GENERATOR:INTERNAL=" "%BUILD_DIR%\CMakeCache.txt"') do set "SINDRE_CACHED_GENERATOR=%%G"
    if defined SINDRE_CACHED_GENERATOR if /i not "!SINDRE_CACHED_GENERATOR!"=="%SINDRE_GENERATOR%" (
        echo [sindre] Build directory "%BUILD_DIR%" uses "!SINDRE_CACHED_GENERATOR!".
        echo [sindre] The current environment selects "%SINDRE_GENERATOR%".
        echo [sindre] Set BUILD_DIR to a new directory or reconfigure it explicitly.
        exit /b 2
    )
)

echo [sindre] Using generator: %SINDRE_GENERATOR%

if defined OPENBLAS_ROOT (
    cmake -S . -B "%BUILD_DIR%" -G "%SINDRE_GENERATOR%" -DCMAKE_BUILD_TYPE=Release ^
        -DSINDRE_BUILD_TESTS=ON -DSINDRE_BUILD_BENCHMARKS=ON ^
        -DSINDRE_BUILD_EXAMPLES=OFF -DCMAKE_PREFIX_PATH="%OPENBLAS_ROOT%" %*
) else (
    cmake -S . -B "%BUILD_DIR%" -G "%SINDRE_GENERATOR%" -DCMAKE_BUILD_TYPE=Release ^
        -DSINDRE_BUILD_TESTS=ON -DSINDRE_BUILD_BENCHMARKS=ON ^
        -DSINDRE_BUILD_EXAMPLES=OFF %*
)
if errorlevel 1 exit /b %errorlevel%

if defined CMAKE_BUILD_PARALLEL_LEVEL (
    cmake --build "%BUILD_DIR%" --parallel %CMAKE_BUILD_PARALLEL_LEVEL%
) else (
    cmake --build "%BUILD_DIR%" --parallel
)
if errorlevel 1 exit /b %errorlevel%

cmake --build "%BUILD_DIR%" --target test
exit /b %errorlevel%
