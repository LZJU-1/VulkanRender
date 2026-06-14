# V2 Feature Guide

V2 对齐的是 Renderer72 第二阶段的渲染管线功能：skybox、tone mapping、normal mapping、displacement/POM、PBR、IBL、材质贴图描述符和批次绘制。当前 v2 的主验证场景是官方 Scene72 `materials.s72`。

## Feature: Vulkan GPU Realtime Preview

实现方式：

- `--profile v2 --preview` 走 Win32 + Vulkan swapchain。
- CPU 负责 Scene72/glTF 数据导入，GPU 负责 vertex buffer、depth、MSAA render pass、descriptor set、shader shading 和 present。
- `.s72` v2 材质场景在 preview 中使用 GPU path，不再用 CPU 光栅化当实时主路径。

素材要求：

- 推荐使用官方材质场景：`assets\third_party\s72_examples\materials.s72`。
- 场景引用的 `.b72`、贴图和 IBL 图片需要保持在同一素材目录。

演示方式：

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v2 --preview --scene assets\third_party\s72_examples\materials.s72 --width 1280 --height 720
```

观察点：

- 应打开 Vulkan GPU preview window。
- 控制台会打印顶点数；`out\vulkan-preview.log` 会记录 swapchain、texture、draw/present 等初始化信息。

## Feature: Scene72 Material Scene

实现方式：

- v2 loader 读取 Scene72 `MATERIAL` 和 `ENVIRONMENT`。
- 支持 material family：simple、environment、mirror、lambertian、pbr。
- 支持 `POSITION`、`NORMAL`、`TANGENT`、`TEXCOORD`、`COLOR`。
- mesh 被转成 `GpuPreviewVertex`，材质被归并成 material set 和 batch。

素材要求：

- 主场景：`assets\third_party\s72_examples\materials.s72`。
- 需要以下几类 `.b72`：
  - `materials.*.pnc.b72`：position、normal、color。
  - `materials.*.pnTtc.b72`：position、normal、tangent、texcoord、color。

演示方式：

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v2 --preview --scene assets\third_party\s72_examples\materials.s72 --width 1280 --height 720
```

观察点：

- 场景应出现多组材质球和标签。
- 有贴图的材质球应显示纹理、法线和位移细节。

## Feature: Material Descriptor Sets And Batch Drawing

实现方式：

- `GpuPreviewGeometry` 保存 `materials` 和 `batches`。
- Vulkan 为每个 material texture set 创建 descriptor set。
- 绘制时按 batch 绑定对应 descriptor set，再执行 `vkCmdDraw`。
- 这样避免所有材质挤在一个全局 texture 状态里，后续扩展更多材质类型更自然。

素材要求：

- 需要一个场景中包含多种材质或多套贴图。
- 推荐 `materials.s72`，当前会形成多个 material set 和 batch。

演示方式：

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v2 --preview --scene assets\third_party\s72_examples\materials.s72 --width 1280 --height 720
```

观察点：

- 不同材质球应显示不同材质响应。
- 如果 descriptor/batch 绑定错，常见现象是所有球共享同一套贴图或颜色。

## Feature: Skybox And Cubemap Environment

实现方式：

- Scene72 RGBE vertical cube strip 会在 CPU 解包成 floating-point cubemap。
- Vulkan image 使用 `VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT`。
- skybox pass 使用 cube texture 采样可见背景。
- v2 使用官方 Ox Bridge Morning 环境，而不是程序蓝色背景。

素材要求：

- `assets\third_party\s72_examples\ox_bridge_morning.png`
- 图片必须是 vertical 6-face strip，作为 cubemap 上传。

演示方式：

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v2 --preview --scene assets\third_party\s72_examples\materials.s72 --width 1280 --height 720
```

观察点：

- 背景应是桥/户外环境。
- 如果 cubemap face 解析错，会看到大块纯色、方向错乱或接缝异常。

## Feature: Tone Mapping

实现方式：

- shader 中对 HDR lighting 结果做 tone mapping 和 gamma 处理。
- skybox 和 mesh shading 都经过同一类显示映射，避免 IBL 贴图过曝或太暗。

素材要求：

- 高动态范围或预过滤环境贴图最容易看出差异。
- 推荐仍使用 Ox Bridge Morning IBL。

