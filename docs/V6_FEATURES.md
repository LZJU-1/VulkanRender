# V6 Hybrid Feature Guide

V6 is the realtime hybrid ray tracing profile aligned to `.reference/VulkanHybridRenderer`. Its default mode matches the reference UI state used for acceptance:

- raytraced shadows
- raytraced ambient occlusion
- raytraced reflections
- denoise shadows and ambient occlusion enabled

## Profile And Entry

- `--profile v6-hybrid` selects the v6 profile.
- Compatibility aliases such as `v6`, `v6-rt`, `hybrid-rt`, `rt`, `raytracing`, `v5`, and `v5-rt` also resolve to the v6 profile.
- `--enable-rt` and `--require-rt` select `v6-hybrid`.
- `RendererApp` enables the RT path automatically for this profile.

Demo:

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v6-hybrid --preview --scene assets\third_party\s72_examples\materials.s72 --width 1280 --height 720
```

Expected log markers:

- `hybridRayTracing=on`
- `createV6HistoryResources`
- `createV6AccelerationStructures: triangles=... tlas=ready`
- `createV6RayTracingDescriptors`
- `shadowMode=raytraced aoMode=raytraced reflectionMode=raytraced`
- `denoise=on(svgf-temporal-bilateral)`
- `mode=v6-reference-hybrid-rt`
- `record v6: dispatch raytrace signal compute`
- `record v6: dispatch denoise compute`
- `record v6: dispatch downsample compute`

## Reference Alignment

The reference hybrid renderer registers this default pass sequence:

1. `G-Buffer Pass`
2. `Raytrace Pass`
3. `SVGF Denoise Pass`
4. `Composition Pass`

The local v6 graph mirrors that shape as:

1. `gbuffer.fill-reference-hybrid`
2. `rt.build-blas`
3. `rt.build-tlas`
4. `rt.reference-raytraced-shadows`
5. `rt.reference-raytraced-ambient-occlusion`
6. `rt.reference-raytraced-reflections`
7. `denoise.svgf-shadow-ao`
8. `denoise.temporal-reflection`
9. `post.taa-resolve-and-downsample`

The compiled reference shader payloads are mirrored under `shaders/v6/hybrid_render_path`. Runtime preview currently keeps this project's established ray-query compute pipeline for reproducible local builds, while matching the reference signal layout and default visual modes.

## Runtime Pipeline

- The preview fills an offscreen G-buffer for primary visibility.
- BLAS/TLAS are built for the uploaded triangle mesh.
- The ray-query pass writes `Raytraced Shadows and Ambient Occlusion` style signal data and a separate raytraced reflection signal.
- The denoise pass applies temporal reprojection and bilateral filtering in the same broad SVGF structure as the reference path.
- The resolve/downsample pass writes the final image to the swapchain storage image.

## Device Requirements

V6 requires a Vulkan device with:

- `VK_KHR_acceleration_structure`
- `VK_KHR_buffer_device_address`
- `VK_KHR_deferred_host_operations`
- `VK_KHR_ray_query`
- `VK_KHR_spirv_1_4`
- `VK_KHR_shader_float_controls`

Use `--require-rt` when missing RT support should fail the run instead of reporting fallback capability.

## Notes

Internal preview resource names still use `v5*` in several places because this path evolved from the previous local RT prototype. The public profile, render graph, validation plan, logs, and defaults are v6.
