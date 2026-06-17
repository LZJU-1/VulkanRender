# Render Me

This is the project-facing version map for testing and demos.

For the executable version of the Renderer72-aligned checklist, run:

```powershell
build\nmake-debug\src\vulkan_render.exe --validation-pipeline
build\nmake-debug\src\vulkan_render.exe --validation-pipeline v3
```

See `docs/VALIDATION_PIPELINE.md` for the compact version-to-reference map.

## v1 Scene Forward

Target behavior: load a scene, advance animation time, cull objects against the camera frustum, and draw simple forward materials.

Current implementation: see `docs/V1_FEATURES.md` for the feature-by-feature implementation, asset requirements, and demo commands.

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

Current implementation: see `docs/V2_FEATURES.md` for the feature-by-feature implementation, asset requirements, and demo commands.

Official materials render:

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v2 --render --scene assets\third_party\s72_examples\materials.s72 --output out\v2-materials.bmp --width 1280 --height 720
```

Realtime preview:

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v2 --preview --scene assets\third_party\s72_examples\materials.s72 --width 1280 --height 720
```

Preview path: this command opens the native Vulkan GPU preview window. Scene data is loaded on the CPU, uploaded to Vulkan buffers/textures, and rasterized by the GPU through the swapchain graphics pipeline. The legacy CPU preview remains only as a fallback for the old `.scene` cube smoke format.

Preview controls: press `R` to toggle free camera roaming, `WASD` to move, `Q/E` or `Ctrl/Space` for vertical movement, arrow keys or `IJKL` to look, hold right mouse and drag to look, `Shift` to move faster, and `Esc` to close the window.

Implementation note: headless `--render` remains a software validation path for BMP output. Realtime v2 material validation should use `--preview`.

## v3 Lights And Shadows

Target behavior: support spot lights with perspective shadow maps, sphere lights with omnidirectional shadow maps, and directional lights with cascaded shadow maps.

Current implementation: see `docs/V3_FEATURES.md`. The GPU preview uses a depth shadow atlas with directional cascades, a spot light shadow map, and six point/sphere omni shadow faces, then applies those shadows in the PBR material shader.

Realtime preview:

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v3 --preview --width 1280 --height 720
```

This opens the dedicated v3 shadow demo scene by default. To be explicit, pass `--scene assets\third_party\s72_examples\v3_shadow_demo.shadowdemo`. The old `materials.s72` scene is still useful for v2 material/IBL regression, but it is not the clearest shadow-map demonstration.

## v4 Deferred And SSAO

Target behavior: render G-buffer data, apply SSAO, and compose many lights efficiently.

Current implementation: see `docs/V4_FEATURES.md`. The realtime Vulkan preview fills an offscreen G-buffer, generates an `R32_SFLOAT` SSAO texture, blurs it into a second `R32_SFLOAT` target, then runs fullscreen deferred composition.

Realtime preview:

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v4 --preview --width 1280 --height 720
```

This opens the dedicated many-light deferred demo scene by default. It contains 10000 instanced PBR spheres and 1024 storage-buffer lights consumed by the v4 fullscreen composition shader. Press `R` for roaming; hold right mouse and drag to look. Press `0-5` to switch final/albedo/normal/depth/SSAO debug views.

Shadow/SSAO regression scene:

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v4 --preview --scene assets\third_party\s72_examples\v3_shadow_demo.shadowdemo --width 1280 --height 720
```

## v6 Hybrid Realtime Ray Tracing

Target behavior: match `.reference/VulkanHybridRenderer` with the default UI state from the reference screenshot: raytraced shadows, raytraced ambient occlusion, raytraced reflections, and denoise enabled.

Current implementation: see `docs/V6_FEATURES.md`. The realtime preview rasterizes the selected mesh scene into a G-buffer, builds BLAS/TLAS acceleration structures for the main triangle mesh, writes raytraced shadow/AO and reflection signal buffers, runs an SVGF-style denoise pass, then resolves through the swapchain storage image.

Realtime preview:

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v6-hybrid --preview --scene assets\third_party\s72_examples\materials.s72 --width 1280 --height 720
```

Expected log markers: `createGBufferRenderPass`, `createV6HistoryResources`, `createV6AccelerationStructures: triangles=... tlas=ready`, `createV6RayTracingDescriptors`, `hybridRayTracing=on`, `shadowMode=raytraced aoMode=raytraced reflectionMode=raytraced`, `denoise=on(svgf-temporal-bilateral)`, `record v6: dispatch raytrace signal compute`, `record v6: dispatch denoise compute`, `record v6: dispatch downsample compute`, and repeated `draw/present`.

PathTracer bathroom2 scene:

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v6-hybrid --preview --scene C:\Users\lzju\Desktop\MonteCarloPathTracer\scenes\bathroom2\bathroom2.xml --width 1280 --height 720
```

This imports the PathTracer OBJ/MTL scene, applies the companion XML camera, builds a TLAS over the bathroom mesh, and runs the same v6 hybrid ray-query path. First load is slow because `bathroom2.obj` is a large text mesh.