演示方式：

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v2 --preview --scene assets\third_party\s72_examples\materials.s72 --width 1280 --height 720
```

观察点：

- 镜面球和天空盒不应整片纯白或死黑。
- 材质球高光应有层次。

## Feature: PBR Material Parameters

实现方式：

- Scene72 PBR material 读取 albedo、roughness、metalness。
- fragment shader 根据 material kind 分支处理 mirror、lambertian、pbr 等响应。
- roughness 控制 specular lobe 和 IBL prefilter level；metalness 控制 diffuse/specular 混合。

素材要求：

- `materials.s72` 中已有官方 PBR 材质球。
- 若自制 Scene72 场景，需要写 `pbr` material 字段。

演示方式：

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v2 --preview --scene assets\third_party\s72_examples\materials.s72 --width 1280 --height 720
```

观察点：

- mirror 球应主要反射环境。
- roughness 不同的 PBR 球应有不同高光宽度。
- lambertian 球应更偏漫反射。

## Feature: Texture Sampling, Mipmaps, And Anisotropic Filtering

实现方式：

- Vulkan texture 上传后生成 mipmap。
- sampler 使用 linear mip filtering。
- 设备支持时启用 anisotropic filtering。
- shader 对材质贴图使用 `SampleGrad`，让 UV 变化和 POM 后采样更稳定。

素材要求：

- 高频纹理最适合验证。
- 当前项目保存了 ambientCG Rock064：
  - `assets\third_party\ambientcg\Rock064_1K-JPG\Rock064_1K-JPG_Color.jpg`
  - `assets\third_party\ambientcg\Rock064_1K-JPG\Rock064_1K-JPG_NormalGL.jpg`
  - `assets\third_party\ambientcg\Rock064_1K-JPG\Rock064_1K-JPG_Roughness.jpg`
  - `assets\third_party\ambientcg\Rock064_1K-JPG\Rock064_1K-JPG_Displacement.jpg`

演示方式：

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v2 --preview --scene assets\third_party\s72_examples\materials.s72 --width 1280 --height 720
```

观察点：

- 拉远时纹理不应剧烈闪烁。
- 斜视表面细节应比无各向异性过滤更稳定。

## Feature: Normal Mapping

实现方式：

- v2 material descriptor 绑定 normal texture。
- shader 将 normal map 从 tangent space 转到 world space。
- TBN 使用 mesh tangent，不再使用 screen-space derivative 反推 tangent。

素材要求：

- 需要 normal map。
- 当前验证素材使用 `Rock064_1K-JPG_NormalGL.jpg`。
- Scene72 mesh 最好提供 `TANGENT`；没有时会使用 UV fallback tangent。

演示方式：

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v2 --preview --scene assets\third_party\s72_examples\materials.s72 --width 1280 --height 720
```

观察点：

- 岩石球表面应有凹凸光照细节。
- 相机靠近/拉远时，不应再出现纹理层突然散开又聚集的问题。

## Feature: Displacement And Parallax Occlusion Mapping

实现方式：

- v2 material descriptor 绑定 displacement texture。
- fragment shader 执行 POM，按 view direction 迭代修正 UV。
- POM 使用显式 mip LOD，并随 LOD fade，减少远处跳变。
- 高度导数会辅助法线扰动，使岩石表面更有体积感。

素材要求：

- 需要 displacement/height texture。
- 当前验证素材使用 `Rock064_1K-JPG_Displacement.jpg`。

演示方式：

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v2 --preview --scene assets\third_party\s72_examples\materials.s72 --width 1280 --height 720
```

观察点：

- 近距离观察岩石球，纹理应有视角相关的位移感。
- 远距离时 POM 会淡出，减少 aliasing 和层级跳变。

## Feature: Image-Based Lighting

实现方式：

- v2 绑定 diffuse irradiance cubemap。
- v2 绑定 5 档 GGX prefiltered specular cubemap。
- v2 生成并绑定 split-sum BRDF LUT。
- shader 按 normal、reflection 和 roughness 采样 IBL。

素材要求：

- 官方 IBL 文件：
  - `assets\third_party\s72_examples\ox_bridge_morning.lambertian.png`
  - `assets\third_party\s72_examples\ox_bridge_morning.ggx-1.png`
  - `assets\third_party\s72_examples\ox_bridge_morning.ggx-2.png`
  - `assets\third_party\s72_examples\ox_bridge_morning.ggx-3.png`
  - `assets\third_party\s72_examples\ox_bridge_morning.ggx-4.png`
  - `assets\third_party\s72_examples\ox_bridge_morning.ggx-5.png`

演示方式：

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v2 --preview --scene assets\third_party\s72_examples\materials.s72 --width 1280 --height 720
```

