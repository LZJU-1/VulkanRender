# Development Log

## 2026-06-16 - V5 Halton Jitter Reprojection

- Restored Halton subpixel jitter for the v5 G-buffer path so primary edges can accumulate antialiasing samples.
- Added previous-frame camera basis and previous jitter to `CameraUniform`.
- Added a surface-history ping-pong image storing previous normal and view depth.
- Updated the v5 denoise pass to reproject current G-buffer world positions into the previous frame before sampling:
  - final color history
  - shadow signal history
  - reflection signal history
- Added previous normal/depth validation after reprojection to reject disoccluded silhouettes.
- Upgraded final color resolve to padded 3x3 neighborhood clipping, edge-aware history weighting, residual edge smoothing, and lightweight adaptive sharpen.
- Added a luma-directional FXAA-style resolve for residual stair steps on high-contrast furniture silhouettes and thin bright geometry.
- Added 2x internal supersampling for v5: G-buffer, RT signals, denoise, and histories run at 2x resolution and a new compute downsample pass writes the swapchain.
- Moved imported local-light shadowed radiance into the shadow signal so it is denoised with the same temporal/bilateral path as directional visibility.
- Added `docs/V5_RT_DENOISING.md` to record the realtime RT denoising pipeline and remaining work.

Validation:

```powershell
scripts\compile_v5_shader_dxc.bat
tools\dxc\dxc_2026_05_27\bin\x64\dxc.exe -spirv "-fspv-target-env=vulkan1.2" -T vs_6_0 -E main shaders\vulkan_gpu\simple_color.vert.hlsl -Fo shaders\vulkan_gpu\simple_color.vert.spv
tools\dxc\dxc_2026_05_27\bin\x64\dxc.exe -spirv "-fspv-target-env=vulkan1.2" -T vs_6_0 -E main shaders\vulkan_gpu\v4_instanced_sphere.vert.hlsl -Fo shaders\vulkan_gpu\v4_instanced_sphere.vert.spv
scripts\build_msvc.bat nmake-debug
build\nmake-debug\src\vulkan_render.exe --profile v5-rt --preview --scene C:\Users\lzju\Desktop\MonteCarloPathTracer\scenes\bathroom2\bathroom2.xml --width 1280 --height 720
```

Expected markers:

```text
taa=halton16-surface-validated-resolve
denoise=split-shadow-reflection-temporal-bilateral-sharpen
internalScale=2x
record v5: dispatch ray signal compute
record v5: dispatch denoise compute
record v5: dispatch downsample compute
```

## 2026-06-16 - V5 Split RT Signal Denoising

- Split the v5 realtime ray tracing path into two compute passes:
  - `v5_raytrace.comp.hlsl` now writes raw shadow visibility and reflection radiance signal buffers.
  - `v5_denoise.comp.hlsl` applies temporal history clamping and normal/depth/material-guided bilateral filtering before final compose.
- Added raw shadow/reflection storage images and separate ping-pong histories for each signal.
- Kept the existing final-color temporal accumulation, now after signal-level denoising.
- Added startup and draw log markers for `denoise=split-shadow-reflection-temporal-bilateral`, `dispatch ray signal compute`, and `dispatch denoise compute`.
- Current limit: temporal stability is still camera-reset based because the G-buffer does not carry motion vectors yet.

Validation:

```powershell
scripts\compile_v5_shader_dxc.bat
scripts\build_msvc.bat nmake-debug
build\nmake-debug\src\vulkan_render.exe --profile v5-rt --preview --scene C:\Users\lzju\Desktop\MonteCarloPathTracer\scenes\bathroom2\bathroom2.xml --width 1280 --height 720
```

Observed log markers:

```text
runVulkanPreviewWindow: geometry vertices=3731769
createV5AccelerationStructures: triangles=1243923 tlas=ready
createTextureResources: ... denoise=split-shadow-reflection-temporal-bilateral
record v5: dispatch ray signal compute
record v5: dispatch denoise compute
draw: present
```

## 2026-06-15 - V5 Bathroom Emissive Lights And RT Reflection Step

