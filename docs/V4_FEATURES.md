# V4 Feature Guide

V4 对齐 Renderer72 的 deferred + SSAO 阶段：先把几何信息写入 G-buffer，再用 fullscreen composition pass 做延迟光照和屏幕空间环境光遮蔽。当前实现是 Vulkan realtime preview 路径，默认使用 v3 shadow demo 场景，因为墙角、台阶、立柱和横梁更容易观察 SSAO 的接触压暗。

## Feature: V4 Profile And Preview Entry

实现方式：
- CLI 通过 `--profile v4` 选择 deferred/SSAO profile。
- `RendererApp` 为 v4 打开 `enableV2Shading`、`enableV3Shadows` 和 `enableV4Ssao`。
- v4 复用 v3 的 shadow demo 场景和漫游相机，但主画面改走 G-buffer + composition。
- 不传 `--scene` 时，v4 preview 默认加载 `assets\third_party\s72_examples\v3_shadow_demo.shadowdemo`。

素材要求：
- 主推荐：`assets\third_party\s72_examples\v3_shadow_demo.shadowdemo`。
- 可选材质回归：`assets\third_party\s72_examples\materials.s72`。

演示方式：

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v4 --preview --width 1280 --height 720
```

观察点：
- `out\vulkan-preview.log` 应包含 `createGBufferRenderPass`、`createGBufferResources`、`createV4ComposeDescriptors`。
- 窗口应持续 `draw/present`。

## Feature: G-buffer Fill

实现方式：
- 新增 offscreen G-buffer render pass。
- G-buffer attachments：
  - albedo + roughness：`R8G8B8A8_UNORM`
  - normal + metalness：`R16G16B16A16_SFLOAT`
  - world position + material kind marker：`R32G32B32A32_SFLOAT`
- G-buffer pass 复用 mesh vertex layout 和 material descriptor set。
- `v4_gbuffer.frag.hlsl` 输出材质颜色、法线、粗糙度、金属度和世界坐标。

素材要求：
- 需要有明显接触关系的几何：地面与立柱、墙与台阶、横梁与遮挡物。
- 贴图材质可选；无贴图材质会使用顶点/程序化颜色。

演示方式：

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v4 --preview --width 1280 --height 720
```

观察点：
- 场景几何应正常显示，没有退回白屏或 sky-only。
- 漫游时 G-buffer 应随相机稳定更新。

## Feature: Deferred Composition

实现方式：
- 新增 fullscreen triangle vertex shader：`v4_fullscreen.vert.hlsl`。
- 新增 composition fragment shader：`v4_ssao_compose.frag.hlsl`。
- composition pass 采样 G-buffer 的 albedo、normal、world position。
- 背景像素通过 world marker 判断，直接生成 sky radiance；几何像素做 deferred lighting。

素材要求：
- 任意 v4 mesh scene 均可。
- 推荐 shadow demo，因为接触面清楚。

演示方式：

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v4 --preview --scene assets\third_party\s72_examples\v3_shadow_demo.shadowdemo --width 1280 --height 720
```

观察点：
- 画面应由 fullscreen composition 产生，而不是 forward material pass。
- 物体法线、粗糙度和金属度会影响延迟光照结果。

## Feature: SSAO

实现方式：
- composition shader 在屏幕空间采样周围 world position。
- 根据 view depth、world distance 和 normal facing 估算 occlusion。
- SSAO 主要压暗接触缝、墙角、台阶内侧和遮挡物靠近地面的区域。
- 当前实现是单 pass SSAO；Renderer72 的独立 SSAO + blur pass 已在架构上对齐，后续可以拆成单独 AO attachment 和 blur pass。

素材要求：
- 需要有接触缝或凹角。推荐 v3/v4 shadow demo。

演示方式：

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v4 --preview --width 1280 --height 720
```

观察点：
- 地面与立柱接触处应更暗。
- 台阶内角、墙角应有轻微环境遮蔽。
- 漫游时遮蔽应稳定跟随几何关系，而不是贴在屏幕固定位置。

## Feature: Camera Roaming

实现方式：
- 复用 GPU preview 的漫游相机。
- 按 `R` 开启漫游。
- `WASD` 平移，`Q/E` 或 `Ctrl/Space` 垂直移动。
- 方向键或 `IJKL` 转向。
- 鼠标右键拖拽转向。

演示方式：

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v4 --preview --width 1280 --height 720
```

## Current V4 Notes

- 已实现 v4 realtime preview：G-buffer fill、deferred composition、SSAO visual result。
- 当前 SSAO 合并在 composition shader 内，方便快速验证视觉效果。
- 后续细化方向：独立 SSAO image、separable blur、debug view、many-light tiled/clustered composition。
