# Development Log

## 2026-06-13 - v1.0 Headless Render Milestone

- Confirmed the NTC project does not expose a standard `VULKAN_SDK`; it uses Donut/NVRHI and bundled compiler/runtime dependencies instead.
- Added a v1 headless renderer so the first milestone can produce a visible image before the Vulkan swapchain is filled in.
- Implemented a compact text scene format, cube animation, CPU frustum culling, simple material shading, z-buffer rasterization, and BMP output.
- Added `--render`, `--output`, `--width`, and `--height` CLI options.

Validation command:

```powershell
scripts\build_msvc.bat
build\nmake-debug\src\vulkan_render.exe --profile v1 --render --scene assets\scenes\v1.scene --output out\v1.bmp --width 1280 --height 720 --frames 16
```

