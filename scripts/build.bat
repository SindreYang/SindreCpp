@echo off
setlocal EnableExtensions EnableDelayedExpansion

rem Initialize the Visual Studio toolchain and build with Ninja + ClangCL.
rem Do not depend on a specific Visual Studio version; outputs go to build_win\bin.
set "ROOT=%~dp0.."
pushd "%ROOT%"

rem Use an explicitly provided VS path, or find the latest complete instance with vswhere.
if defined SINDRE_VS_INSTALLATION_PATH (
    set "VS_INSTALLATION_PATH=%SINDRE_VS_INSTALLATION_PATH%"
) else (
    set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
    if not exist "!VSWHERE!" set "VSWHERE=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"
    if not exist "!VSWHERE!" (
        echo [sindre] Error: vswhere.exe was not found.>&2
        echo [sindre] Install Visual Studio with the Desktop C++ workload.>&2
        set "STATUS=2"
        goto :finish
    )
    for /f "usebackq tokens=* delims=" %%I in (`"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VS_INSTALLATION_PATH=%%I"
)

if not defined VS_INSTALLATION_PATH (
    echo [sindre] Error: no complete Visual Studio C++ installation was found.>&2
    echo [sindre] Required component: Microsoft.VisualStudio.Component.VC.Tools.x86.x64.>&2
    set "STATUS=2"
    goto :finish
)

set "VSDEV_CMD=!VS_INSTALLATION_PATH!\Common7\Tools\VsDevCmd.bat"
if not exist "!VSDEV_CMD!" (
    echo [sindre] Error: VsDevCmd.bat was not found under !VS_INSTALLATION_PATH!.>&2
    set "STATUS=2"
    goto :finish
)
call "!VSDEV_CMD!" -arch=x64
if errorlevel 1 (
    echo [sindre] Error: failed to initialize the Visual Studio x64 environment.>&2
    set "STATUS=2"
    goto :finish
)

for %%T in (cmake ninja clang-cl) do (
    where.exe %%T >nul 2>&1
    if errorlevel 1 (
        echo [sindre] Error: %%T is not available after VS environment initialization.>&2
        if "%%T"=="clang-cl" echo [sindre] Install the Visual Studio C++ Clang tools.>&2
        if "%%T"=="ninja" echo [sindre] Install the Visual Studio CMake/Ninja component.>&2
        set "STATUS=2"
        goto :finish
    )
)

cmake --preset windows-clang-cl %*
if errorlevel 1 (
    set "STATUS=%errorlevel%"
    goto :finish
)
cmake --build --preset windows-clang-cl --parallel
if errorlevel 1 (
    set "STATUS=%errorlevel%"
    goto :finish
)
ctest --preset windows-clang-cl
set "STATUS=%errorlevel%"

:finish
popd
exit /b %STATUS%
