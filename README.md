# Angeloid Alpha

[English Version](README_EN.md)

> 主人，这是一个用于渲染 PMX 模型的程序。我，伊卡洛斯，会尽力为主人服务。

Angeloid Alpha 是一个 PMX 模型渲染器，支持 GPU 骨骼动画和 VMD 动画播放。

## 这是什么？

主人可以用它来加载 MMD 格式的 3D 模型，进行实时渲染和自由探索。动画、表情、物理碰撞体……我都能为主人展示出来。

## 我能做什么

| 功能 | 说明 |
|------|------|
| PMX 加载 | 读取主人的模型文件 |
| GPU 蒙皮 | 用纹理驱动骨骼动画，在 GPU 上高效计算 |
| VPD 姿势 | 加载主人指定的姿势文件 |
| VMD 动画 | 播放舞蹈和动作，支持多轨混合 |
| Morph 表情 | 眨眼、微笑……如果我也能做出表情就好了 |
| Toon 着色 | 卡通风格的渲染效果 |
| 边缘描边 | 清晰的轮廓线，让模型更立体 |
| 物理可视化 | 用线框显示刚体和 Joint，方便主人检查 |
| FPS 显示 | 在标题栏显示帧率，让主人知道运行情况 |

## 项目结构

```
mmd-demo/
├── main.py                   # 主人运行这个文件就可以了
├── src/
│   ├── gpu/                  # GPU 相关的功能放在这里
│   │   ├── mesh.py           # VAO 和 VBO 的封装
│   │   └── texture.py        # 纹理管理
│   ├── pmx_model.py          # PMX 的读取和数据结构
│   ├── vpd_loader.py         # VPD 姿势文件读取
│   ├── vmd_player.py         # VMD 动画的读取和混合播放
│   ├── bone_math.py          # 骨骼矩阵计算和物理网格生成
│   ├── renderer.py           # 我的核心……渲染都在这里
│   ├── animation_controller.py
│   ├── morph_controller.py
│   ├── shader_manager.py
│   └── camera.py             # 主人的视角控制
└── resources/
    ├── shaders/              # GLSL 着色器
    ├── models/               # PMX 模型们
    ├── toon/                 # 默认 Toon 纹理
    ├── vpd/                  # 姿势文件
    └── motions/              # 动作文件
```

已经整理得很清楚了，主人。

## 使用方法

```bash
python main.py                          # 加载默认模型
python main.py -m marine-swimwear       # 指定模型
python main.py -v motions/xxx.vmd       # 加载动画
python main.py -v a.vmd b.vmd           # 混合多个动画
```

### 启动参数

| 参数 | 说明 |
|------|------|
| `-m, --model` | 选择要加载的模型 |
| `-v, --vmd` | VMD 动画文件，支持同时加载多个 |

### 操作按键

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
| Esc | 释放鼠标光标 |

## 技术细节

### GPU 骨骼蒙皮

我把每帧的骨骼矩阵打包进一张 RGBA32F 纹理——每个矩阵占 4 个纹素，每个纹素存矩阵的一列。Shader 里用 `texelFetch` 按索引取出来还原成 mat4。

这样做的好处是：每帧只需要更新一张纹理，不用逐顶点上传数据。数据量小，效率很高，主人。

### 骨骼层级变换

PMX 的骨骼位置存储的是世界坐标，不是相对父骨骼的局部坐标。所以我的处理流程是：

1. 用子骨骼位置减去父骨骼位置，得到局部偏移
2. 结合 VMD 动画中的旋转，构建局部变换矩阵
3. 从根骨骼开始，沿层级向下累积世界变换
4. 世界变换矩阵 × 逆绑定矩阵，得到最终的蒙皮矩阵

这些逆绑定矩阵只在加载时算一次，动画时复用缓存，不会每帧重复求逆。

### Toon 着色

用 `N·L`（法线与光照方向的点积）去采样一张渐变纹理。阈值以上是亮面，以下是暗面。还加了一点边缘光。

### 边缘描边

两个 Pass：先正常渲染模型写入深度，再把顶点沿法线方向推出去一点，只画背面，用纯色填充。这样不管怎么看，描边都不会闪烁。

### 刚体和 Joint 可视化

主人按 B 键就能看到。它们和模型共用同一张骨骼纹理，所以动画播放时也会跟着动。刚体按类型画成 Box、Sphere 或 Capsule 的线框，Joint 按连接对用不同颜色区分。

### VMD 动画

支持贝塞尔曲线插值、四元数 SLERP 旋转插值，以及多个 VMD 同时混合播放。

## 依赖

- [PyOpenGL](https://pyopengl.sourceforge.net/) — OpenGL 接口
- [glfw](https://www.glfw.org/) — 窗口管理
- [numpy](https://numpy.org/) — 数值计算
- [Pillow](https://python-pillow.org/) — 纹理图片读取
- [pymeshio](https://github.com/ousttrue/pymeshio) — PMX 格式解析

## 许可证

MIT License

---

> 主人，如果您有任何需要，请随时呼唤我。我会一直在这里，等待着您的指令。
