@echo off
setlocal

rem 统一的 Windows Release 构建入口；所有产物位于 build\windows\bin。
set "ROOT=%~dp0.."
set "BUILD_DIR=%ROOT%\build\windows"

where ninja.exe >nul 2>nul
if errorlevel 1 (
    echo Ninja is required. Install Ninja and make it available on PATH.
    exit /b 2
)

if defined SINDRE_MATH_OPENBLAS_ROOT (
    cmake -S "%ROOT%" -B "%BUILD_DIR%" -G Ninja ^
        -DCMAKE_BUILD_TYPE=Release ^
        -DSINDRE_BUILD_TESTS=ON ^
        -DSINDRE_BUILD_EXAMPLES=ON ^
        "-DSINDRE_MATH_OPENBLAS_ROOT=%SINDRE_MATH_OPENBLAS_ROOT%" %*
) else (
    cmake -S "%ROOT%" -B "%BUILD_DIR%" -G Ninja ^
        -DCMAKE_BUILD_TYPE=Release ^
        -DSINDRE_BUILD_TESTS=ON ^
        -DSINDRE_BUILD_EXAMPLES=ON %*
)
if errorlevel 1 exit /b %errorlevel%

cmake --build "%BUILD_DIR%" --parallel
if errorlevel 1 exit /b %errorlevel%

ctest --test-dir "%BUILD_DIR%" --output-on-failure
exit /b %errorlevel%
