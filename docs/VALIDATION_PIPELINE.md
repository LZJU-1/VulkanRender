# Renderer72-Aligned Validation Pipeline

This project tracks the public `YJJfish/Renderer72` README as a staged reference:

| Local profile | Reference stage | Reference feature group |
| --- | --- | --- |
| `v1` | Renderer72 `v1.0` | scene loader, simple material, animation, frustum culling |
| `v2` | Renderer72 `v2.0` | skybox, tone mapping, normal mapping, displacement mapping, PBR material, IBL, environment precompute |
| `v3` | Renderer72 `v3.0` | spot shadows, sphere/point omni shadows, directional cascaded shadows, cascade visualization |
| `v4` | Renderer72 `v4.0` | deferred shading, SSAO, 1024 sphere lights, 10000 PBR spheres |
| `v5-rt` | local extension | hybrid G-buffer + Vulkan ray-query shadows, temporal accumulation, OBJ/MTL/XML import |

Print the current pipeline from the executable:

```powershell
build\nmake-debug\src\vulkan_render.exe --validation-pipeline
build\nmake-debug\src\vulkan_render.exe --validation-pipeline v2
```

The printed plan is data-backed by `src/core/ValidationPipeline.cpp` and covered by the profile smoke tests. It is intentionally a validation plan, not an always-on CI run: v1/v2 headless renders are automation-friendly, while v2-v5 realtime preview checks need a Vulkan-capable Windows desktop and visual inspection.

## Per-Version Gate

Recommended order:

1. Build and run profile tests:

```powershell
scripts\build_msvc.bat
ctest --preset nmake-debug
```

2. Print the plan for the target version:

```powershell
build\nmake-debug\src\vulkan_render.exe --validation-pipeline v1
```

3. Run the commands listed under that version.

4. Check the listed log markers and visual observations.

## Version Notes

`v1` is the strongest headless gate. It should produce BMP outputs for the built-in cube scene and official Scene72 examples, then use preview mode to confirm driver animation.

`v2` combines a software BMP smoke render with the Vulkan preview path. The reference features are material-heavy, so final acceptance should come from the GPU preview: skybox, tone mapping, PBR/IBL response, normal detail, displacement/parallax stability, mipmaps, and anisotropic filtering.

`v3` is preview-first. The dedicated `.shadowdemo` scene exists because `materials.s72` is useful for material regression but weak for judging shadow projection. The default v3 preview should show directional cascades, spot shadowing, and point/sphere omni shadow behavior.

`v4` is also preview-first. The default `.manylights` marker is the Renderer72-scale benchmark gate and should report `lights=1024` and `sphereInstances=10000`. The shadow demo remains useful for SSAO contact regression and debug views.

`v5-rt` is not in the public Renderer72 README. It is kept in the same pipeline so the local realtime ray tracing work has the same validation shape: expected log markers, primary assets, and visual acceptance checks.
