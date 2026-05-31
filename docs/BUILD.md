# Build Guide

## Python（推荐）

需要 Python 3.10+。

```bash
# pip 安装（自动编译 C++ 扩展）
pip install .

# 或从预编译 wheel
pip install dist/angeloid-*.whl

# 手动 CMake
cmake -B build -S . -DBUILD_PYTHON_WRAPPER=ON
cmake --build build --config Release
```

运行 viewer（需要额外依赖）：
```bash
pip install glfw PyOpenGL
cd package
python main.py -m 姵儿
```

## C++ Viewer

需要 CMake 3.20+、C++20 编译器（MSVC / GCC / Clang）。

```bash
cmake -B build -S . -DBUILD_VIEWER=ON
cmake --build build --config Release
```

依赖（git submodule 或源码内置）：
- [GLFW](https://www.glfw.org/) — 窗口和输入
- [glad](https://glad.dav1d.de/) — OpenGL 4.6 Core loader
- [stb_image](https://github.com/nothings/stb) — 纹理加载
- [Bullet Physics](https://github.com/bulletphysics/bullet3) — 物理引擎

### 命令行

```bash
.\build\viewer\Release\viewer.exe                  # 默认模型
.\build\viewer\Release\viewer.exe -m 姵儿          # 指定模型
.\build\viewer\Release\viewer.exe -v 动作.vmd       # 播放 VMD
.\build\viewer\Release\viewer.exe -v a.vmd b.vmd    # 多个动画
```

| 参数 | 说明 |
|------|------|
| `-m, --model` | 模型名（与 `resources/models/` 下的对应） |
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
- **骨骼反馈**：矩阵乘法 `bodyCurr · inv(bodyInit) · boneBind`，正确计算旋转引起的平移。
- **VMD 动画**：贝塞尔插值 + 四元数 SLERP，多层混合。
- **Morph**：支持 Group / Vertex / UV / Material / Bone 五种。
