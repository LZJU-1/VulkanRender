# Roadmap

## Completed In This Rewrite Pass

- New CMake project structure.
- Explicit staged feature profiles from v1 through v5.
- Realtime ray tracing profile and capability checks.
- Mock backend for SDK-free builds.
- Vulkan backend for device and extension enumeration when the SDK is available.
- Unit tests for profile and graph behavior.
- v1 software render and realtime preview window.
- Official Scene'72 v1 loader with driver animation.
- Static glTF/GLB mesh import.
- v2 software validation path for the official `materials.s72` demo, including material families, UV/normal attributes, PNG texture sampling, approximate environment lighting, and tone mapping.
- Native Vulkan GPU preview window for mesh scenes: Win32 surface, swapchain, graphics pipeline, depth buffer, vertex buffer upload, uniform camera, and roaming controls.

## Next Engineering Steps

1. Move the v2 material model from vertex colors into Vulkan descriptor sets, sampled images, and fragment shaders.
2. Add true cubemap/environment prefilter resources for diffuse irradiance and roughness-dependent specular IBL.
3. Add normal/displacement evaluation in the v2 GPU shader path.
4. Add G-buffer image resources and deferred composition for v4.
5. Implement RT resources: BLAS/TLAS build inputs, scratch buffers, shader binding table, and ray tracing dispatch.
6. Port selected offline path tracing material/sampling ideas into realtime-friendly shader code.
