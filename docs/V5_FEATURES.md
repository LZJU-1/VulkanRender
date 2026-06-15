# V5 Feature Guide

V5 is the realtime ray tracing profile. The current implementation follows a modern hybrid-game layout: rasterized G-buffer first, hardware Vulkan ray-query shadows from a TLAS/BLAS scene, temporal accumulation for low-sample RT stability, then a GPU compute compose pass writing directly into the swapchain as a storage image.

## Feature: V5 Profile And Preview Entry

Implementation:

- `--profile v5-rt` and aliases such as `v5`, `rt`, and `raytracing` select the v5 profile.
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
- repeated `draw/present`

PathTracer bathroom2 demo:

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v5-rt --preview --scene C:\Users\lzju\Desktop\MonteCarloPathTracer\scenes\bathroom2\bathroom2.xml --width 1280 --height 720
```

## Feature: GPU Compute Ray Generation

Implementation:

- Added `shaders/vulkan_gpu/v5_raytrace.comp.hlsl`.
- The shader is compiled to `shaders/vulkan_gpu/v5_raytrace.comp.spv` with DXC SPIR-V output.
- The compute shader uses one thread per pixel and dispatches in 8x8 workgroups.
- For every pixel, the shader reads G-buffer albedo, normal, and world-position data and reconstructs shading in screen space.
- The shader binds `RaytracingAccelerationStructure sceneTlas` at binding 21 and uses `RayQuery` for shadow visibility.
- The shader reads a previous-frame history texture and writes the current accumulated color to a ping-pong history target.

Scene features:

- Hardware ray-query directional shadows.
- Screen-space reflections for glossy/metallic surfaces.
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

## Feature: Temporal RT Accumulation

Implementation:

- V5 allocates two `VK_FORMAT_R16G16B16A16_SFLOAT` history images.
- Descriptor binding 22 reads the previous history image as `SAMPLED_IMAGE`.
- Descriptor binding 23 writes the current history image as `STORAGE_IMAGE`.
- The command buffer ping-pongs the two history images every frame and inserts layout barriers around compute.
- The camera uniform uses `v4Flags.w` as the current history frame count for v5.
- Camera pose/FOV changes reset the accumulation so roaming does not smear old views.

Validation:

- Expected startup log contains `createV5HistoryResources`.
- When the camera is still, the soft RT shadow noise should settle over multiple frames.

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
- The command buffer transitions the acquired swapchain image:
  - `UNDEFINED` to `GENERAL`
  - compute dispatch writes the full image
  - `GENERAL` to `PRESENT_SRC_KHR`

Validation:

- If the selected surface does not support storage-image swapchain usage, v5 throws a clear error instead of silently falling back.

## Current Limits

- This is a ray-query compute path, not the final full Vulkan ray tracing pipeline path.
- There is no shader binding table yet.
- Hardware RT is currently used for directional shadows. Reflections are still screen-space.
- Mitsuba/path-tracer area lights, transmissive glass, and emissive materials are not physically imported yet; v5 still shades with its current realtime light model.
- Screen-space AO is disabled in the main v5 lighting path because it produced unstable false occluders on G-buffer edges.
- Instanced many-light benchmark spheres are still rasterized into the G-buffer but are not yet added as TLAS instances for RT shadows.
- Large text OBJ scenes such as bathroom2 load slowly; a cached binary mesh or `.glb` conversion would be better for repeated preview.

## Next V5 Steps

- Add instanced/procedural geometry to the TLAS.
- Add a full `VK_KHR_ray_tracing_pipeline` path with raygen/miss/closest-hit shaders and SBT upload.
- Add motion-vector reprojection/history clamping instead of the current camera-reset-only accumulation.
- Add an edge-aware spatial filter for the shadow visibility signal before temporal resolve.
