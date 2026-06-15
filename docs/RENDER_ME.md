# Render Me

This is the project-facing version map for testing and demos.

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

This opens the dedicated many-light deferred demo scene by default. It contains a 16x16 PBR sphere field and 256 procedural point lights in the v4 fullscreen composition shader. Press `R` for roaming; hold right mouse and drag to look.

Shadow/SSAO regression scene:

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v4 --preview --scene assets\third_party\s72_examples\v3_shadow_demo.shadowdemo --width 1280 --height 720
```

## v5 Realtime Ray Tracing

Target behavior: opt into hardware ray tracing for primary visibility, reflections, shadows, or hybrid accumulation.

Current implementation: the app detects Vulkan RT capability and exposes the RT graph stages. Concrete Vulkan BLAS/TLAS allocation and shader binding table upload are the next implementation step.