- Imported PathTracer/Mitsuba XML light declarations such as `<light mtlname="Light" radiance="125.0,100.0,75.0"/>`.
- Added an `Emissive` material kind to the OBJ/MTL preview path.
- OBJ triangles using emissive materials are now:
  - drawn as visible bright light surfaces in the G-buffer
  - converted into bounded realtime preview lights for v5 shading
- Bound the existing light storage buffer into the v5 compute descriptor set at binding 20.
- Updated `v5_raytrace.comp.hlsl` so imported local lights contribute PBR light through TLAS ray-query visibility tests.
- Reworked glossy/mirror reflection to query the TLAS first, then sample visible G-buffer material when the hit projects back into the view.

Validation:

```powershell
scripts\compile_v5_shader_dxc.bat
scripts\build_msvc.bat
build\nmake-debug\src\vulkan_render.exe --profile v5-rt --preview --scene C:\Users\lzju\Desktop\MonteCarloPathTracer\scenes\bathroom2\bathroom2.xml --width 1280 --height 720
```

Observed log markers:

```text
runVulkanPreviewWindow: geometry vertices=3731769
createV5AccelerationStructures: triangles=1243923 tlas=ready
VulkanGpuRenderer: materialSets=4 batches=10 lights=2 sphereInstances=0 v5RayTracing=on
record v5: dispatch compute
draw: present
```

Current limitations:

- Reflection still uses a ray-query compute approximation, not a full closest-hit shader with triangle material lookup.
- Glass/transmission is still treated as opaque.
- `bathroom2.obj` remains slow to load because it is a large text mesh.

## 2026-06-15 - V5 PathTracer Bathroom Scene Import

- Added a lightweight OBJ/MTL importer for realtime preview scenes:
  - `v`, `vt`, `vn`
  - `f` polygon triangulation
  - positive and negative OBJ indices
  - `mtllib` / `usemtl`
  - `Kd`, `Ks`, `Ns`, and `map_Kd`
- Added companion XML camera support for PathTracer/Mitsuba-style scenes where `bathroom2.xml` sits beside `bathroom2.obj`.
- Added `.obj` and `.xml` as supported GPU preview scene extensions.
- Mapped OBJ materials approximately into the current PBR preview model:
  - `Kd` / `map_Kd` -> base color
  - `Ks` -> metalness
  - `Ns` -> roughness
- Validated `C:\Users\lzju\Desktop\MonteCarloPathTracer\scenes\bathroom2\bathroom2.xml` in v5 realtime RT preview.

Observed log markers:

```text
runVulkanPreviewWindow: geometry vertices=3731769
createV5AccelerationStructures: triangles=1243923 tlas=ready
VulkanGpuRenderer: materialSets=4 batches=10 lights=2 sphereInstances=0 v5RayTracing=on
record v5: dispatch compute
draw: present
```

Current limitations:

- The PathTracer area light/emissive material model is imported as bounded realtime preview lights, not full physically sampled area lights yet.
- Glass/transmission is treated as opaque material for now.
- The large text OBJ path is slow to load; a cached mesh or `.glb` conversion should be added for repeated tests.

## 2026-06-15 - V5 Hardware Ray-Query Shadows

- Corrected the v5 shadow path so it no longer uses shadow maps or screen-space G-buffer occlusion as the ray-traced shadow source.
- V5 now requires and enables the Vulkan hardware RT extension path:
  - `VK_KHR_acceleration_structure`
  - `VK_KHR_buffer_device_address`
  - `VK_KHR_deferred_host_operations`
  - `VK_KHR_ray_query`
  - `VK_KHR_spirv_1_4`
  - `VK_KHR_shader_float_controls`
