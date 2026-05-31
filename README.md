# Angeloid Alpha

[English Version](README_EN.md)

> 主人，这是一个用于渲染 PMX 模型的程序。我，伊卡洛斯，会尽我所能为主人服务……这样说的话，主人会开心吗？

Angeloid Alpha 是一个 MMD PMX 模型渲染器，C++20 核心 + Python 绑定。支持 GPU 骨骼蒙皮、VMD 动画、Bullet 物理仿真。

![preview](./docs/assets/preview.gif)

## Python 快速开始

主人可以通过 Python 直接使用我。需要 Python 3.10+。

```bash
# 1. 安装（自动编译 C++ 扩展）
pip install .

# 2. 安装运行时依赖（仅运行 main.py 需要）
pip install glfw PyOpenGL

# 3. 运行
cd package
python main.py -m 姵儿
```

或者从预编译 wheel 安装：
```bash
pip install dist/angeloid-*.whl
```

手动编译（需要 CMake 3.20+）：
```bash
cmake -B build -S . -DBUILD_PYTHON_WRAPPER=ON
cmake --build build --config Release
# 产物 _angeloid.pyd 自动拷贝到 package/angeloid/
```

```python
from angeloid import init, glInit, dispose, Model, Camera

# 初始化
glInit()
init("resources/shaders", "resources/toon", ["blink", "まばたき"])

# 加载模型
model = Model()
model.load("resources/models/姵儿/椛暗式-姵儿ver1.2.pmx")

# 加载 VPD 姿势和 VMD 动画
vpd_id = model.loadVpd("resources/vpd/自然站姿.vpd")
model.applyVpd(vpd_id)
track_id = model.loadVmd("resources/vmd/dance.vmd")
model.playVmd(track_id)

# 游戏循环
cam = Camera()
while running:
    dt = compute_delta_time()
    cam.update(dt, w, a, s, d, e, q)  # WASD 移动
    model.update(dt)
    model.draw(width, height)

# 清理
dispose()
```

## Python API 参考

主人，这是我能提供的所有接口。请不要让我做力不能及的事……虽然我会尽量满足您的要求。

### 模块函数

| 函数 | 说明 |
|------|------|
| `init(shaderDir, toonDir, blinkMorphs=[])` | 初始化渲染器。`shaderDir` 指向 `.vert/.frag` 目录，`toonDir` 指向 toon 纹理目录，`blinkMorphs` 是眨眼 morph 名称列表 |
| `glInit()` | 初始化 OpenGL（需在 `glfw.make_context_current()` 之后调用） |
| `dispose()` | 释放所有 GPU 资源和模型数据 |
| `initArgs()` → `dict` | 返回当前运行配置，包含 `blinkMorphs` 等信息 |

### class `Model`

物理引擎、骨骼蒙皮、动画播放全部封装在里面……主人不需要知道内部有多复杂。

**加载与更新**

| 方法 | 说明 |
|------|------|
| `load(path: str)` | 加载 PMX 模型。路径支持中文……虽然我不明白为什么文件名要用汉字。 |
| `update(dt: float)` | 每帧更新：推进 VMD 动画 → 物理模拟 → GPU 骨骼上传 |
| `draw(width: int, height: int)` | 渲染模型（自动处理 skinned/static/morph 多种 pass） |

**VMD 动画**（返回值是 track ID，-1 表示加载失败）

| 方法 | 说明 |
|------|------|
| `loadVmd(path: str) -> int` | 加载 VMD 文件。返回 track ID。可以同时加载多个，用 track ID 区分。 |
| `playVmd(trackId, onEnd=None)` | 播放。`onEnd` 是可选回调 `(int) -> None`，播完时调用。 |
| `pauseVmd(trackId)` / `stopVmd(trackId)` / `removeVmd(trackId)` | 暂停/停止/移除轨道 |
| `playAllVmd()` / `pauseAllVmd()` / `stopAllVmd()` | 全局控制所有轨道 |
| `isVmdPlaying() -> bool` | 是否有轨道在播放 |
| `vmdCurrentFrame(trackId) -> float` | 当前帧号。`vmdMaxFrame` 也可以问……但那个值不太靠谱，不要依赖它。 |
| `setVmdFrame(trackId, frame)` | 跳转到指定帧 |

