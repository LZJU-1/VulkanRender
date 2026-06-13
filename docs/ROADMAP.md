# Roadmap

## Completed In This Rewrite Pass

- New CMake project structure.
- Explicit staged feature profiles from v1 through v5.
- Realtime ray tracing profile and capability checks.
- Mock backend for SDK-free builds.
- Vulkan backend for device and extension enumeration when the SDK is available.
- Unit tests for profile and graph behavior.

## Next Engineering Steps

1. Replace mock scene pass data with a compact JSON scene loader.
2. Add window/swapchain support behind the RHI.
3. Implement v1 forward raster pass with simple shaders.
4. Add G-buffer image resources and deferred composition.
5. Implement RT resources: BLAS/TLAS build inputs, scratch buffers, shader binding table, and ray tracing dispatch.
6. Port selected offline path tracing material/sampling ideas into realtime-friendly shader code.

