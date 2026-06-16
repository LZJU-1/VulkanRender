# V5 Feature Guide

V5 is the realtime hybrid ray tracing profile. The current implementation follows a modern hybrid-game layout: a jittered raster G-buffer provides primary visibility, hardware Vulkan ray-query traces only selected shadow/reflection signals from a TLAS/BLAS scene, raw RT signal buffers are temporally reprojected and bilateral-filtered, then an edge-aware TAA resolve writes into the swapchain as a storage image.

## Feature: V5 Profile And Preview Entry

Implementation:

- `--profile v5-rt` and aliases such as `v5`, `rt`, `raytracing`, `hybrid-rt`, and `v5-hybrid` select the v5 profile.
- `RendererApp` now allows v5 in realtime preview mode.
- `V1RenderSettings::enableV5RayTracing` selects the Vulkan compute ray tracing path.
- The preview window still supports the existing roaming camera controls, so the ray traced camera can be moved in realtime.

Asset requirements:

- Any v4-style mesh scene works for primary visibility, because v5 reads material data through the rasterized G-buffer.
- Shadow visibility currently uses the main triangle mesh uploaded to `GpuPreviewGeometry::vertices`.
- The current preview uses the same materials scene as v2/v4 validation.
- PathTracer/Mitsuba-style `.xml` files are supported when a companion `.obj/.mtl` with the same stem exists beside the XML.
- Plain `.obj` scenes with `mtllib`, `usemtl`, `v/vt/vn`, and polygon faces are supported.
- Procedural instanced benchmark spheres are not yet included in the v5 TLAS.

Demo:

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v5-rt --preview --width 1280 --height 720
```

Expected log markers:

- `createV5RayTracingDescriptors`
- `createV5AccelerationStructures: triangles=... tlas=ready`
- `v5RayTracing=on`
- `taa=halton16-surface-validated-resolve`
- `denoise=hybrid-split-signal-temporal-bilateral`
- `mode=realtime-hybrid-rt`
- `internalScale=1x`
- `record v5: dispatch ray signal compute`
- `record v5: dispatch denoise compute`
- `record v5: dispatch downsample compute`
- repeated `draw/present`

PathTracer bathroom2 demo:

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v5-rt --preview --scene C:\Users\lzju\Desktop\MonteCarloPathTracer\scenes\bathroom2\bathroom2.xml --width 1280 --height 720
```

## Feature: GPU Compute Ray Generation

Implementation:

- Added `shaders/vulkan_gpu/v5_raytrace.comp.hlsl`.
- The shader is compiled to `shaders/vulkan_gpu/v5_raytrace.comp.spv` with DXC SPIR-V output.
- The ray signal compute shader uses one thread per pixel and dispatches in 8x8 workgroups.
- For every pixel, the shader reads G-buffer albedo, normal, and world-position data before tracing RT shadow/reflection signals.
- The shader binds `RaytracingAccelerationStructure sceneTlas` at binding 21 and uses `RayQuery` for shadow visibility.
- The shader writes raw directional shadow visibility to binding 24 and raw reflection radiance to binding 25.
- `v5_denoise.comp.hlsl` reads those raw signals, applies temporal history clamping and bilateral spatial filtering, then writes final color.

Scene features:

- Hardware ray-query directional shadows.
- Screen-space reflections for glossy/metallic surfaces.
- Separate raw shadow and reflection signal buffers.
- Signal-level temporal accumulation with history clamp.
- Normal/depth/material-guided bilateral filtering for shadows and reflections.
- PBR lighting against the rasterized scene.
- Tone mapping and gamma correction.

## Feature: Hardware Ray-Query Shadows

Implementation:

- V5 enables `VK_KHR_acceleration_structure`, `VK_KHR_buffer_device_address`, `VK_KHR_deferred_host_operations`, `VK_KHR_ray_query`, `VK_KHR_spirv_1_4`, and `VK_KHR_shader_float_controls`.
- The preview builds one BLAS from the scene triangle vertex buffer and one TLAS instance referencing it.
- The v5 descriptor set adds binding 21 as `VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR`.
- `v5_raytrace.comp.hlsl` casts 12 cone-sampled `RayQuery` shadow rays from each visible G-buffer surface point toward the directional light.
- The cone sample rotation changes per frame and is stabilized by temporal history accumulation.
- Shadow maps are not used in v5. The shadow atlas remains a v3/v4 raster feature.

## Feature: Split RT Signal Denoising

Implementation:

- V5 uses a 16-sample Halton jitter sequence for subpixel coverage in the jittered G-buffer.
- V5 defaults to 1x internal resolution for realtime interaction. `kV5QualityInternalScale` remains in code as the comparison/quality target for later optional modes.
- The camera uniform stores both current and previous camera frames:
  - current camera basis and current jitter
  - previous camera basis and previous jitter
