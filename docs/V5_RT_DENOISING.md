# V5 Realtime RT Denoising Notes

This document records the v5 realtime ray tracing image-quality path and the reasoning behind the current implementation.

## Problem

Realtime ray tracing cannot rely on many rays per pixel the way the offline path tracer can. The renderer therefore needs two separate stability systems:

- Subpixel antialiasing for primary visibility and geometric edges.
- Signal denoising for stochastic RT shadows and reflections.

Halton jitter helps antialiasing by moving the projection by a fraction of a pixel each frame. However, jittered history is only valid if the previous frame is sampled at the matching previous-frame location. Sampling the same pixel every frame causes whole-image shimmer because the primary G-buffer itself has moved.

## Current V5 Frame Graph

1. Rasterize the scene into a jittered G-buffer.
2. For v5 quality mode, rasterize/read the G-buffer at 2x internal resolution.
3. Run `v5_raytrace.comp.hlsl`.
   - Cast ray-query shadows against the TLAS.
   - Compute first-hit reflection probes.
   - Write raw shadow and reflection signal buffers.
4. Run `v5_denoise.comp.hlsl`.
   - Reproject current world positions into the previous camera+jitter frame.
   - Validate the reprojected sample against previous normal/depth surface history.
   - Sample previous final-color, shadow, and reflection histories at the reprojected UV.
   - Clamp history against a padded 3x3 current neighborhood.
   - Run normal/depth/material-guided bilateral filtering.
   - Compose final lighting, edge-aware resolve, luma directional silhouette smoothing, lightweight sharpen, and write a high-resolution resolved color image.
5. Run `v5_downsample.comp.hlsl`.
   - Downsample the high-resolution resolved color image to the swapchain.

## Signal Buffers

V5 uses separate RT signal buffers because shadow and reflection noise have different behavior.

- Shadow signal:
  - `r`: directional light visibility.
  - `gba`: local-light direct radiance after RT shadowing.
- Reflection signal:
  - `rgb`: reflection radiance.
  - `a`: reflection confidence/weight.

Each signal has its own ping-pong history. The final tone-mapped color also keeps a ping-pong history.

V5 also stores a surface history:

- `rgb`: previous normal encoded as `normal * 0.5 + 0.5`.
- `a`: previous view-space depth.

The surface history is not a lighting signal. It is used to reject history when a reprojected UV points to a different surface.

## Internal Supersampling

Thin furniture silhouettes in bathroom2 can remain visibly aliased even after TAA and FXAA-style resolve because the primary G-buffer is otherwise still a single coverage sample per output pixel. V5 now renders its realtime RT path at 2x internal resolution:

```text
1280x720 window -> 2560x1440 v5 G-buffer / RT / denoise / history -> 1280x720 downsample
```

This makes the final edge resolve use real subpixel coverage rather than only post-process guesses. The cost is roughly 4x more pixels for the v5 G-buffer, RT signal dispatch, denoise dispatch, and history textures.

## Reprojection

The camera uniform stores current and previous camera frames:

- current eye/right/up/forward/FOV/aspect
- current Halton jitter
- previous eye/right/up/forward/FOV/aspect
- previous Halton jitter

The denoiser uses the current G-buffer world position for each pixel and projects it into the previous frame:

```text
current pixel -> current world position -> previous camera clip -> previous history UV
```

This gives a static-scene motion vector without adding a separate velocity attachment. It is enough for camera movement and subpixel jitter in bathroom2. Animated or deforming objects still need a real velocity buffer.

After reprojection, the denoiser samples previous surface history at the same UV. The sample is accepted only when:

- previous normal is close enough to the current normal
- previous depth is close enough to the projected previous-frame depth
- previous UV is inside the frame

This keeps history from bleeding across newly exposed silhouettes.

## Ghosting Controls

The denoiser limits stale history with:

- padded 3x3 neighborhood min/max clipping around current color/signals
- luma disagreement checks for final color and reflection
- normal/depth/material bilateral weights
- previous-UV rejection when the reprojected point falls off screen
- previous normal/depth rejection from surface history
- edge-aware history weights so stable silhouettes can accumulate Halton coverage
- luma-directional edge resolve for residual stair steps on high-contrast thin silhouettes
- lightweight final sharpen after edge smoothing

This is closer to SVGF/NRD structure, but it is not yet a full production denoiser.

## Remaining Work

- Add a velocity buffer for animated/skinned/dynamic geometry.
- Add variance and moment buffers for SVGF-style adaptive temporal weights.
- Add disocclusion confidence from previous depth/normal.
- Make reflection filtering roughness-aware over multiple kernel radii.
- Move first-hit reflection shading toward a full ray tracing pipeline with hit shaders and material lookup.
- Add explicit debug views for raw shadow, denoised shadow, raw reflection, denoised reflection, history weight, and reprojection failure.
- Add a real MSAA or supersampled G-buffer resolve if thin geometry still aliases after temporal/FXAA resolve.

## Bathroom2 Validation

Use bathroom2 as the v5 validation scene:

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v5-rt --preview --scene C:\Users\lzju\Desktop\MonteCarloPathTracer\scenes\bathroom2\bathroom2.xml --width 1280 --height 720
```

Expected log markers:

```text
createV5AccelerationStructures: triangles=1243923 tlas=ready
taa=halton16-surface-validated-resolve
denoise=split-shadow-reflection-temporal-bilateral-sharpen
internalScale=2x
record v5: dispatch ray signal compute
record v5: dispatch denoise compute
record v5: dispatch downsample compute
draw: present
```
