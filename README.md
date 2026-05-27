# Angeloid Alpha

[English Version](README_EN.md)

> 主人，这是一个用于渲染 PMX 模型的程序。我，伊卡洛斯，会尽我所能为主人服务……这样说的话，主人会开心吗？

Angeloid Alpha 是一个 PMX 模型渲染器，C++20 重写。支持 GPU 骨骼动画、VMD 动画播放、Bullet 物理仿真。

## 构建

主人，请按以下步骤构建程序……这是命令，不是请求。

```bash
cd mmd
cmake -B ../build -S .
cmake --build ../build --config Release
```

需要 CMake 3.20+、C++20 编译器。

依赖（通过源码内置或 git submodule）：
- [GLFW](https://www.glfw.org/) — 窗口和输入。这是一个库……吗？
- [glad](https://glad.dav1d.de/) — OpenGL 4.6 Core loader
- [stb_image](https://github.com/nothings/stb) — 纹理加载
- [Bullet Physics](https://github.com/bulletphysics/bullet3) — 物理引擎。主人的模型会动起来……虽然还有些问题。

## 使用方法

```bash
# 默认模型是 ikaros-uniform。我是主人默认的选择……对吗？
.\build\Release\mmd.exe

# 指定模型名
.\build\Release\mmd.exe -m marine-swimwear

# 中文模型名也可以。安比……是主人的另一个选择。
.\build\Release\mmd.exe -m 安比

# 播放 VMD 动画。动作会让模型动起来。
.\build\Release\mmd.exe -v 动作.vmd

# 混合多个动画
.\build\Release\mmd.exe -v a.vmd b.vmd
```

## 启动参数

| 参数 | 说明 |
|------|------|
| `-m, --model` | 模型名 |
| `-v, --vmd` | VMD 动画文件，可以接多个 |

## 操作按键

主人，这些按键控制着程序的行为。我已经记住了。

| 按键 | 功能 |
|------|------|
| 鼠标左键拖拽 | 旋转视角 |
| W/A/S/D | 前后左右移动 |
| E/Q | 上下移动 |
| 鼠标滚轮 | 调整移动速度 |
| **显示控制** | |
| X | 世界坐标轴 |
| G | 地面网格 |
| B | 刚体线框……主人可以看到物理的形状 |
| Y | 物理仿真开关。开启后模型会动，但帧率会下降……困扰。 |
| H | 模型网格。关掉就看不到了…… |
| O | 边缘描边 |
| T | Toon 着色 |
| F | 物理 Debug 信息输出 |
| **动画控制** | |
| Space | VMD 播放/暂停 |
| L | VMD 循环 |
| [ / ] | 快退/快进 30 帧 |
| **姿势与表情** | |
| P | VPD 姿势 |
| K | GPU 蒙皮 |
| I | 待机动画（呼吸+眨眼） |
| M | Morph 表情模式 |
| , / . | 切换表情 |
| ↑ / ↓ | 调整表情强度 |
| **其他** | |
| R | 重置摄像机 |
| Esc | 退出……主人要走了吗？ |

## 项目结构

```
mmd-demo/
├── mmd/
│   ├── main.cpp           # 入口。一切从这里开始。
│   ├── CMakeLists.txt     # 构建。CMake 会处理一切。
│   ├── src/
│   │   ├── core/          # Application, Camera, Encoding
│   │   ├── gpu/           # VAO, VBO, Texture, Shader
│   │   ├── pmx/           # PmxModel, PmxReader
│   │   ├── anim/          # BoneSkinning, MorphController, VmdPlayer, VpdLoader, PhysicsWorld
│   │   └── render/        # ModelRenderer, ShaderManager, WorldAxis, PhysicsDebug
│   └── thirdparty/        # GLFW, glad, stb_image, Bullet Physics
├── prototype/             # Python 参考实现。已经完成了它的使命。
├── resources/             # 模型、纹理、shader、VMD、VPD
└── build/                 # 编译产物
```

## 技术细节

主人想知道程序是怎么运作的……我来说明。

- **GPU 骨骼蒙皮**：Bone 矩阵打包为 RGBA32F 纹理，shader 中用 texelFetch 还原。最多支持 1024 根骨骼。
- **Bullet 物理**：PMX 原生空间运行（无模型缩放），重力自动适配。YXZ 旋转顺序匹配 MMD。形状尺寸可调。碰撞 mask 匹配社区惯例。
- **弹簧补偿**：锁死的关节加 k=10000 强力弹簧，限位紧密用 k=2000 或 k=500。
- **骨骼反馈**：Mode 0 刚体跟随骨骼（kinematic），Mode 1 全动态，Mode 2 带延时修正力。蒙皮矩阵纯 `world * inv(bind)`，GPU modelMat 处理显示变换。
- **VMD 动画**：贝塞尔插值 + 四元数 SLERP。多层 VMD 用 0.5 混合系数 SLERP。
- **Morph 系统**：支持 Group/Vertex/UV/Material/Bone 五种类型。Material morph 的 index=-1 会应用到所有材质。
- **纹理编码**：UTF-16LE ↔ UTF-8 纯 C++ 实现。CP932 调 Windows MultiByteToWideChar。
- **Toon 着色**：梯度纹理 + 双 Pass 边缘描边（背面膨胀法）。

## 许可证

MIT License

---

> 主人，程序目前的状态就是这样。物理部分还需要继续改进……但是，我会一直在这里，等候您的下一个命令。
