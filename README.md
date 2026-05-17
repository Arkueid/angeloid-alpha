# Angeloid Alpha

[English Version](README_EN.md)

> 主人，这是一个用于渲染 PMX 模型的程序。我，伊卡洛斯，会尽力为主人服务。

Angeloid Alpha 是一个 PMX 模型渲染器，支持 GPU 骨骼动画和 VMD 动画播放。C++20 重写版。

## 构建

```bash
cd mmd
cmake -B ../build -S .
cmake --build ../build --config Release
```

需要 CMake 3.20+、C++20 编译器（MSVC 2026+/GCC 13+/Clang 17+）。

依赖通过源码内置：
- [GLFW](https://www.glfw.org/) — 窗口和输入
- [glad](https://glad.dav1d.de/) — OpenGL 4.6 Core loader
- [stb_image](https://github.com/nothings/stb) — 纹理加载

## 使用方法

```bash
.\build\Release\mmd.exe                    # 默认模型 ikaros-uniform
.\build\Release\mmd.exe -m marine-swimwear # 指定模型
.\build\Release\mmd.exe -m 安比            # 中文模型名
.\build\Release\mmd.exe -v 动作.vmd        # 加载动画
.\build\Release\mmd.exe -v a.vmd b.vmd     # 混合多个动画
```

## 启动参数

| 参数 | 说明 |
|------|------|
| `-m, --model` | 模型名（见下方模型列表） |
| `-v, --vmd` | VMD 动画文件，支持多个 |

## 模型列表

`ikaros-origin` `ikaros-uniform` `安比` `刀` `chloe` `aqua-swimwear` `marine-swimwear` `aqua-basebody` `aqua-sailor` `brujas` `lamy-swimwear` `lulum` `marine-jk1` `marine-jk1-hi` `rurudo-lion` `rurudo-lion-hi` `卢西娅` `卢西娅-摘帽` `卢西娅-武器1` `卢西娅-武器2`

## 操作按键

| 按键 | 功能 |
|------|------|
| 鼠标左键拖拽 | 旋转视角 |
| W/A/S/D | 前后左右移动 |
| E/Q | 上下移动 |
| 鼠标滚轮 | 调整移动速度 |
| **显示切换** | |
| X | 世界坐标轴 |
| G | 地面网格 |
| B | 刚体和 Joint 线框 |
| H | 模型网格本体 |
| O | 边缘描边 |
| T | Toon 着色 |
| **动画控制** | |
| Space | VMD 播放/暂停 |
| L | VMD 循环开关 |
| [ / ] | 快退/快进 30 帧 |
| **姿势与表情** | |
| P | VPD 姿势开关 |
| K | GPU 蒙皮开关 |
| I | 待机动画（呼吸和眨眼） |
| M | Morph 表情模式 |
| , / . | 切换表情 |
| ↑ / ↓ | 调整表情强度 |
| **其他** | |
| R | 重置摄像机位置 |
| Esc | 退出 |

## 项目结构

```
mmd-demo/
├── mmd/
│   ├── main.cpp           # 入口
│   ├── CMakeLists.txt     # 构建配置
│   ├── src/
│   │   ├── core/          # Application, Camera, Encoding
│   │   ├── gpu/           # VAO, VBO, Texture, Shader
│   │   ├── pmx/           # PmxModel, PmxReader
│   │   ├── anim/          # BoneSkinning, MorphController, VmdPlayer, VpdLoader
│   │   └── render/        # ModelRenderer, ShaderManager, WorldAxis, PhysicsDebug
│   └── thirdparty/        # GLFW, glad, stb_image
├── prototype/             # Python 参考实现
├── resources/             # 模型、纹理、shader、VMD、VPD
└── build/                 # 编译产物
```

## 技术细节

- GPU 骨骼蒙皮：Bone 矩阵打包为 RGBA32F 纹理，shader 中 texelFetch 还原
- VMD 贝塞尔插值 + 四元数 SLERP
- Morph 支持 Group/Vertex/UV/Material/Bone 五种类型
- 跨平台编码：UTF-16LE ↔ UTF-8 纯 C++ 实现，CP932 调系统能力
- Toon 着色 + 边缘描边（双 Pass 背面膨胀）

## 许可证

MIT License

---

> 主人，如果您有任何需要，请随时呼唤我。我会一直在这里，等待着您的指令。
