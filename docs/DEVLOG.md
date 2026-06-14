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

## 2026-06-13 - v2 Preview Cache

- Identified the main preview bottleneck: the v2 software window was reloading Scene'72 JSON, `.b72` buffers, and PNG textures every frame.
- Added an in-process cache for static v2 Scene'72 geometry and environment textures.
- Kept v1 Scene'72 animation uncached so driver animation remains correct.

Validation command:

```powershell
scripts\build_msvc.bat
```

## 2026-06-13 - Vulkan GPU Preview Window

- Added a native Vulkan Win32 preview path for mesh scenes.
- Vendored Vulkan headers from the local NTC dependency tree and dynamically loads `vulkan-1.dll`, so the preview does not require a globally configured `VULKAN_SDK` import library.
- Added HLSL shaders compiled to SPIR-V with the local NTC `dxc.exe`.
- Uploads imported Scene'72/glTF geometry to a Vulkan vertex buffer and draws it through a swapchain graphics pipeline with depth testing and a uniform-buffer camera.
- Reused the roaming camera controls from the CPU preview.
- Left the CPU software preview only as a fallback for the legacy `.scene` cube smoke scene.

Validation commands:

```powershell
scripts\build_msvc.bat
build\nmake-debug\src\vulkan_render.exe --profile v2 --preview --scene assets\third_party\s72_examples\materials.s72 --width 1280 --height 720
```

## 2026-06-13 - Vulkan Preview Startup Hardening

- Moved the Win32 preview timer startup until after the Vulkan renderer is fully initialized.
- Added draw-loop exception handling so recoverable Vulkan preview errors report to stderr instead of crashing silently.
- Resolved relative scene and shader paths by walking upward from the executable directory, so commands can be launched outside the repository root.
- Reworked Vulkan GPU startup to match the proven NTC/Donut order more closely: create the window surface before adapter selection, require graphics+present support on the chosen queue family, enable `VK_KHR_swapchain`, load device-level function pointers after `vkCreateDevice`, and only then fetch graphics/present queues.
- Fixed the immediate access violation: `vkGetDeviceQueue` was being called before its function pointer was loaded, which could jump through address `0x0` right after device creation.

Validation commands:

```powershell
scripts\build_msvc.bat
C:\Users\lzju\Desktop\VulkanRender\build\nmake-debug\src\vulkan_render.exe --profile v2 --preview --scene assets\third_party\s72_examples\materials.s72 --width 1280 --height 720
```

## 2026-06-14 - V2 MSAA And Texture Filtering

- Added GPU MSAA to the Vulkan preview path.
  - The renderer selects 4x MSAA when the device supports it for both color and depth attachments, falling back to 2x or 1x.
  - Rendering now targets a multisampled color attachment and resolves into the swapchain image before presentation.
  - Depth attachments and both mesh/sky graphics pipelines use the selected sample count.
- Added real texture mipmap generation during GPU texture upload.
  - Textures are uploaded with a full mip chain when linear blitting is supported.
  - The sampler now exposes the generated mip range instead of clamping to LOD 0.
- Enabled anisotropic texture filtering when supported by the selected Vulkan device.

Validation commands:

```powershell
scripts\build_msvc.bat
C:\Users\lzju\Desktop\VulkanRender\build\nmake-debug\src\vulkan_render.exe --profile v2 --preview --scene assets\third_party\s72_examples\materials.s72 --width 1280 --height 720
```

## 2026-06-14 - V2 Image-Based Lighting Preview

- Added official Scene'72 Ox Bridge Morning IBL textures to the Vulkan v2 preview path:
  - `ox_bridge_morning.png` for the visible background environment.
  - `ox_bridge_morning.lambertian.png` for diffuse irradiance.
  - `ox_bridge_morning.ggx-1.png` through `ox_bridge_morning.ggx-5.png` for roughness-dependent specular radiance.
