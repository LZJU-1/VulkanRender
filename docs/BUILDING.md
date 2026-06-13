# Building And Testing

## Prerequisites

- CMake 3.24 or newer
- A C++20 compiler
- Optional: Vulkan SDK with `glslc`

On Windows, install the Vulkan SDK and set `VULKAN_SDK` if CMake cannot find it automatically.

## Commands

```powershell
cmake --preset msvc-debug
cmake --build --preset msvc-debug
ctest --preset msvc-debug
```

List GPU capability:

```powershell
.\build\msvc-debug\src\Debug\vulkan_render.exe --list-devices
```

Run the realtime ray tracing profile:

```powershell
.\build\msvc-debug\src\Debug\vulkan_render.exe --profile v5-rt --enable-rt --frames 1
```

If the GPU or driver does not expose the required extensions, the app reports the missing support and keeps the graph in fallback mode unless `--require-rt` is passed.

