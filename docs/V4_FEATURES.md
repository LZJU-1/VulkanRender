# V4 Feature Guide

V4 对齐 Renderer72 的 Deferred Shading & SSAO 阶段。当前实时 Vulkan preview 的主路径已经拆成：

1. shadow atlas pass
2. G-buffer fill pass
3. SSAO raw pass
4. SSAO blur pass
5. fullscreen deferred composition pass

默认演示场景仍使用 `assets\third_party\s72_examples\v3_shadow_demo.shadowdemo`，因为它有地面、柱体、台阶和遮挡物，比较容易观察阴影和 SSAO 接触压暗。

## Feature: V4 Profile And Preview Entry

实现方式：

- `--profile v4` 选择 `v4 deferred and ssao` profile。
- `RendererApp` 为 v4 打开 `enableV2Shading`、`enableV3Shadows` 和 `enableV4Ssao`。
- 不传 `--scene` 时，v4 preview 默认加载 `assets\third_party\s72_examples\v3_shadow_demo.shadowdemo`。
- v4 使用 native Vulkan realtime preview，不走旧的 CPU 预览路径。

素材要求：

- 推荐：`assets\third_party\s72_examples\v3_shadow_demo.shadowdemo`。
- 可选回归：`assets\third_party\s72_examples\materials.s72`，用于观察材质球在 deferred path 下是否正常。

演示方式：

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v4 --preview --width 1280 --height 720
```

观察点：

- `out\vulkan-preview.log` 应包含 `createGBufferRenderPass`、`createSsaoRenderPass`、`createSsaoResources`、`createV4ComposeDescriptors`。
- 窗口应持续 `draw/present`，不应白屏、黑屏或闪退。

## Feature: G-buffer Fill

实现方式：

- 新增 offscreen G-buffer render pass。
- G-buffer attachments：
  - albedo + roughness：`VK_FORMAT_R8G8B8A8_UNORM`
  - normal + metalness：`VK_FORMAT_R16G16B16A16_SFLOAT`
  - world position + material marker：`VK_FORMAT_R32G32B32A32_SFLOAT`
- G-buffer pass 复用 mesh vertex layout 和 material descriptor set。
- `v4_gbuffer.frag.hlsl` 输出材质颜色、法线、粗糙度、金属度和世界坐标。
- 法线贴图和 POM 仍在 G-buffer 阶段执行，因此后续 deferred lighting/SSAO 看到的是材质修正后的 surface 数据。

素材要求：

- 需要有明显几何接触关系的场景：地面与柱体、墙角、台阶、横梁与遮挡物。
- 贴图材质可选；无贴图材质会使用顶点/程序化颜色。

演示方式：

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v4 --preview --width 1280 --height 720
```

观察点：

- 几何应该由 deferred composition 产生最终颜色，而不是 forward material pass。
- 漫游时 G-buffer 结果应随相机稳定更新。

## Feature: SSAO Raw Pass

实现方式：

- 新增 `VK_FORMAT_R32_SFLOAT` 的 `ssaoTarget_`。
- 新增 fullscreen `v4_ssao.frag.hlsl`。
- SSAO pass 读取 G-buffer normal 和 world position，输出单通道 AO。
- 当前采样核为 shader 内固定 16 sample，并使用 interleaved gradient noise 做屏幕空间旋转，避免完全固定方向的条纹。
- AO 半径会随 view depth 变化，近处更紧，远处稍大。

素材要求：

- 推荐使用有接触缝、凹角和遮挡关系的 shadow demo。
- 光滑单个球体不适合验证 SSAO，因为缺少明显接触遮挡。

演示方式：

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v4 --preview --scene assets\third_party\s72_examples\v3_shadow_demo.shadowdemo --width 1280 --height 720
```

观察点：

- 地面与柱体接触处、台阶内角、墙角应有局部压暗。
- 漫游时 AO 应跟随几何关系，不应固定贴在屏幕坐标上。

## Feature: SSAO Blur Pass

实现方式：

- 新增 `VK_FORMAT_R32_SFLOAT` 的 `ssaoBlurTarget_`。
- 新增 fullscreen `v4_ssao_blur.frag.hlsl`。
- blur pass 读取 raw AO 和 G-buffer world position。
- 当前使用 5x5 屏幕空间 blur，并用 world position 差异作为轻量 depth-aware 权重，减少跨物体边缘的 AO 漫开。

素材要求：

- 同 SSAO raw pass。

演示方式：

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v4 --preview --width 1280 --height 720
```

观察点：

- 接触阴影应比 raw SSAO 更平滑。
- 物体边缘附近不应出现大面积灰色泄漏。

## Feature: Deferred Composition

实现方式：

- `v4_fullscreen.vert.hlsl` 绘制 fullscreen triangle。
- `v4_ssao_compose.frag.hlsl` 读取 G-buffer albedo、normal、world position 和 blurred SSAO。
- 背景像素通过 world marker 判断，直接生成 sky radiance。
- 几何像素使用 G-buffer 数据做 deferred lighting，并把 blurred SSAO 乘进 ambient/contact 项。

素材要求：

- 推荐 shadow demo，用于同时观察 deferred lighting、shadow 和 SSAO。
- `materials.s72` 可用于材质球回归，但不是最清晰的 SSAO 场景。

演示方式：

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v4 --preview --scene assets\third_party\s72_examples\v3_shadow_demo.shadowdemo --width 1280 --height 720
```

观察点：

- 法线、粗糙度和金属度会影响最终光照结果。
- SSAO 压暗只应出现在几何附近，不应压暗天空背景。

## Feature: Camera Roaming

实现方式：

- 按 `R` 开启/关闭漫游。
- `WASD` 平移。
- `Q/E` 或 `Ctrl/Space` 垂直移动。
- 方向键或 `IJKL` 转向。
- 按住鼠标右键拖拽转向。
- `Shift` 加速。

演示方式：

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v4 --preview --width 1280 --height 720
```

## Current V4 Notes

- 已实现：G-buffer fill、独立 SSAO raw pass、独立 SSAO blur pass、deferred composition、v3 shadow atlas 复用、漫游相机。
- 与 Renderer72 v4 的主要剩余差距：many-light benchmark/debug view 还没完全对齐，Renderer72 README 里的 `1024 sphere lights + 10000 PBR spheres` 仍是下一步重点。
- 当前 SSAO 是 fullscreen graphics pass，不是 compute pass；RenderGraph 里的 `ssao.generate`/`ssao.blur` 语义已经对应实际 pass，但底层执行队列仍是 raster fullscreen。
