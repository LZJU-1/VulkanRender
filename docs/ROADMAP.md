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
- v2 GPU material path for the official `materials.s72` demo, including material families, texture descriptors, normal/POM detail, IBL, MSAA, mipmaps, anisotropic filtering, mesh tangents, and tone mapping.
- Native Vulkan GPU preview window for mesh scenes: Win32 surface, swapchain, graphics pipeline, depth buffer, vertex buffer upload, uniform camera, and roaming controls.
- v3 GPU shadow/light path: depth shadow atlas with directional cascades, spot shadow map, point/sphere omni shadow faces, and realtime PBR lighting.

## Next Engineering Steps

1. Add G-buffer image resources and deferred composition for v4.
2. Implement RT resources: BLAS/TLAS build inputs, scratch buffers, shader binding table, and ray tracing dispatch.
3. Port selected offline path tracing material/sampling ideas into realtime-friendly shader code.
