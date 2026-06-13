@echo off
setlocal

set "VS_VCVARS=%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VS_VCVARS%" set "VS_VCVARS=D:\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

if not exist "%VS_VCVARS%" (
    echo Could not find vcvars64.bat. Open a Developer Command Prompt or update scripts\smoke_msvc.bat.
    exit /b 1
)

call "%VS_VCVARS%"
if not exist build mkdir build

cl /nologo /std:c++20 /EHsc /W4 /permissive- /DVR_HAS_VULKAN=0 /I src ^
    tests\profile_tests.cpp src\core\FeatureProfile.cpp src\core\RenderGraph.cpp ^
    /Fe:build\profile_tests.exe
if errorlevel 1 exit /b 1

build\profile_tests.exe
if errorlevel 1 exit /b 1

cl /nologo /std:c++20 /EHsc /W4 /permissive- /DVR_HAS_VULKAN=0 /I src ^
    src\main.cpp src\core\FeatureProfile.cpp src\core\RenderGraph.cpp src\core\RendererApp.cpp ^
    src\platform\CommandLine.cpp src\platform\PreviewWindow.cpp src\render\SoftwareV1Renderer.cpp ^
    src\rhi\RenderDevice.cpp src\rhi\VulkanDevice.cpp user32.lib gdi32.lib ^
    /Fe:build\vulkan_render_smoke.exe
if errorlevel 1 exit /b 1

build\vulkan_render_smoke.exe --profile v5-rt --enable-rt --frames 1
