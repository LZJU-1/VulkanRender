# Render Me

This is the project-facing version map for testing and demos.

## v1 Scene Forward

Target behavior: load a scene, advance animation time, cull objects against the camera frustum, and draw simple forward materials.

Current implementation: profile and pass graph are wired; scene loading is represented as a deterministic pass boundary.

Smoke render:

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v1 --render --scene assets\scenes\v1.scene --output out\v1.bmp --width 1280 --height 720 --frames 16
```

This produces a headless BMP with simple material cubes, animation at the selected frame, and a deliberately off-frustum cube that is counted as culled.

Realtime preview:

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v1 --preview --scene assets\scenes\v1.scene --width 960 --height 540
```

Official Scene'72 examples:

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v1 --render --scene assets\third_party\s72_examples\rotation.s72 --output out\official-rotation.bmp --width 960 --height 960
build\nmake-debug\src\vulkan_render.exe --profile v1 --render --scene assets\third_party\s72_examples\sg-Articulation.s72 --output out\official-articulation.bmp --width 1280 --height 720
build\nmake-debug\src\vulkan_render.exe --profile v1 --preview --scene assets\third_party\s72_examples\rotation.s72 --width 720 --height 720
build\nmake-debug\src\vulkan_render.exe --profile v1 --preview --scene assets\third_party\s72_examples\sg-Articulation.s72 --width 1280 --height 720
```

Generic glTF/GLB static mesh smoke test:

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v1 --render --scene path\to\model.glb --output out\model.bmp --width 1280 --height 720
```

## v2 PBR And IBL

Target behavior: add skybox rendering, tone mapping, normal mapping, displacement mapping, PBR material parameters, and environment map precomputation.

Current implementation: graph stages are named and ordered so shader/resource work can be filled in without changing the public profile.

## v3 Lights And Shadows

Target behavior: support spot lights with perspective shadow maps, sphere lights with omnidirectional shadow maps, and directional lights with cascaded shadow maps.

Current implementation: shadow and light pass slots are present.

## v4 Deferred And SSAO

Target behavior: render G-buffer data, apply SSAO, and compose many lights efficiently.

Current implementation: deferred graph shape is ready and tested.

## v5 Realtime Ray Tracing

Target behavior: opt into hardware ray tracing for primary visibility, reflections, shadows, or hybrid accumulation.

Current implementation: the app detects Vulkan RT capability and exposes the RT graph stages. Concrete Vulkan BLAS/TLAS allocation and shader binding table upload are the next implementation step.