- Added dynamic loading for acceleration-structure and buffer-device-address functions in the preview renderer.
- Added BLAS/TLAS construction from `GpuPreviewGeometry::vertices`; the validation materials scene builds 32009 triangles into a TLAS instance.
- Added a v5 acceleration-structure descriptor at binding 21.
- Updated `v5_raytrace.comp.hlsl` to bind `RaytracingAccelerationStructure sceneTlas` and use `RayQuery` shadow rays for directional-light visibility.
- Replaced the first hard single-ray shadow test with 12 stable cone-sampled ray-query rays per visible pixel for a softer directional-light penumbra.
- Added v5 ping-pong temporal history:
  - two `VK_FORMAT_R16G16B16A16_SFLOAT` history images
  - descriptor binding 22 for previous history input
  - descriptor binding 23 for current history storage output
  - command-buffer layout barriers around history read/write
  - camera-change reset through the v5 history frame counter
- Changed the RT shadow sample rotation to vary per frame and rely on temporal accumulation to reduce soft-shadow noise over a few still frames.
- Updated the DXC v5 shader compile script to target `cs_6_5` and emit SPIR-V ray-query extensions.
- Current limitation: v5 hardware RT shadows include the main triangle mesh; the v4 many-light instanced benchmark spheres are not yet added as TLAS instances.

Validation:

```powershell
scripts\compile_v5_shader_dxc.bat
scripts\build_msvc.bat
C:\Users\lzju\Desktop\VulkanRender\build\nmake-debug\src\vulkan_render.exe --profile v5-rt --preview --scene assets\third_party\s72_examples\materials.s72 --width 1280 --height 720
```

Observed log markers:

```text
selectDevice: NVIDIA GeForce RTX 2080 Ti
createV5AccelerationStructures: triangles=32009 tlas=ready
createV5HistoryResources
createV5RayTracingDescriptors
record v5: dispatch compute
draw: present
```

## 2026-06-15 - V5 Hybrid Realtime Ray Tracing Pass

- Reworked v5 from a standalone procedural compute ray demo into a hybrid realtime RT pipeline.
- V5 now creates the v4 G-buffer render pass/resources and records a first raster pass for real scene geometry.
- Added v5 compute descriptors for:
  - swapchain storage image output
  - material sampler
  - G-buffer albedo/roughness
  - G-buffer normal/metalness
  - G-buffer world position/material kind
- Replaced the procedural v5 shader with a G-buffer-driven compute shader for screen-space ray traced shadows, reflections, and ambient occlusion.
- Fixed the v5 preview crash caused by binding a null G-buffer graphics pipeline: the pipeline creation gate now includes `enableV5RayTracing_`.
- Kept the v5 command path explicit: G-buffer pass, image barrier to compute read, swapchain transition to storage, compute dispatch, then present transition.
- Retuned v5 shadow tracing from long screen-space directional rays to short contact shadows with distance fade. This avoids broken large shadow fragments from missing offscreen/backside G-buffer information.
- Disabled screen-space shadow/AO contribution in the main v5 lighting path after validation showed false occluders and black smear artifacts on the material-sphere scene. Stable v5 ray-traced shadows should be supplied by the upcoming BLAS/TLAS hardware RT path.
- Changed v5 G-buffer reads from filtered `SampleLevel` to `Load` point reads so world position and normal buffers are not linearly blended across geometry edges.
- Removed the temporary v3 shadow-atlas integration from v5 after review: shadow maps are a raster fallback and should not be presented as ray-traced shadows. The v5 shadow path should use KHR acceleration structures and hardware shadow rays.

Validation commands:

```powershell
scripts\build_msvc.bat
C:\Users\lzju\Desktop\VulkanRender\build\nmake-debug\src\vulkan_render.exe --profile v5-rt --preview --scene assets\third_party\s72_examples\materials.s72 --width 1280 --height 720
```

Validation log markers:

```text
createGBufferRenderPass
createV5RayTracingDescriptors
v5RayTracing=on
record v5: dispatch compute
draw: present
draw: done
```

## 2026-06-15 - V4 Instanced Sphere Benchmark Geometry

- Replaced the v4 many-light benchmark's CPU-expanded 10000-sphere vertex buffer with GPU instancing.
- Added `GpuPreviewSphereInstance` records to `GpuPreviewGeometry`.
- The many-light scene now keeps room/light marker geometry as regular mesh vertices, while the 10000 PBR spheres are submitted as 10000 instances.
- Added `v4_instanced_sphere.vert.hlsl` and a dedicated instanced G-buffer pipeline.
- The G-buffer pass now draws benchmark spheres with one `vkCmdDraw(..., instanceCount=10000)` call.
- Validation log changed from the old expanded path:

