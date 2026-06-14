# V3 Feature Guide

V3 对齐 Renderer72 的 lights + shadows 阶段：spot light perspective shadow、point/sphere omni shadow、directional light cascaded shadow，以及这些光源和 v2 PBR/IBL 材质的实时组合。当前 v3 使用一个 Vulkan depth shadow atlas 统一管理全部 shadow maps。

## Feature: V3 Profile And Preview Entry

实现方式：

- CLI 通过 `--profile v3` 选择 lights/shadows profile。
- v3 复用 v2 的 Scene72 material loader、PBR/IBL shader 和 Vulkan GPU preview。
- `RendererApp` 为 v3 打开 `enableV2Shading` 和 `enableV3Shadows`。
- v3 的验证路径是 realtime Vulkan preview；headless BMP render 不作为 v3 shadow 验证目标。

素材要求：

- 推荐使用官方材质场景：
  - `assets\third_party\s72_examples\materials.s72`
- 需要同目录的 `.b72`、材质贴图、Rock064 PBR texture set 和 Ox Bridge IBL。

演示方式：

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v3 --preview --scene assets\third_party\s72_examples\materials.s72 --width 1280 --height 720
```

观察点：

- 应打开 Vulkan GPU preview window。
- `out\vulkan-preview.log` 应包含 `createShadowRenderPass`、`createShadowResources` 和连续 `draw/present`。

## Feature: Shadow Atlas

实现方式：

- v3 创建一张 `D32_SFLOAT` depth shadow atlas。
- atlas 使用 4x3 tile layout，每个 tile 是 1024x1024。
- 每帧主画面前先执行 depth-only shadow pass。
- shadow pass 在同一个 render pass 内循环设置 viewport/scissor 到每个 tile。
- `shadow_depth.vert.hlsl` 通过 `SV_InstanceID` 选择当前 shadow map 的光源投影。
- 主 PBR shader 通过 binding 14 采样 shadow atlas，并用 3x3 PCF 做软化。

素材要求：

- 需要有可互相遮挡的 mesh。
- `materials.s72` 可用于检查材质球、地面和局部光之间的遮挡。

演示方式：

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v3 --preview --scene assets\third_party\s72_examples\materials.s72 --width 1280 --height 720
```

观察点：

- 阴影不应跟着相机空间移动。
- 近距离检查应能看到 PCF 软化后的阴影边界。

## Feature: Directional Cascaded Shadow Maps

实现方式：

- atlas tile 0、1、2 存储方向光的 3 个 cascade。
- cascades 共享方向光方向和中心，使用不同 ortho extent。
- fragment shader 根据 light-space XY 范围选择 cascade。
- 每个 cascade 使用独立 tile 采样，并带 slope-style bias 减轻 shadow acne。

素材要求：

- 需要方向光下有不同远近层次的物体。
- `materials.s72` 中的球体、地面和标签 mesh 可用于观察。

演示方式：

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v3 --preview --scene assets\third_party\s72_examples\materials.s72 --width 1280 --height 720
```

观察点：

- 平行光阴影覆盖范围应比单张 shadow map 更稳定。
- 漫游相机时，近处阴影应保持较清晰，远处阴影应继续有遮挡关系。

## Feature: Spot Light Shadow Map

实现方式：

- atlas tile 3 存储 spot light perspective shadow。
- shadow pass 从 spot light 的 position/direction 建立 perspective projection。
- fragment shader 使用 spot cone 投影坐标采样 tile 3。
- spot light 仍保留 inner/outer cone 平滑 falloff。

素材要求：

- 任意 v3 mesh 场景均可。
- `materials.s72` 适合观察冷色 spot light 在 PBR 球上的高光和遮挡。

演示方式：

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v3 --preview --scene assets\third_party\s72_examples\materials.s72 --width 1280 --height 720
```

观察点：

- spot 光照区应该会被前方几何遮挡。
- cone 边缘应是渐变，shadow 边缘有 PCF 软化。

## Feature: Point/Sphere Omni Shadow Maps

实现方式：

- atlas tile 4-9 存储 point/sphere light 的六个方向 shadow face。
- shadow pass 对 point light 的 +X、-X、+Y、-Y、+Z、-Z 六个方向分别渲染。
- fragment shader 按世界点相对 point light 的最大轴选择 face。
- point/sphere shadow 使用距离深度，并按 light radius 归一化。

素材要求：

- 任意 v3 mesh 场景均可。
- 材质球场景更容易观察暖色 point/sphere light 被附近物体遮挡后的变化。

演示方式：

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v3 --preview --scene assets\third_party\s72_examples\materials.s72 --width 1280 --height 720
```

观察点：

- 暖色局部光应有六方向遮挡效果。
- 漫游到 point light 周围时，不同方向的遮挡应来自对应 shadow face。

## Feature: V3 Camera Roaming

实现方式：

- 继续复用 Vulkan preview 的 roaming camera。
- 每帧更新 view uniform；shadow atlas 使用稳定光源空间。

素材要求：

- 推荐 `materials.s72`。

演示方式：

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v3 --preview --scene assets\third_party\s72_examples\materials.s72 --width 1280 --height 720
```

控制：

- `R`：开关漫游。
- `WASD`：水平移动。
- `Q/E`：垂直移动。
- 方向键或 `IJKL`：转向。
- `Shift`：加速。
- `Esc`：退出。

观察点：

- 阴影和局部光不应跟着相机空间乱跑。
- 局部光高光会随观察方向变化，这是正常的 view-dependent specular。

## Current V3 Notes

- v3 shadow feature 已覆盖 Renderer72 v3 的核心类别：spot shadow、point/sphere omni shadow、directional cascaded shadow。
- 当前实现使用单 atlas 管理所有 shadow maps，方便后续加 shadow atlas debug view。
- 后续可继续优化的是 shadow quality：cascade split 策略、light fitting、EVSM/PCSS、contact shadow 和可视化工具。