**VPD 姿势**

| 方法 | 说明 |
|------|------|
| `loadVpd(path: str) -> int` | 加载 VPD 姿势文件 |
| `applyVpd(vpdId)` / `removeVpd(vpdId)` | 应用/移除姿势 |
| `resetPose()` / `syncVpdPose()` | 重置为绑定姿势 / 重新同步 VPD |
| `vpdApplied() -> bool` | 是否有 VPD 生效 |

**显示控制**

| 方法 | 说明 |
|------|------|
| `showModel(bool)` / `getShowModel() -> bool` | 主模型显隐 |
| `showOutline(bool)` / `getShowOutline() -> bool` | 边缘描边 |
| `showToon(bool)` / `getShowToon() -> bool` | Toon 着色 |
| `setSkinning(bool)` / `isSkinned() -> bool` | GPU 蒙皮开关。关闭后模型会变成 T-pose……那是它的原始姿态。 |
| `showRigidBodies(bool)` | 显示物理碰撞体线框（调试用）。蓝色是 mode-2，橙色是 mode-1……像不像我的发饰？ |

**物理**

| 方法 | 说明 |
|------|------|
| `enablePhysics(bool)` / `physicsEnabled() -> bool` | 物理仿真开关。关闭后裙子就不会飘了……会安静地垂着。 |

**Morph 表情**

| 方法 | 说明 |
|------|------|
| `setMorphWeight(name: str, weight: float)` | 设置 morph 权重（0.0 ~ 1.0） |
| `savedMorphWeight(name) -> float` | 查询上次设置的权重 |
| `clearMorphs()` | 清除所有 morph |
| `setMorphWeights(dict)` | 批量设置（如 `{"笑い": 0.5, "困る": 0.3}`） |
| `setIdleBlink(bool)` | 开启/关闭自动眨眼。我的眼睛……每隔 4 秒眨一次。 |
| `morphCount() -> int` | morph 总数 |
| `interactableMorphs() -> list[int]` | 可交互的 morph 索引列表 |
| `morphName(index) -> str` | 根据索引获取 morph 名称 |

**属性（只读）**

| 属性 | 类型 | 说明 |
|------|------|------|
| `modelName()` | `str` | PMX 模型名称 |
| `modelScale()` | `float` | 模型缩放（`2 / maxDimension`） |
| `modelMatrix()` | `list[float]` | 16 个 float 的列主序 4×4 矩阵 |

### class `Camera`

| 属性/方法 | 说明 |
|------|------|
| `x, y, z` | 相机位置……主人可以自由移动视角。 |
| `rotX, rotY` | 旋转角度（弧度） |
| `speed` | 移动速度，滚轮调节 |
| `update(dt, w, a, s, d, e, q)` | 每帧更新。WASD 移动，EQ 升降。 |
| `onMouseButton(pressed)` / `onCursorPos(x, y)` / `onScroll(yoffset)` | 鼠标输入 |
| `reset()` | 重置到默认位置 |
| `viewMatrix() -> list[float]` | 16 float 列主序 View 矩阵 |
| `projectionMatrix(w, h, fov=45, near=0.1, far=1000) -> list[float]` | 静态方法，列主序 Projection 矩阵 |

## C++ 构建

主人，如果想从源码构建……这些命令可以让程序出现在这个世界上。

```bash
cmake -B build -S .
cmake --build build --config Release
```

需要 CMake 3.20+、C++20 编译器。

