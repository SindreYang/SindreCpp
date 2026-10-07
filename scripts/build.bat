@echo off
setlocal EnableExtensions EnableDelayedExpansion

rem Windows Release 配置、编译和测试快捷入口。
rem 如需使用内置 profile 之外的固定 OpenBLAS SDK，可设置：
rem   set SINDRE_MATH_OPENBLAS_ROOT=C:\sdk\OpenBLAS
rem   scripts\build.bat

cd /d "%~dp0.."
if not defined BUILD_DIR set "BUILD_DIR=build"

rem 优先使用 Ninja；如果当前 Visual Studio 环境没有继承 Ninja，则回退到 NMake。
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

if defined SINDRE_MATH_OPENBLAS_ROOT (
    cmake -S . -B "%BUILD_DIR%" -G "%SINDRE_GENERATOR%" -DCMAKE_BUILD_TYPE=Release ^
        -DSINDRE_BUILD_TESTS=ON -DSINDRE_BUILD_BENCHMARKS=ON ^
        -DSINDRE_BUILD_EXAMPLES=OFF -DSINDRE_MATH_OPENBLAS_ROOT="%SINDRE_MATH_OPENBLAS_ROOT%" %*
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
