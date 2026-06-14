# V1 Feature Guide

V1 对齐的是 Renderer72 第一阶段：把场景加载、层级变换、基础动画、可见性裁剪和简单 forward 绘制跑通。这个版本的重点不是 PBR，而是验证“场景数据能正确变成画面”。

## Feature: Profile And Render Graph

实现方式：

- CLI 通过 `--profile v1` 选择 `v1` profile。
- `RendererApp` 构建 v1 render graph，并按 frame 输出 pass 执行信息。
- 实际图像输出由 `renderSoftwareV1` 或 `buildGpuPreviewGeometry` 承接。

素材要求：

- 不要求复杂材质。
- 可以使用项目内置 `.scene`，也可以使用官方 Scene72 `.s72` 示例。

演示方式：

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v1 --render --scene assets\scenes\v1.scene --output out\v1.bmp --width 1280 --height 720 --frames 16
```

观察点：

- 命令行会打印 v1 graph pass。
- `out\v1.bmp` 应该出现简单立方体场景。

## Feature: Built-In Text Scene

实现方式：

- `assets\scenes\v1.scene` 是一个轻量测试格式。
- loader 读取 cube 行，生成简单立方体实例。
- CPU path 会做基础动画、裁剪、z-buffer 和 diffuse/specular 风格的简单着色。

素材要求：

- `.scene` 每行描述一个 cube。
- 适合做 smoke test，不适合验证 Scene72 loader 或材质系统。

演示方式：

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v1 --preview --scene assets\scenes\v1.scene --width 960 --height 540
```

观察点：

- `.scene` 预览会走 legacy CPU preview window。
- 场景中有一个故意放到视锥外的对象，用来验证 culling 统计。

## Feature: Scene72 Mesh Loading

实现方式：

- 支持官方 `.s72` JSON 场景和 `.b72` 二进制 buffer。
- 当前 v1 loader 读取 `SCENE`、`NODE`、`MESH`、`CAMERA`。
- mesh 支持非索引 `TRIANGLE_LIST`。
- 顶点属性支持 `POSITION` 和 `COLOR`，后来也复用了 `NORMAL`、`TEXCOORD` 等路径供后续版本使用。

素材要求：

- `.s72` 文件和它引用的 `.b72` 文件必须在同一素材树下。
- v1 基础验证推荐：
  - `assets\third_party\s72_examples\rotation.s72`
  - `assets\third_party\s72_examples\sg-Articulation.s72`

演示方式：

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v1 --render --scene assets\third_party\s72_examples\rotation.s72 --output out\official-rotation.bmp --width 960 --height 960
build\nmake-debug\src\vulkan_render.exe --profile v1 --preview --scene assets\third_party\s72_examples\rotation.s72 --width 720 --height 720
```

观察点：

- `rotation.s72` 用来检查基础节点、mesh 和颜色是否正确。
- Vulkan preview 会打开实时窗口，mesh 几何由 CPU loader 上传到 GPU 顶点 buffer。

## Feature: Scene Graph Transform

实现方式：

- 每个 Scene72 `NODE` 会组合 translation、rotation、scale。
- 子节点递归继承父节点 transform。
- mesh 顶点在加载阶段变换到 world space。

素材要求：

- 需要带层级节点的 `.s72`。
- 推荐 `sg-Articulation.s72`，它包含机械臂层级结构。

演示方式：

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v1 --render --scene assets\third_party\s72_examples\sg-Articulation.s72 --output out\official-articulation.bmp --width 1280 --height 720
```

观察点：

- 机械臂各部件应保持正确父子关系。
- 如果父节点 transform 出错，机械臂会散架或位置明显不对。

## Feature: Driver Animation

实现方式：

- 读取 Scene72 `DRIVER`。
- 支持 `translation`、`scale`、`rotation` channel。
- 支持 `STEP`、`LINEAR` 和 `SLERP` 插值。
- v1 `.s72` realtime preview 每帧重建/上传几何，因此机械臂动画可以在窗口中播放。

素材要求：

- 需要包含 `DRIVER` 的 Scene72 场景。
- 推荐素材：`assets\third_party\s72_examples\sg-Articulation.s72`。

演示方式：

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v1 --preview --scene assets\third_party\s72_examples\sg-Articulation.s72 --width 1280 --height 720
```

单帧对比：

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v1 --render --scene assets\third_party\s72_examples\sg-Articulation.s72 --frames 1 --output out\articulation-frame-001.bmp --width 640 --height 360
build\nmake-debug\src\vulkan_render.exe --profile v1 --render --scene assets\third_party\s72_examples\sg-Articulation.s72 --frames 24 --output out\articulation-frame-024.bmp --width 640 --height 360
```

观察点：

- 实时窗口中机械臂应循环运动。
- 两张 headless 输出应是不同姿态。

## Feature: Camera And Roaming Preview

实现方式：

- Scene72 camera 会转成内部 `V1CameraSettings`。
- `.scene` CPU preview 和 `.s72` Vulkan GPU preview 都支持 roaming camera。
- 按 `R` 开关漫游；`WASD` 移动，`Q/E` 垂直移动，方向键或 `IJKL` 观察，`Shift` 加速，`Esc` 退出。

素材要求：

- 所有 v1 可加载场景都能用于相机漫游。
- 推荐用 `sg-Articulation.s72`，因为空间结构更容易观察。

演示方式：

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v1 --preview --scene assets\third_party\s72_examples\sg-Articulation.s72 --width 1280 --height 720
```

观察点：

- 默认使用场景相机或自动相机。
- 按 `R` 后窗口标题会进入 roaming camera 模式。

## Feature: Static glTF/GLB Smoke Loading

实现方式：

- 使用 `cgltf` 读取 `.gltf` 和 `.glb`。
- 当前 v1 路径支持静态 triangle mesh、position、normal、vertex color 或 material base color factor。
- v1 不做 glTF skinning/animation。

素材要求：

- 普通静态 `.glb` 或 `.gltf`。
- 推荐先用无骨骼、无复杂材质依赖的模型做 smoke test。

演示方式：

```powershell
build\nmake-debug\src\vulkan_render.exe --profile v1 --render --scene path\to\model.glb --output out\model.bmp --width 1280 --height 720
build\nmake-debug\src\vulkan_render.exe --profile v1 --preview --scene path\to\model.glb --width 1280 --height 720
```

观察点：

- 模型应能显示基本轮廓和颜色。
- 如果模型依赖 glTF 动画、骨骼、复杂 PBR 贴图，v1 不作为验证目标。

