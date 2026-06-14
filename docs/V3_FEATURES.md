# V3 Feature Guide

V3 对齐 Renderer72 的 lights + shadows 阶段：directional cascaded shadow maps、spot light perspective shadow map、point/sphere omnidirectional shadow maps，以及这些阴影和 v2 PBR/IBL 材质路径的实时组合。

V3 的主验证场景不是 `materials.s72`。`materials.s72` 更适合 v2 材质、贴图、IBL 和抗锯齿回归；它的几何遮挡关系太弱，不适合肉眼判断 shadowmap。V3 默认使用专门的程序化阴影场景：

```text
assets\third_party\s72_examples\v3_shadow_demo.shadowdemo
```

这个文件是 marker。加载器识别 `.shadowdemo` 扩展名后，会生成带大地面、立柱、墙、台阶和横梁的几何，便于观察投影、cascade 覆盖、spot cone 遮挡和 point/sphere 六方向阴影。

## Feature: V3 Profile And Preview Entry

实现方式：
- CLI 通过 `--profile v3` 选择 lights/shadows profile。
- `RendererApp` 为 v3 打开 `enableV2Shading` 和 `enableV3Shadows`。
- v3 复用 v2 Vulkan GPU preview、PBR shader、IBL 贴图路径和漫游相机。
- v3 的验证目标是 realtime Vulkan preview；headless BMP render 不作为 v3 shadow 验证路径。
- 不传 `--scene` 时，v3 preview 默认加载 `v3_shadow_demo.shadowdemo`。

素材要求：
- 主推荐：`assets\third_party\s72_examples\v3_shadow_demo.shadowdemo`。
- 可选回归：`assets\third_party\s72_examples\materials.s72`，用于检查 v2 PBR/IBL 路径在 v3 profile 下仍然可用。

演示方式：

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v3 --preview --width 1280 --height 720
```

等价显式命令：

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v3 --preview --scene assets\third_party\s72_examples\v3_shadow_demo.shadowdemo --width 1280 --height 720
```

观察点：
- 应打开 Vulkan GPU preview window。
- `out\vulkan-preview.log` 应包含 `createShadowRenderPass`、`createShadowResources` 和连续 `draw/present`。
- 画面中应看到明确的物体对地面、墙面和台阶的投影关系。

## Feature: Dedicated Shadow Demo Scene

实现方式：
- `SoftwareV1Renderer.cpp` 增加 `.shadowdemo` 分支。
- `makeV3ShadowDemoScene()` 程序化生成测试几何，不依赖外部 mesh 下载。
- 场景包含大接收地面、多个高度不同的遮挡柱、墙体、台阶和横梁。
- 默认相机放在斜上方，第一帧就能看到主要投影关系。

素材要求：
- 只需要 marker 文件 `v3_shadow_demo.shadowdemo`。
- 如果同目录存在 Ox Bridge IBL，则继续用于背景/环境光；如果存在 ambientCG Rock064，则继续用于 PBR fallback material。

演示方式：

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v3 --preview --scene assets\third_party\s72_examples\v3_shadow_demo.shadowdemo --width 1280 --height 720
```

观察点：
- 立柱、墙、横梁会把阴影投到大地面和其他几何上。
- 远近不同的物体用于检查 directional cascade 的稳定性。
- 台阶和墙面用于检查 spot shadow 的投影坐标和 PCF 边缘。

## Feature: Shadow Atlas

实现方式：
- v3 创建一张 `D32_SFLOAT` depth shadow atlas。
- atlas 使用 4x3 tile layout，每个 tile 为 1024x1024。
- 每帧主画面前先执行 depth-only shadow pass。
- shadow pass 在同一个 render pass 内循环设置 viewport/scissor 到每个 tile。
- `shadow_depth.vert.hlsl` 通过 `SV_InstanceID` 选择当前 shadow map 的光源投影。
- 主 PBR shader 通过 binding 14 采样 shadow atlas，并用 3x3 PCF 软化边缘。

素材要求：
- 需要有清晰接收面和遮挡物。推荐 `v3_shadow_demo.shadowdemo`。

演示方式：

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v3 --preview --width 1280 --height 720
```