```text
geometry vertices=1190718
```

to the instanced path:

```text
geometry vertices=110718
materialSets=1 batches=1 lights=1024 sphereInstances=10000
draw/present
```

Validation commands:

```powershell
scripts\build_msvc.bat
C:\Users\lzju\Desktop\VulkanRender\build\nmake-debug\src\vulkan_render.exe --profile v4 --preview --width 1280 --height 720
```

## 2026-06-15 - V4 Renderer72-Scale Many-Light Alignment

- Upgraded the v4 many-light demo from a procedural shader loop to a Vulkan storage-buffer light path.
- `GpuPreviewGeometry` now carries `GpuPreviewLight` records.
- The Vulkan preview uploads the v4 lights into a `VK_DESCRIPTOR_TYPE_STORAGE_BUFFER` bound at `binding 20`.
- `v4_ssao_compose.frag.hlsl` now reads `StructuredBuffer<ManyLight>` instead of hard-coded light positions.
- Expanded the demo to Renderer72-scale counts:
  - 10000 PBR spheres
  - 1024 sphere/point lights
- Fixed the procedural sphere cap winding bug that caused the visible top cut-outs in the previous many-light scene.
- Added v4 debug views on number keys:
  - `0`: final composition
  - `1`: albedo
  - `2`: normal
  - `3`: depth
  - `4`: SSAO raw
  - `5`: SSAO blur

Validation commands:

```powershell
scripts\build_msvc.bat
C:\Users\lzju\Desktop\VulkanRender\build\nmake-debug\src\vulkan_render.exe --profile v4 --preview --width 1280 --height 720
```

Validation log markers:

```text
geometry vertices=1190718
materialSets=1 batches=1 lights=1024
draw/present
```

## 2026-06-15 - V4 Many-Light Demo Scene

- Added `assets/third_party/s72_examples/v4_many_lights.manylights` as a procedural v4 deferred demo marker.
- Added a many-light procedural geometry path:
  - 16x16 PBR sphere grid for roughness/metalness variation.
  - visible colored light markers above the sphere field.
  - room walls and a floor receiver for SSAO and light falloff.
- Changed the default v4 preview scene to the many-light marker.
- Added a v4 uniform flag so the deferred composition shader only enables the procedural many-light loop for the `.manylights` scene.
- Added 256 procedural point lights in `v4_ssao_compose.frag.hlsl` as the first many-light deferred demo path.
- Saved a validation screenshot at `out\v4_many_lights_window_final.png`.

Validation commands:

```powershell
scripts\build_msvc.bat
C:\Users\lzju\Desktop\VulkanRender\build\nmake-debug\src\vulkan_render.exe --profile v4 --preview --width 1280 --height 720
```

## 2026-06-15 - V4 Independent SSAO And Blur Passes

- Split v4 SSAO out of the final composition shader.
- Added an `R32_SFLOAT` raw SSAO target and an `R32_SFLOAT` blurred SSAO target.
- Added a dedicated fullscreen SSAO pass that reads G-buffer normal/world-position data.
- Added a dedicated fullscreen SSAO blur pass that reads raw AO and applies a light depth-aware 5x5 blur.
- Extended the v4 descriptor layout with raw AO and blurred AO sampled-image bindings.
- Updated final deferred composition to read blurred SSAO instead of recomputing AO inline.
- Captured a window-only preview screenshot at `out\v4_ssao_blur_window_blt.png` to confirm the Vulkan swapchain is presenting a visible scene.

Validation commands:

```powershell
scripts\build_msvc.bat
C:\Users\lzju\Desktop\VulkanRender\build\nmake-debug\src\vulkan_render.exe --profile v4 --preview --width 1280 --height 720
```

## 2026-06-15 - V4 Deferred G-buffer And SSAO Preview

