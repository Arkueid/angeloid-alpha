# Python API Reference

## 模块函数

| 函数 | 说明 |
|------|------|
| `init(shaderDir, toonDir, blinkMorphs=[])` | 初始化渲染器。`shaderDir` 指向 `.vert/.frag` 目录，`toonDir` 指向 toon 纹理目录，`blinkMorphs` 是眨眼 morph 名称列表 |
| `glInit()` | 初始化 OpenGL（需在 `glfw.make_context_current()` 之后调用） |
| `dispose()` | 释放所有 GPU 资源和模型数据 |
| `initArgs()` → `dict` | 返回当前运行配置，包含 `blinkMorphs` 等信息 |

## class `Model`

物理引擎、骨骼蒙皮、动画播放全部封装在里面。

### 加载与更新

| 方法 | 说明 |
|------|------|
| `load(path: str)` | 加载 PMX 模型 |
| `update(dt: float)` | 每帧更新：推进 VMD 动画 → 物理模拟 → GPU 骨骼上传 |
| `draw(width: int, height: int)` | 渲染模型（自动处理 skinned/static/morph 多种 pass） |

### VMD 动画

返回值是 track ID，-1 表示加载失败。

| 方法 | 说明 |
|------|------|
| `loadVmd(path: str) -> int` | 加载 VMD 文件。可同时加载多个，用 track ID 区分 |
| `playVmd(trackId, onEnd=None)` | 播放。`onEnd` 是可选回调 `(int) -> None` |
| `pauseVmd(trackId)` / `stopVmd(trackId)` / `removeVmd(trackId)` | 暂停/停止/移除轨道 |
| `playAllVmd()` / `pauseAllVmd()` / `stopAllVmd()` | 全局控制所有轨道 |
| `isVmdPlaying() -> bool` | 是否有轨道在播放 |
| `isVmdPlayingTrack(trackId) -> bool` | 指定轨道是否在播放 |
| `vmdCurrentFrame(trackId) -> float` | 当前帧号 |
| `vmdMaxFrame(trackId) -> float` | 最大帧号（可能不精确） |
| `setVmdFrame(trackId, frame)` | 跳转到指定帧 |
| `vmdTrackCount() -> int` | 已加载的 VMD 轨道数 |

### VPD 姿势

| 方法 | 说明 |
|------|------|
| `loadVpd(path: str) -> int` | 加载 VPD 姿势文件 |
| `applyVpd(vpdId)` / `removeVpd(vpdId)` | 应用/移除姿势 |
| `resetPose()` / `syncVpdPose()` | 重置为绑定姿势 / 重新同步 VPD |
| `vpdApplied() -> bool` | 是否有 VPD 生效 |

### 显示控制

| 方法 | 说明 |
|------|------|
| `showModel(bool)` / `getShowModel() -> bool` | 主模型显隐 |
| `showOutline(bool)` / `getShowOutline() -> bool` | 边缘描边 |
| `showToon(bool)` / `getShowToon() -> bool` | Toon 着色 |
| `setSkinning(bool)` / `isSkinned() -> bool` | GPU 蒙皮开关。关闭后模型会变成 T-pose |
| `showRigidBodies(bool)` | 显示物理碰撞体线框（调试用） |

### 物理

| 方法 | 说明 |
|------|------|
| `enablePhysics(bool)` / `physicsEnabled() -> bool` | 物理仿真开关 |

### Morph 表情

| 方法 | 说明 |
|------|------|
| `setMorphWeight(name: str, weight: float)` | 设置 morph 权重（0.0 ~ 1.0） |
| `savedMorphWeight(name) -> float` | 查询上次设置的权重 |
| `clearMorphs()` | 清除所有 morph |
| `setMorphWeights(dict)` | 批量设置（如 `{"笑い": 0.5, "困る": 0.3}`） |
| `setIdleBlink(bool)` | 开启/关闭自动眨眼（每 4 秒一次） |
| `morphCount() -> int` | morph 总数 |
| `interactableMorphs() -> list[int]` | 可交互的 morph 索引列表 |
| `morphName(index) -> str` | 根据索引获取 morph 名称 |

### 只读属性

| 属性 | 类型 | 说明 |
|------|------|------|
| `modelName()` | `str` | PMX 模型名称 |
| `modelScale()` | `float` | 模型缩放（`2 / maxDimension`） |
| `modelMatrix()` | `list[float]` | 16 个 float 的列主序 4×4 矩阵 |

## class `Camera`

| 属性/方法 | 说明 |
|------|------|
| `x, y, z` | 相机位置 |
| `rotX, rotY` | 旋转角度（弧度） |
| `speed` | 移动速度，滚轮调节 |
| `update(dt, w, a, s, d, e, q)` | 每帧更新。WASD 移动，EQ 升降 |
| `onMouseButton(pressed)` / `onCursorPos(x, y)` / `onScroll(yoffset)` | 鼠标输入 |
| `reset()` | 重置到默认位置 |
| `viewMatrix() -> list[float]` | 16 float 列主序 View 矩阵 |
| `projectionMatrix(w, h, fov=45, near=0.1, far=1000) -> list[float]` | 静态方法，列主序 Projection 矩阵 |