- V5 allocates two `VK_FORMAT_R16G16B16A16_SFLOAT` final-color history images.
- V5 also allocates raw shadow/reflection signal images and separate ping-pong history images for each signal.
- V5 stores a surface-history ping-pong image for previous normal/depth validation.
- Descriptor binding 22 reads the previous history image as `SAMPLED_IMAGE`.
- Descriptor binding 23 writes the current history image as `STORAGE_IMAGE`.
- Descriptor bindings 24 and 25 store raw shadow and reflection signals.
- Descriptor bindings 26/27 ping-pong shadow history.
- Descriptor bindings 28/29 ping-pong reflection history.
- Descriptor bindings 30/31 ping-pong surface history.
- Descriptor binding 32 stores the resolved color image used by the final swapchain resolve pass.
- The command buffer runs three compute passes:
  - ray signal pass: writes raw shadow/reflection buffers.
  - denoise pass: reads raw buffers and previous histories, writes denoised signal histories and final color.
- resolve pass: copies 1x resolved color into the swapchain or downsamples when an optional larger internal scale is enabled.
- The denoise shader reprojects each current G-buffer world position into the previous camera/jitter frame before sampling:
  - final color history
  - shadow signal history
  - reflection signal history
- The denoise shader now samples previous histories with validated 2x2 bilinear taps and rejects individual taps across depth/normal discontinuities.
- Final color history falls back to a small 3x3 validated search when the bilinear taps miss due to disocclusion.
- Final color TAA uses padded 3x3 neighborhood clipping, edge-aware history weighting, a residual edge smoothing pass, and lightweight adaptive sharpening.
- Neighborhood min/max clamping is applied around the current pixel after reprojection to reduce ghosting.
- The raw shadow signal uses four channels:
  - `r`: directional RT visibility
  - `gba`: imported local-light direct radiance after RT shadowing
- The camera uniform uses `v4Flags.w` as the current history frame count for v5.
- Camera pose/FOV changes reduce history confidence so the view visibly re-accumulates; static world positions are still carried through by reprojection instead of sampling the same screen pixel.
- Halton jitter is driven by the real frame index, not the history confidence counter, so motion does not freeze the subpixel jitter sequence.

Validation:

- Expected startup log contains `createV5HistoryResources`.
- Expected draw log contains `record v5: dispatch ray signal compute` followed by `record v5: dispatch denoise compute`.
- Expected draw log then contains `record v5: dispatch downsample compute`.
- When the camera is still, Halton jitter should converge edges rather than causing visible whole-frame shimmer.
- During camera movement, history should follow surfaces through previous-frame reprojection instead of sampling the same screen pixel.
- Silhouette disocclusions should reject previous history through surface normal/depth validation.

Asset requirements:

- Triangle mesh geometry must be present in `GpuPreviewGeometry::vertices`.
- The current AS path assumes opaque geometry.
- OBJ/MTL materials are mapped approximately: `Kd/map_Kd` to base color, `Ks` to metalness, and `Ns` to roughness.

## Feature: Swapchain Storage Output

Implementation:

- V5 requests `VK_IMAGE_USAGE_STORAGE_BIT` when creating the swapchain.
- Each swapchain image view gets a v5 descriptor set with:
  - binding 0: camera uniform buffer
  - binding 1: storage image output
  - binding 5: sampler
  - binding 15/16/17: G-buffer albedo, normal, and world-position images
  - binding 21: TLAS acceleration structure
  - binding 22: previous v5 history image
  - binding 23: current v5 history storage image
  - binding 24: raw shadow signal storage image
  - binding 25: raw reflection signal storage image
  - binding 26/27: previous/current shadow history
  - binding 28/29: previous/current reflection history
  - binding 30/31: previous/current surface normal-depth history
- The command buffer transitions the acquired swapchain image:
  - `UNDEFINED` to `GENERAL`
  - denoise compute dispatch writes the full image
  - `GENERAL` to `PRESENT_SRC_KHR`

Validation:

- If the selected surface does not support storage-image swapchain usage, v5 throws a clear error instead of silently falling back.

## Current Limits

- This is a ray-query compute path, not the final full Vulkan ray tracing pipeline path.
- There is no shader binding table yet. V5 currently uses ray-query compute passes, not a full raygen/miss/closest-hit pipeline.
- Hardware RT is currently used for directional shadows and first-hit reflection probes; reflected hit shading still falls back to G-buffer projection where available.
- Reprojection currently targets static geometry via G-buffer world position and previous camera state. It is not yet a full velocity-buffer path for animated/skinned geometry.
- Mitsuba/path-tracer area lights, transmissive glass, and emissive materials are not physically imported yet; v5 still shades with its current realtime light model.
- Screen-space AO is disabled in the main v5 lighting path because it produced unstable false occluders on G-buffer edges.
- Instanced many-light benchmark spheres are still rasterized into the G-buffer but are not yet added as TLAS instances for RT shadows.
- Large text OBJ scenes such as bathroom2 load slowly; a cached binary mesh or `.glb` conversion would be better for repeated preview.

## Next V5 Steps

- Add instanced/procedural geometry to the TLAS.
- Add an optional `v5-pt-preview` path with progressive path tracing style accumulation.
- Add a full `VK_KHR_ray_tracing_pipeline` path with raygen/miss/closest-hit shaders and SBT upload when material hit shading is needed.
- Add variance/moment tracking for a closer SVGF implementation.
- Add a dedicated velocity buffer for animated objects and dynamic geometry.
- Add roughness-aware reflection hit confidence and disocclusion masks.