- Expanded the Vulkan descriptor layout from material textures only to material textures plus IBL environment textures.
- Updated the PBR fragment shader to use diffuse irradiance, roughness-blended prefiltered specular radiance, and an environment BRDF approximation.
- Updated the sky pass to sample the full-resolution background environment rather than a prefiltered GGX map.
- Fixed Scene'72 cube/RGBE environment sampling in the Vulkan preview shaders; the Ox Bridge images are vertical cubemap strips, not lat-long panoramas.
- Reworked the environment path to match Renderer72 more closely:
  - Scene'72 RGBE cube strips are unpacked on the CPU to `R32G32B32A32_SFLOAT`.
  - Background, diffuse irradiance, and roughness-prefiltered specular maps are uploaded as `VK_IMAGE_VIEW_TYPE_CUBE`.
  - Sky and PBR shaders now sample `TextureCube` resources instead of manually sampling a 2D strip.
  - The PBR shader now samples a generated split-sum environment BRDF LUT.
  - Displacement mapping now uses parallax occlusion mapping with iterative layer stepping and UV discard.
- Added a Renderer72-style material submission path for the Vulkan preview:
  - GPU preview geometry now carries material texture sets and draw batches.
  - The Vulkan path creates one descriptor set per material texture set.
  - Mesh rendering binds the material descriptor set per batch before issuing `vkCmdDraw`.
  - Shared environment cubemaps and BRDF LUT remain common across material sets.
- Stabilized POM texture sampling while roaming the camera:
  - Displacement height lookup now uses an explicit mip level derived from the original UV derivatives.
  - POM fades out smoothly as the texture footprint grows at distance.
  - Albedo, normal, and roughness sampling use stable gradients after parallax UV adjustment.

Validation commands:

```powershell
scripts\build_msvc.bat
C:\Users\lzju\Desktop\VulkanRender\build\nmake-debug\src\vulkan_render.exe --profile v2 --preview --scene assets\third_party\s72_examples\materials.s72 --width 1280 --height 720
```

## 2026-06-14 - V2 Smooth Vertex Normals

- Fixed visible faceted triangle shading in the Vulkan v2 preview.
- The Scene'72 loader already read per-vertex normals, but the GPU preview collapsed them into a single averaged triangle normal.
- Added per-corner normals to the internal triangle representation and upload them as per-vertex Vulkan attributes.
- Added glTF normal import for static model previews, falling back to face normals when a model does not provide normals.

Validation commands:

```powershell
scripts\build_msvc.bat
C:\Users\lzju\Desktop\VulkanRender\build\nmake-debug\src\vulkan_render.exe --profile v2 --preview --scene assets\third_party\s72_examples\materials.s72 --width 1280 --height 720
```

## 2026-06-14 - V2 Renderer72-Aligned PBR Demo Assets

- Downloaded and organized a CC0 ambientCG material package for v2 feature validation:
  - Source: `https://ambientcg.com/a/Rock064`
  - Archive: `assets/third_party/ambientcg/downloads/Rock064_1K-JPG.zip`
  - Extracted files: `assets/third_party/ambientcg/Rock064_1K-JPG/`
- Added source/license notes in `assets/third_party/ambientcg/README.md`.
- Extended v2 GPU preview texture bindings from one albedo texture to a PBR texture set:
  - albedo/color
  - normal
  - roughness
  - displacement
- Updated the v2 shader to use the dedicated normal map for tangent-space normal mapping, roughness map for specular response, and displacement map for parallax-style UV offset.
- The v2 preview now uses the ambientCG Rock064 PBR texture set when it is present, falling back to the official Scene'72 wood textures otherwise.

Validation commands:

```powershell
scripts\build_msvc.bat
C:\Users\lzju\Desktop\VulkanRender\build\nmake-debug\src\vulkan_render.exe --profile v2 --preview --scene assets\third_party\s72_examples\materials.s72 --width 1280 --height 720
```

## 2026-06-14 - V2 GPU Texture And Bump/Parallax Preview

