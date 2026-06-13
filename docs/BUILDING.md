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

Render the current v1 image:

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v1 --render --scene assets\scenes\v1.scene --output out\v1.bmp --width 1280 --height 720 --frames 16
```

Open the v1 realtime preview window:

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v1 --preview --scene assets\scenes\v1.scene --width 960 --height 540
```

Render the official Scene'72 examples:

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v1 --render --scene assets\third_party\s72_examples\rotation.s72 --output out\official-rotation.bmp --width 960 --height 960
build\nmake-debug\src\vulkan_render.exe --profile v1 --render --scene assets\third_party\s72_examples\sg-Articulation.s72 --output out\official-articulation.bmp --width 1280 --height 720
```

Render the official v2 materials example:

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v2 --render --scene assets\third_party\s72_examples\materials.s72 --output out\v2-materials.bmp --width 1280 --height 720
```

Open an official Scene'72 example in the realtime preview window:

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v1 --preview --scene assets\third_party\s72_examples\rotation.s72 --width 720 --height 720
build\nmake-debug\src\vulkan_render.exe --profile v2 --preview --scene assets\third_party\s72_examples\materials.s72 --width 1280 --height 720
```

These mesh preview commands use the native Vulkan GPU window. The CPU preview remains only as a fallback for the old `assets\scenes\v1.scene` cube smoke scene.

Render a glTF or GLB model:

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v1 --render --scene path\to\model.glb --output out\model.bmp --width 1280 --height 720
build\nmake-debug\src\vulkan_render.exe --profile v1 --preview --scene path\to\model.gltf --width 1280 --height 720
```
