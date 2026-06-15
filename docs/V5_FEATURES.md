# V5 Feature Guide

V5 is the realtime ray tracing profile. The first runnable implementation uses a Vulkan compute shader as the ray generation path and writes directly into the swapchain as a storage image. This is a GPU realtime ray tracer and a stepping stone toward the later KHR ray tracing pipeline version with BLAS/TLAS/SBT.

## Feature: V5 Profile And Preview Entry

Implementation:

- `--profile v5-rt` and aliases such as `v5`, `rt`, and `raytracing` select the v5 profile.
- `RendererApp` now allows v5 in realtime preview mode.
- `V1RenderSettings::enableV5RayTracing` selects the Vulkan compute ray tracing path.
- The preview window still supports the existing roaming camera controls, so the ray traced camera can be moved in realtime.

Asset requirements:

- The current v5 shader traces a procedural validation scene in shader code: reflective spheres, a checker floor, hard shadows, and sky reflection.
- Mesh BLAS/TLAS scene ingestion is intentionally left for the next v5 step.

Demo:

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v5-rt --preview --width 1280 --height 720
```

Expected log markers:

- `createV5RayTracingDescriptors`
- `v5RayTracing=on`
- repeated `draw/present`

## Feature: GPU Compute Ray Generation

Implementation:

- Added `shaders/vulkan_gpu/v5_raytrace.comp.hlsl`.
- The shader is compiled to `shaders/vulkan_gpu/v5_raytrace.comp.spv` with DXC SPIR-V output.
- The compute shader uses one thread per pixel and dispatches in 8x8 workgroups.
- For every pixel, the shader builds a camera ray from the same camera uniform layout used by the Vulkan preview.

Scene features:

- Procedural sphere intersections.
- Procedural plane intersection.
- Hard shadow ray against the same procedural scene.
- Reflective sky contribution for metal-like spheres.
- Tone mapping and gamma correction.

## Feature: Swapchain Storage Output

Implementation:

- V5 requests `VK_IMAGE_USAGE_STORAGE_BIT` when creating the swapchain.
- Each swapchain image view gets a v5 descriptor set with:
  - binding 0: camera uniform buffer
  - binding 1: storage image output
- The command buffer transitions the acquired swapchain image:
  - `UNDEFINED` to `GENERAL`
  - compute dispatch writes the full image
  - `GENERAL` to `PRESENT_SRC_KHR`

Validation:

- If the selected surface does not support storage-image swapchain usage, v5 throws a clear error instead of silently falling back.

## Current Limits

- This is not yet the final Vulkan KHR ray tracing pipeline path.
- There is no BLAS/TLAS construction yet.
- There is no shader binding table yet.
- The first scene is procedural rather than imported from `.s72`/`.glb`.
- The current render is single-sample per frame with small pixel jitter, not full temporal accumulation.

## Next V5 Steps

- Add a hardware RT capability path inside the preview renderer, independent of the current configure-time SDK check.
- Build BLAS from `GpuPreviewGeometry::vertices`.
- Build a TLAS with at least one mesh instance.
- Add raygen/miss/closest-hit shaders and SBT upload.
- Add accumulation/reset on camera movement.
- Add fallback selection between KHR RT pipeline and compute ray tracing.
