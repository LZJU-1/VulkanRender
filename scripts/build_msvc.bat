@echo off
setlocal

set "VS_VCVARS=%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VS_VCVARS%" set "VS_VCVARS=D:\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

if not exist "%VS_VCVARS%" (
    echo Could not find vcvars64.bat. Open a Developer Command Prompt or update scripts\build_msvc.bat.
    exit /b 1
)

set "PRESET=%~1"
if "%PRESET%"=="" set "PRESET=nmake-debug"

call "%VS_VCVARS%"
cmake --preset %PRESET%
if errorlevel 1 exit /b 1
cmake --build --preset %PRESET%
if errorlevel 1 exit /b 1
ctest --preset %PRESET%