- Added per-vertex UVs to the GPU preview geometry instead of using one averaged UV per triangle.
- Added a Vulkan sampled image + sampler descriptor path for the official `wood_floor_deck_diff_1k.png` albedo texture.
- Uploads the albedo texture through a staging buffer into an optimal tiled GPU image.
- Updated the v2 mesh fragment shader to sample albedo in the GPU fragment stage.
- Added shader-side bump mapping and parallax-style UV displacement using albedo luminance as a height source for the current official material scene.
- This moves v2 closer to the Renderer72 v2 feature set: sky/tone mapping, PBR material parameters, texture sampling, normal-map-style perturbation, and displacement-style parallax are now represented in the GPU path.
- Current limits: the official local `materials.s72` bundle does not include dedicated normal or height maps, so this uses albedo-derived height until proper normal/displacement demo assets are added.

Validation commands:

```powershell
scripts\build_msvc.bat
C:\Users\lzju\Desktop\VulkanRender\build\nmake-debug\src\vulkan_render.exe --profile v2 --preview --scene assets\third_party\s72_examples\materials.s72 --width 1280 --height 720
```

## 2026-06-14 - V2 GPU Sky And Material Parameters

- Added roughness, metalness, and material kind to the Vulkan preview vertex layout.
- Updated the v2 mesh shader to branch between simple, environment, mirror, lambertian, and PBR-style preview shading.
- Added roughness-controlled specular response and metalness-controlled diffuse/specular mixing for the official `materials.s72` material spheres.
- Added a dedicated Vulkan sky pipeline that renders a procedural environment before the mesh pass.
- Added tone mapping in the sky and mesh fragment shaders.
- Current limits: this is still a preview PBR path; true Vulkan texture/sampler descriptors, normal maps, displacement mapping, and prefiltered cubemap IBL remain follow-up work.

Validation commands:

```powershell
scripts\build_msvc.bat
C:\Users\lzju\Desktop\VulkanRender\build\nmake-debug\src\vulkan_render.exe --profile v2 --preview --scene assets\third_party\s72_examples\materials.s72 --width 1280 --height 720
```

## 2026-06-13 - V1 Vulkan Preview Animation Upload

- Fixed v1 Scene'72 animation previews after the preview path moved to Vulkan.
- The issue was that the Vulkan preview built its vertex buffer only once at window creation; animated `.s72` scenes changed through `frameIndex`, but the updated geometry was never uploaded to the GPU.
- Added per-frame geometry rebuild/upload for v1 `.s72` previews while keeping v2 material previews cached/static.

Validation commands:

```powershell
scripts\build_msvc.bat
C:\Users\lzju\Desktop\VulkanRender\build\nmake-debug\src\vulkan_render.exe --profile v1 --preview --scene assets\third_party\s72_examples\sg-Articulation.s72 --width 1280 --height 720
C:\Users\lzju\Desktop\VulkanRender\build\nmake-debug\src\vulkan_render.exe --profile v1 --render --scene assets\third_party\s72_examples\sg-Articulation.s72 --frames 1 --output out\articulation-frame-001-check.bmp --width 640 --height 360
C:\Users\lzju\Desktop\VulkanRender\build\nmake-debug\src\vulkan_render.exe --profile v1 --render --scene assets\third_party\s72_examples\sg-Articulation.s72 --frames 24 --output out\articulation-frame-024-check.bmp --width 640 --height 360
```

## 2026-06-13 - V2 GPU Material Preview Shading

- Checked the Renderer72 README v2.0 demos: the reference splits material work into skybox/tone mapping, normal mapping, displacement mapping, PBR+IBL, and PBR material spheres.
- Extended the Vulkan preview vertex layout with world-space normals.
- Updated the Vulkan preview shaders from flat color to a simple lit material preview with direct light, specular highlight, and rim term.
- Kept the renderer on the official Scene'72 `materials.s72` demo for v2 material validation; full Vulkan texture descriptors, normal maps, displacement, and IBL remain planned follow-up work.

Validation commands:

```powershell
scripts\build_msvc.bat
C:\Users\lzju\Desktop\VulkanRender\build\nmake-debug\src\vulkan_render.exe --profile v2 --preview --scene assets\third_party\s72_examples\materials.s72 --width 1280 --height 720
```
