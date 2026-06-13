# Development Log

## 2026-06-13 - v1.0 Headless Render Milestone

- Confirmed the NTC project does not expose a standard `VULKAN_SDK`; it uses Donut/NVRHI and bundled compiler/runtime dependencies instead.
- Added a v1 headless renderer so the first milestone can produce a visible image before the Vulkan swapchain is filled in.
- Implemented a compact text scene format, cube animation, CPU frustum culling, simple material shading, z-buffer rasterization, and BMP output.
- Added `--render`, `--output`, `--width`, and `--height` CLI options.
- Added `--preview` for a Windows realtime v1 preview window using the same software frame renderer.

Validation command:

```powershell
scripts\build_msvc.bat
build\nmake-debug\src\vulkan_render.exe --profile v1 --render --scene assets\scenes\v1.scene --output out\v1.bmp --width 1280 --height 720 --frames 16
```

## 2026-06-13 - v1.0 Realtime Preview Window

- Added a Windows realtime preview path using Win32/GDI and the same v1 software frame renderer.
- Verified the preview window launches with `--profile v1 --preview --scene assets\scenes\v1.scene --width 960 --height 540`.
- Checked `YJJfish/Renderer72`: the public repo contains source code and demo GIFs, but no committed `.s72`, `.b72`, glTF, OBJ, or other scene asset files.
- Checked the CMU A1/A2/A3 assignment pages: the course uses Scene'72 and asks students to create/report their own animation, model, and scene files; it does not appear to publish the exact Renderer72 demo scenes as downloadable assets.

Preview command:

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v1 --preview --scene assets\scenes\v1.scene --width 960 --height 540
```