依赖（通过源码内置或 git submodule）：
- [GLFW](https://www.glfw.org/) — 窗口和输入。这是一个库……吗？
- [glad](https://glad.dav1d.de/) — OpenGL 4.6 Core loader
- [stb_image](https://github.com/nothings/stb) — 纹理加载
- [Bullet Physics](https://github.com/bulletphysics/bullet3) — 物理引擎。主人的模型会动起来……经过多次修复，现在应该没问题了。

## C++ 命令行

```bash
# 默认模型
.\build\viewer\Release\viewer.exe

# 指定模型
.\build\viewer\Release\viewer.exe -m 姵儿

# 播放 VMD
.\build\viewer\Release\viewer.exe -v 动作.vmd

# 多个动画
.\build\viewer\Release\viewer.exe -v a.vmd b.vmd
```

| 参数 | 说明 |
|------|------|
| `-m, --model` | 模型名（与 resources/models/ 下的对应） |
| `-v, --vmd` | VMD 文件，可接多个 |

## 操作按键

| 按键 | 功能 |
|------|------|
| 鼠标左键 | 旋转视角 |
| W/A/S/D | 移动 |
| E/Q | 升降 |
| 滚轮 | 调整速度 |
| X | 世界坐标轴 |
| B | 刚体线框 |
| Y | 物理开关 |
| H | 模型显隐 |
| O | 描边 |
| T | Toon 着色 |
| Space | VMD 播放/暂停 |
| [ / ] | VMD ±30 帧 |
| P | VPD 姿势 |
| I | 自动眨眼 |
| K | GPU 蒙皮 |
| < / > | 切换表情 |
| ↑ / ↓ | 调整表情强度 |
| R | 重置摄像机 |
| Esc | 退出 |

## 技术细节

- **GPU 蒙皮**：Bone 矩阵打包为 RGBA32F 纹理，Shader 中 `texelFetch` 采样。最多 1024 骨骼。
- **物理引擎**（Bullet 3.27）：PMX 原生空间运行，重力 = `-9.8 / modelScale`。YXZ 旋转顺序。碰撞形状尺寸 1.0x PMX 定义值。
- **关节**：`btGeneric6DofSpringConstraint`。PMX 弹簧原值直接使用；紧锁的平动 DOF 自动补偿（k=10000/2000/500 三档）。
- **骨骼反馈**：矩阵乘法 `bodyCurr · inv(bodyInit) · boneBind`，正确计算旋转引起的平移。Mode-2 刚体保留动画位置、仅回传旋转。
- **VMD 动画**：贝塞尔插值 + 四元数 SLERP，多层混合。
- **Morph**：支持 Group / Vertex / UV / Material / Bone 五种。Material morph index=-1 作用于全部材质。

## 项目结构

```
mmd-demo/
├── mmd/                   # C++ 核心库（无 GPU 依赖）
│   ├── pmx/               # PmxModel, PmxReader
│   ├── anim/              # BoneSkinning, PhysWorld, VmdPlayer, MorphCtl
│   ├── render/opengl/     # ModelRenderer, GPU 封装, 调试可视化
│   └── math/              # Vec2/3/4, Quat
├── viewer/                # C++ 原生应用入口
├── wrapper/               # CPython 绑定（pybind11-free, 纯 C API）
├── package/               # Python 应用 + angeloid 包
├── resources/             # 模型/纹理/shader/VMD/VPD
├── thirdparty/            # GLFW, glad, Bullet, stb
└── docs/                  # 技术文档（架构/物理/格式参考）
```

## 模型版权

`resources/models/` 下模型为第三方资产，各有独立许可。详见各模型目录下 `readme.txt`。

- **姵儿** © 上海鹏拜信息技术有限公司（Playbox） — 模型：椛暗 / 设定：Pre / 原案：王乾龙Ashsteins
- **艾尔莎** © 虚研社 — 建模：悠米露 / 绑定：补骨脂

## 许可证

MIT License

---

> 主人，经过这段时间的修复……物理引擎已经和 saba 验证一致了。裙摆不会再穿模，您可以放心使用。如果您需要其他功能，我随时待命。
