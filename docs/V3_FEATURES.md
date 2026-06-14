# V3 Feature Guide

V3 的目标是把 v2 的 PBR/IBL 材质场景推进到 lights + shadows：方向光阴影、局部光源、spot light，以及后续的 sphere/point omni shadow 和 directional cascades。当前实现先落地可实时验证的 GPU shadow/light 核心路径。

## Feature: V3 Profile And Preview Entry

实现方式：

- CLI 通过 `--profile v3` 选择 lights/shadows profile。
- v3 复用 v2 的 Scene72 material loader 和 Vulkan GPU preview。
- `RendererApp` 会为 v3 打开 `enableV2Shading` 和 `enableV3Shadows`，默认场景仍是官方 `materials.s72`。

素材要求：

- 推荐使用 v2 官方材质场景：
  - `assets\third_party\s72_examples\materials.s72`
- 需要同目录的 `.b72`、材质贴图和 Ox Bridge IBL。

演示方式：

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v3 --preview --scene assets\third_party\s72_examples\materials.s72 --width 1280 --height 720
```

观察点：

- 应打开 Vulkan GPU preview window。
- `out\vulkan-preview.log` 应包含 `createShadowRenderPass`、`createShadowResources` 和连续 `draw/present`。

## Feature: Directional Shadow Map

实现方式：

- v3 创建一个 2048x2048 `D32_SFLOAT` depth texture。
- 每帧主画面前先执行 depth-only shadow pass。
- 新增 `shadow_depth.vert.hlsl`，从方向光视角把场景写入 shadow map。
- 主 PBR shader 通过 binding 14 采样 directional shadow map，并用 3x3 PCF 做软化。
- shader 中带 slope-style bias，减轻 shadow acne。

素材要求：

- 需要有可互相遮挡的 mesh。
- `materials.s72` 可用于检查材质球对地面和附近物体的投影。

演示方式：

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v3 --preview --scene assets\third_party\s72_examples\materials.s72 --width 1280 --height 720
```

观察点：

- 材质球和场景物体应出现方向一致的阴影衰减。
- 按 `R` 开启漫游，靠近地面和球体边缘检查 shadow acne / peter-panning。

## Feature: Point/Sphere Local Light

实现方式：

- v3 shader 中新增一个 point/sphere-style local light。
- uniform 传入 position、radius、color 和 intensity。
- 光照使用距离衰减、法线点积和 roughness-aware specular。

素材要求：

- 任意 v3 mesh 场景均可。
- 材质球场景更容易观察局部暖色光影响。

演示方式：

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v3 --preview --scene assets\third_party\s72_examples\materials.s72 --width 1280 --height 720
```

观察点：

- 场景一侧会有暖色局部光贡献。
- roughness 不同的球体对局部高光的响应不同。

当前限制：

- 这一版 point/sphere light 还没有 cubemap omni shadow map。
- Renderer72 v3 对齐项中的 `shadow.sphere-omni` 仍是后续补齐目标。

## Feature: Spot Light

实现方式：

- v3 shader 中新增一个 spot light。
- uniform 传入 position、direction、inner cone、outer cone、color 和 intensity。
- cone falloff 使用 inner/outer cosine 做平滑过渡。

素材要求：

- 任意 v3 mesh 场景均可。
- `materials.s72` 适合观察冷色 spot light 在 PBR 球上的高光。

演示方式：

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v3 --preview --scene assets\third_party\s72_examples\materials.s72 --width 1280 --height 720
```

观察点：

- 场景另一侧会有偏冷色的局部 spot 光。
- spot cone 边缘应是渐变而不是硬切。

当前限制：

- 这一版 spot light 先完成局部光照，还没有单独 spot shadow map。

## Feature: V3 Camera Roaming

实现方式：

- 继续复用 Vulkan preview 的 roaming camera。
- 每帧更新 view uniform，shadow uniform 保持稳定光源空间。

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

- 阴影不应跟着相机空间乱跑。
- 局部光高光会随观察方向变化。

## V3 Remaining Alignment Work

当前 v3 已经有真实 GPU directional shadow map、局部 point/sphere light、spot light 和实时预览。为了完全对齐 Renderer72 v3 的典型 shadow feature，还需要继续补：

- Spot light shadow map：为 spot light 增加独立 depth map 和投影采样。
- Sphere/point omni shadow：为 point/sphere light 增加 cubemap depth shadow。
- Directional cascades：把当前单张方向光 shadow map 扩展为多 cascade，并增加 split selection/可视化。