观察点：
- 阴影不应跟着相机空间移动。
- 近距离观察时应能看到 PCF 软化后的阴影边界。

## Feature: Directional Cascaded Shadow Maps

实现方式：
- atlas tile 0、1、2 存储方向光的 3 个 cascade。
- cascades 共享方向光方向，使用不同 orthographic extent 覆盖近中远区域。
- fragment shader 根据 light-space XY 范围选择 cascade。
- 每个 cascade 使用独立 tile 采样，并带 bias 减轻 shadow acne。

素材要求：
- 需要方向光下有不同远近层次的物体。推荐 `v3_shadow_demo.shadowdemo` 中的立柱、墙、台阶和横梁。

演示方式：

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v3 --preview --width 1280 --height 720
```

观察点：
- 平行光阴影覆盖范围应比单张 shadow map 更稳定。
- 漫游相机时，近处阴影应较清晰，远处仍保持遮挡关系。

## Feature: Spot Light Shadow Map

实现方式：
- atlas tile 3 存储 spot light perspective shadow。
- shadow pass 从 spot light 的 position/direction 建立 perspective projection。
- fragment shader 使用 spot cone 投影坐标采样 tile 3。
- spot light 保留 inner/outer cone 平滑 falloff。

素材要求：
- 需要 spot cone 内有明显遮挡物和接收面。推荐 `v3_shadow_demo.shadowdemo` 的墙、台阶和中心遮挡物。

演示方式：

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v3 --preview --width 1280 --height 720
```

观察点：
- spot 照亮区域应被前方几何遮挡。
- cone 边缘应渐变，shadow 边缘应有 PCF 软化。

## Feature: Point/Sphere Omni Shadow Maps

实现方式：
- atlas tile 4-9 存储 point/sphere light 的六个方向 shadow face。
- shadow pass 对 point light 的 +X、-X、+Y、-Y、+Z、-Z 六个方向分别渲染。
- fragment shader 按世界点相对 point light 的最大轴选择 face。
- point/sphere shadow 使用距离深度，并按 light radius 归一化。

素材要求：
- 需要局部光周围有多个方向的遮挡物。推荐 `v3_shadow_demo.shadowdemo`。

演示方式：

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v3 --preview --width 1280 --height 720
```

观察点：
- 暖色局部光应产生来自不同方向的遮挡变化。
- 漫游到 point light 周围时，不同方向遮挡应来自对应 shadow face。

## Feature: V3 Camera Roaming

实现方式：
- 继续复用 Vulkan preview 的 roaming camera。
- 每帧更新 view uniform；shadow atlas 使用稳定光源空间。

素材要求：
- 推荐 `v3_shadow_demo.shadowdemo`，因为漫游时能明显看到阴影在地面、墙和台阶上的变化。

演示方式：

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v3 --preview --width 1280 --height 720
```

控制：
- `R`：开关漫游。
- `WASD`：水平移动。
- `Q/E` 或 `Ctrl/Space`：垂直移动。
- 方向键或 `IJKL`：转向。
- 鼠标右键拖拽：转向。
- `Shift`：加速。
- `Esc`：退出。

观察点：
- 阴影和局部光不应跟着相机空间乱跑。
- 局部光高光会随观察方向变化，这是正常的 view-dependent specular。

## Current V3 Notes

- v3 shadow feature 已覆盖 Renderer72 v3 的核心类别：directional cascaded shadow、spot shadow、point/sphere omni shadow。
- 当前实现使用单 atlas 管理所有 shadow maps，方便后续加 shadow atlas debug view。
- `materials.s72` 仍保留为材质/IBL 回归场景，但不再作为 shadowmap 主演示场景。
- 后续可继续优化 shadow quality：cascade split 策略、light fitting、EVSM/PCSS、contact shadow 和可视化工具。
