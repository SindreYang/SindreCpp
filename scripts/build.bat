@echo off
setlocal

rem 统一的 Windows ClangCL + Visual Studio RelWithDebInfo 构建入口；
rem 所有产物位于 build_win\bin。
set "ROOT=%~dp0.."
pushd "%ROOT%"
cmake --preset windows-clang-cl %*
if errorlevel 1 exit /b %errorlevel%
cmake --build --preset windows-clang-cl
if errorlevel 1 exit /b %errorlevel%
ctest --preset windows-clang-cl
set "STATUS=%errorlevel%"
popd
exit /b %STATUS%