观察点：

- mirror 和低 roughness 材质应反射环境。
- roughness 增大时，反射应变得更模糊。

## Feature: MSAA

实现方式：

- Vulkan preview 查询设备支持的 sample count。
- 优先使用 4x MSAA，必要时 fallback 到 2x 或 1x。
- color attachment 使用 multisampled image，并 resolve 到 swapchain。

素材要求：

- 几何边缘明显的模型或材质球。
- `materials.s72` 中球体边缘可用于观察。

演示方式：

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v2 --preview --scene assets\third_party\s72_examples\materials.s72 --width 1280 --height 720
```

观察点：

- 几何边缘锯齿应比无 MSAA 更轻。
- 纹理内部 aliasing 主要靠 mipmap、anisotropic filtering 和 POM fade，不完全由 MSAA 解决。

## Feature: Stable Mesh Tangents

实现方式：

- Scene72 loader 读取 `TANGENT` 的 `R32G32B32A32_SFLOAT`。
- glTF/GLB loader 读取 tangent attribute。
- `GpuPreviewVertex` 上传 tangent.xyzw 到 Vulkan location 8。
- shader 正交化 tangent，并用 tangent.w 决定 bitangent handedness。

素材要求：

- 带 normal/displacement 的材质最容易验证。
- 推荐 `materials.Icosphere.pnTtc.b72` 和 Rock064 texture set。

演示方式：

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v2 --preview --scene assets\third_party\s72_examples\materials.s72 --width 1280 --height 720
```

观察点：

- 岩石材质球在移动相机时应稳定。
- 光滑球本身不依赖 normal/displacement，因此不是这个 feature 的主要验证对象。

## Feature: Camera Roaming

实现方式：

- Vulkan preview 保存当前 camera uniform。
- 按 `R` 进入 roaming camera。
- 每帧根据键盘输入更新 eye/target，再更新 uniform buffer。

素材要求：

- 推荐 `materials.s72`，可以近距离看材质球。

演示方式：

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v2 --preview --scene assets\third_party\s72_examples\materials.s72 --width 1280 --height 720
```

控制：

- `R`：开关漫游。
- `WASD`：水平移动。
- `Q/E`：垂直移动。
- 方向键或 `IJKL`：转向。
- `Shift`：加速。
- `Esc`：退出。

观察点：

- 进入漫游后可以靠近材质球检查 normal/POM/mipmap 稳定性。

## Feature: Headless V2 Validation Render

实现方式：

- `--profile v2 --render` 仍保留软件验证路径，用于输出 BMP 和检查 loader/material 语义。
- 实时预览的主路径是 Vulkan GPU；headless BMP 不是当前 v2 的性能目标。

素材要求：

- 推荐 `materials.s72`。

演示方式：

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v2 --render --scene assets\third_party\s72_examples\materials.s72 --output out\v2-materials.bmp --width 1280 --height 720
```

观察点：

- 用于离线检查大体构图、材质分类和输出路径。
- 视觉细节以 Vulkan realtime preview 为准。

## Feature: glTF/GLB Compatibility Baseline

实现方式：

- v2 preview 可通过通用 mesh path 加载 `.gltf`/`.glb`。
- 当前支持静态 mesh、normal、tangent fallback 或 glTF tangent attribute。
- 当前不是完整 glTF PBR 材质系统：贴图绑定仍以 v2 Scene72/materials demo 为主要验证目标。

素材要求：

- 静态 `.glb` 或 `.gltf`。
- 如果模型带 normal map，最好有 tangent attribute。
- glTF skinning、morph target、animation、完整 metallic-roughness texture workflow 还不是 v2 验证目标。

演示方式：

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v2 --preview --scene path\to\model.glb --width 1280 --height 720
```

观察点：

- 模型应能作为静态 mesh 显示。
- 如果要验证 Renderer72 v2 对齐程度，仍以 `materials.s72` 为准。