- Enabled realtime preview for `--profile v4`.
- Added `enableV4Ssao` render setting and routed v4 through the GPU preview path.
- Added offscreen G-buffer resources:
  - albedo + roughness
  - normal + metalness
  - world position + material marker
- Added v4 shaders:
  - `v4_gbuffer.frag.hlsl`
  - `v4_fullscreen.vert.hlsl`
  - `v4_ssao_compose.frag.hlsl`
- Compiled the v4 shaders with `C:\Users\lzju\Desktop\NTC\bin\x64\dxc.exe`.
- Added fullscreen deferred composition with SSAO contact darkening.
- Fixed the fullscreen composition UV orientation so the v4 G-buffer image is not vertically flipped.
- Updated v4 docs and render commands.

Validation commands:

```powershell
scripts\build_msvc.bat
C:\Users\lzju\Desktop\VulkanRender\build\nmake-debug\src\vulkan_render.exe --profile v4 --preview --width 1280 --height 720
```

## 2026-06-15 - GPU Preview Mouse Look

- Added right-mouse drag look to the Vulkan GPU preview roaming camera.
- Kept the existing `R` toggle, `WASD`, `Q/E`, arrow-key, and `IJKL` controls.
- Updated render docs so v2/v3 realtime preview controls include mouse look before starting v4 work.

Validation commands:

```powershell
scripts\build_msvc.bat
C:\Users\lzju\Desktop\VulkanRender\build\nmake-debug\src\vulkan_render.exe --profile v3 --preview --width 1280 --height 720
```

## 2026-06-15 - V3 Dedicated Shadow Demo Scene

- Added `assets/third_party/s72_examples/v3_shadow_demo.shadowdemo` as the default v3 realtime preview scene.
- The `.shadowdemo` marker triggers a procedural scene with a large shadow receiver, tall blockers, wall geometry, steps, and an overhead beam.
- Removed coplanar bottom faces from ground-contact demo boxes so they do not z-fight with the floor while roaming the camera.
- Retuned the default v3 demo lights so the directional light is the primary visible shadow source, while spot and point lights remain present but less visually intrusive.
- Changed v3 preview defaults so `--profile v3 --preview` opens the shadow-focused scene instead of the v2 material sphere scene.
- Kept `materials.s72` available for v2 material/IBL regression, but no longer use it as the primary shadow-map demo because its geometry does not show strong projection relationships.
- Updated the v3 feature guide and render commands around the new shadow validation scene.

Validation commands:

```powershell
scripts\build_msvc.bat
C:\Users\lzju\Desktop\VulkanRender\build\nmake-debug\src\vulkan_render.exe --profile v3 --preview --width 1280 --height 720
```

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

## 2026-06-14 - V3 Directional Shadow And Local Lights

- Enabled `--profile v3` for the Vulkan GPU preview path, reusing the official Scene'72 `materials.s72` material scene.
- Added a v3 settings flag so v3 can keep the v2 PBR/IBL material path while enabling shadow/light shader work.
- Added a 2048x2048 directional shadow map:
  - depth-only Vulkan render pass
  - depth texture sampled by the main material shader
  - `shadow_depth.vert.hlsl` shadow projection shader
  - 3x3 PCF filtering and slope-style bias in the material shader
- Added point/sphere-style local light contribution and spot light contribution to the v3 material shader.
- Added `docs/V3_FEATURES.md` with implementation notes, asset requirements, demo command, and current v3 status.

Validation commands:

```powershell
scripts\build_msvc.bat
C:\Users\lzju\Desktop\VulkanRender\build\nmake-debug\src\vulkan_render.exe --profile v3 --preview --scene assets\third_party\s72_examples\materials.s72 --width 1280 --height 720
```

## 2026-06-14 - V3 Shadow Atlas Alignment

- Replaced the single directional shadow map with a unified v3 shadow atlas:
  - 4x3 atlas layout
  - 1024x1024 per shadow tile
  - 3 directional cascade tiles
  - 1 spot light shadow tile
  - 6 point/sphere omni shadow face tiles
