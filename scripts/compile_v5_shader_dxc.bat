@echo off
setlocal

set "DXC=%DXC_PATH%"
if "%DXC%"=="" set "DXC=%~dp0..\tools\dxc\dxc_2026_05_27\bin\x64\dxc.exe"

if not exist "%DXC%" (
    echo Could not find DXC. Set DXC_PATH or place dxc.exe under tools\dxc\dxc_2026_05_27\bin\x64.
    exit /b 1
)

"%DXC%" -spirv "-fspv-target-env=vulkan1.2" "-fspv-extension=SPV_KHR_ray_query" "-fspv-extension=SPV_KHR_ray_tracing" -T cs_6_5 -E main "%~dp0..\shaders\vulkan_gpu\v5_raytrace.comp.hlsl" -Fo "%~dp0..\shaders\vulkan_gpu\v5_raytrace.comp.spv"
if errorlevel 1 exit /b 1

echo Compiled shaders\vulkan_gpu\v5_raytrace.comp.spv
