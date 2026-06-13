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

## 2026-06-13 - Official Scene'72 v1 Examples

- Vendored the official `15-472/s72` `s24` examples needed for v1 demos: `rotation.s72` and `sg-Articulation.s72` with their `.b72` buffers.
- Added a minimal Scene'72 v1 loader for official examples: JSON parsing, scene roots, node transforms, non-indexed `TRIANGLE_LIST` meshes, `POSITION` float3, and `COLOR` rgba8.
- Rendered official examples:
  - `out\official-rotation.bmp`
  - `out\official-articulation.bmp`
- Verified realtime preview with official `rotation.s72`.

Validation commands:

```powershell
scripts\build_msvc.bat
build\nmake-debug\src\vulkan_render.exe --profile v1 --render --scene assets\third_party\s72_examples\rotation.s72 --output out\official-rotation.bmp --width 960 --height 960
build\nmake-debug\src\vulkan_render.exe --profile v1 --render --scene assets\third_party\s72_examples\sg-Articulation.s72 --output out\official-articulation.bmp --width 1280 --height 720
build\nmake-debug\src\vulkan_render.exe --profile v1 --preview --scene assets\third_party\s72_examples\rotation.s72 --width 720 --height 720
```

## 2026-06-13 - Scene'72 Driver Animation

- Added Scene'72 `DRIVER` support for v1 examples.
- Implemented `translation`, `scale`, and `rotation` channels with `STEP`, `LINEAR`, and `SLERP` sampling.
- Verified `sg-Articulation.s72` changes pose across frames and loops back to the start.
- Verified realtime preview with the official articulated arm scene.

Validation commands:

```powershell
scripts\build_msvc.bat
build\nmake-debug\src\vulkan_render.exe --profile v1 --render --scene assets\third_party\s72_examples\sg-Articulation.s72 --output out\official-articulation-frame001.bmp --width 1280 --height 720 --frames 1
build\nmake-debug\src\vulkan_render.exe --profile v1 --render --scene assets\third_party\s72_examples\sg-Articulation.s72 --output out\official-articulation-frame025.bmp --width 1280 --height 720 --frames 25
build\nmake-debug\src\vulkan_render.exe --profile v1 --preview --scene assets\third_party\s72_examples\sg-Articulation.s72 --width 1280 --height 720
```

## 2026-06-13 - Static glTF/GLB Import

- Added `cgltf` 1.15 as a vendored MIT-licensed dependency.
- Added static `.gltf` and `.glb` import for v1 preview/render.
- Supported node transforms, mesh primitives, indexed and non-indexed triangle lists, `POSITION`, `COLOR_0`, and material base color fallback.
- Skinned meshes, animation channels, textures, and PBR shading remain future work.

Example commands:

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v1 --render --scene path\to\model.glb --output out\model.bmp --width 1280 --height 720
build\nmake-debug\src\vulkan_render.exe --profile v1 --preview --scene path\to\model.gltf --width 1280 --height 720
```

## 2026-06-13 - v2.0 PBR And IBL Software Validation

- Vendored the official Scene'72 `materials.s72` demo assets: material meshes, Ox Bridge Morning environment images, and Wood Floor Deck albedo/roughness textures.
- Added `stb_image` for PNG texture loading.
- Extended the Scene'72 loader with `MATERIAL`, `ENVIRONMENT`, `NORMAL`, `TANGENT`, and `TEXCOORD` support.
- Implemented a v2 software validation path behind `--profile v2`: textured lambertian material, mirror/environment material approximation, PBR-style roughness/metalness shading, environment-map skybox approximation, and tone mapping.
- Wired v2 render and preview commands through the existing software frame renderer while keeping v1 behavior intact.
- Documented current limits: this is not yet the final Vulkan PBR pipeline; normal/displacement map shader evaluation and true cubemap IBL prefiltering remain future work.

Validation commands:

```powershell
scripts\build_msvc.bat
build\nmake-debug\src\vulkan_render.exe --profile v2 --render --scene assets\third_party\s72_examples\materials.s72 --output out\v2-materials.bmp --width 1280 --height 720
build\nmake-debug\src\vulkan_render.exe --profile v1 --render --scene assets\third_party\s72_examples\sg-Articulation.s72 --output out\v1-regression-articulation.bmp --width 640 --height 360 --frames 25
```

## 2026-06-13 - v2 Camera Roaming And Skybox Cleanup

- Removed the stretched HDRI-style background lookup that made the v2 preview look striped.
- Kept the official Scene'72 environment image as an approximate lighting/reflection source, while using a cleaner procedural preview sky for the visible background.
- Added a roaming camera mode to the Windows preview window.
- Preview controls: `R` toggles roaming, `WASD` moves, `Q/E` or `Ctrl/Space` move vertically, arrows or `IJKL` look around, `Shift` moves faster, `Esc` closes the window.

Validation commands:

```powershell
scripts\build_msvc.bat
build\nmake-debug\src\vulkan_render.exe --profile v2 --render --scene assets\third_party\s72_examples\materials.s72 --output out\v2-materials-fixed.bmp --width 1280 --height 720
```