- Updated the shadow pass to render each tile with its own viewport/scissor while sharing the same depth-only render pass and pipeline.
- Updated `shadow_depth.vert.hlsl` to select directional cascade, spot, or point face projection through `SV_InstanceID`.
- Updated the v3 material shader to sample the shadow atlas for directional cascades, spot shadows, and point/sphere omni shadows.
- Updated `docs/V3_FEATURES.md`, `docs/RENDER_ME.md`, and `docs/ROADMAP.md` to reflect v3 shadow-map feature alignment.

Validation commands:

```powershell
scripts\build_msvc.bat
C:\Users\lzju\Desktop\VulkanRender\build\nmake-debug\src\vulkan_render.exe --profile v3 --preview --scene assets\third_party\s72_examples\materials.s72 --width 1280 --height 720
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

## 2026-06-14 - V2 Stable Mesh Tangents

- Added mesh-provided tangent support to the Scene'72 loader and Vulkan preview vertex layout.
- Added glTF/GLB tangent attribute support with a UV-based fallback tangent for meshes that do not provide one.
- Updated the v2 material shader to build its TBN basis from interpolated mesh tangents instead of screen-space derivatives.
- This fixes the textured rock material shimmer/pop where normal/displacement detail appeared to spread and regroup during camera distance changes.

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

## 2026-06-15 - V5 Realtime Compute Ray Tracing Preview

- Added the first runnable v5 realtime ray tracing preview path.
- `--profile v5-rt --preview` now opens the Vulkan preview instead of stopping at graph planning.
- Added `V1RenderSettings::enableV5RayTracing` to select the v5 path.
- Added a Vulkan compute pipeline that dispatches `shaders/vulkan_gpu/v5_raytrace.comp.spv`.
- Added `shaders/vulkan_gpu/v5_raytrace.comp.hlsl`, a procedural GPU ray tracer with:
  - primary camera rays
  - sphere and plane intersections
  - hard shadow rays
  - simple reflection/sky contribution
  - tone mapping
- V5 writes directly to the acquired swapchain image as a storage image:
  - swapchain requests `VK_IMAGE_USAGE_STORAGE_BIT`
  - descriptor binding 0 is the camera uniform buffer
  - descriptor binding 1 is the storage output image
  - command buffer transitions `UNDEFINED -> GENERAL -> PRESENT_SRC_KHR`
- Added `docs/V5_FEATURES.md` and updated `docs/RENDER_ME.md`.
- Downloaded DXC locally during development to compile the v5 shader, but `tools/` is ignored and is not intended to be committed.
- Current limit: this is a GPU compute ray tracing path, not yet a KHR ray tracing pipeline with BLAS/TLAS/SBT.

Validation commands:

```powershell
scripts\build_msvc.bat
build\nmake-debug\src\vulkan_render.exe --profile v5-rt --preview --width 1280 --height 720
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
# 2026-06-16 - V5 Realtime Hybrid RT Direction

- Saved the previous v5 denoising/supersampling work as commit `1eb2f47`.
- Re-centered v5 on realtime hybrid RT for bathroom2 validation:
  - default internal scale is now 1x for interactive performance;
  - 2x remains a quality/debug target instead of the main path;
  - render graph/docs now describe the actual raster G-buffer + ray-query signal + denoise path;
  - camera movement reduces effective history confidence so the image re-accumulates instead of trusting a stale long history;
  - final resolve now adapts to 1x or optional larger internal scales.
- Reduced per-pixel directional RT shadow rays so v5 behaves more like a realtime noisy-signal pipeline that relies on temporal/bilateral denoising.

# 2026-06-16 - V5 Moving-Camera TAA Fixes

- Used `.reference/VulkanHybridRenderer` as the local reference for hybrid G-buffer + SVGF-style reprojection.
- Fixed a moving-camera issue where Halton jitter was tied to the history confidence counter and could freeze while the camera was moving.
- Upgraded v5 history sampling:
  - previous final color, shadow, and reflection histories now use validated 2x2 bilinear taps;
  - previous surface normal/depth history validates each tap;
  - final color uses a 3x3 validated fallback search when bilinear reprojection misses.
- Camera motion now caps history confidence at a short accumulation window instead of forcing it down to two frames.
