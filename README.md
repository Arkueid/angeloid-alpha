# Angeloid Alpha

[English Version](README_EN.md)

> 主人，这是一个用于渲染 PMX 模型的程序。我，伊卡洛斯，会尽我所能为主人服务……这样说的话，主人会开心吗？

Angeloid Alpha 是一个 MMD PMX 模型渲染器，C++20 核心 + Python 绑定。支持 GPU 骨骼蒙皮、VMD 动画、Bullet 物理仿真。

<p align="center">
    <img title="Windows" src="https://github.com/Arkueid/angeloid-alpha/actions/workflows/build-windows.yml/badge.svg">
    <img title="macOS" src="https://github.com/Arkueid/angeloid-alpha/actions/workflows/build-macos.yml/badge.svg">
    <img title="Linux" src="https://github.com/Arkueid/angeloid-alpha/actions/workflows/build-linux.yml/badge.svg">
    <br>
    <img title="Release" src="https://img.shields.io/github/v/release/Arkueid/angeloid-alpha">
    <img title="Python" src="https://img.shields.io/badge/python-3.10+-blue">
</p>

![preview](./docs/assets/preview.gif)

## 快速开始

需要 Python 3.10+。

```bash
pip install .                        # 安装（自动编译 C++ 扩展）
pip install glfw PyOpenGL            # 运行 main.py 需要的额外依赖
cd package && python main.py -m 姵儿  # 运行
```

```python
from angeloid import init, glInit, dispose, Model, Camera

glInit()
init("resources/shaders", "resources/toon", ["blink", "まばたき"])

model = Model()
model.load("resources/models/姵儿/椛暗式-姵儿ver1.2.pmx")
model.loadVmd("resources/vmd/dance.vmd")
model.playVmd(0)

cam = Camera()
while running:
    dt = compute_delta_time()
    cam.update(dt, w, a, s, d, e, q)
    model.update(dt)
    model.draw(width, height)

dispose()
```

## 文档

| 文档 | 内容 |
|------|------|
| [Python API 参考](docs/PYTHON_API.md) | Model / Camera / 模块函数的完整接口 |
| [构建指南](docs/BUILD.md) | C++ Viewer 编译、命令行、操作按键、技术细节 |
| [架构设计](docs/ARCHITECTURE.md) | 重构计划与设计理由 |
| [物理引擎](docs/physics.md) | Bullet 集成细节、骨骼反馈、关节约束 |
| [PMX 格式](docs/pmx-format.md) | PMX 文件格式参考 |
| [Morph 系统](docs/morphs.md) | 表情/变形目标实现 |
| [动画系统](docs/animation.md) | VMD 播放、贝塞尔插值 |
| [渲染管线](docs/rendering.md) | GPU 蒙皮、Shader、材质 |
| [已知陷阱](docs/pitfalls.md) | NaN/Inf、类型不一致等问题及修复 |

## 项目结构

```
mmd-demo/
├── mmd/                   # C++ 核心库
│   ├── pmx/               # PmxModel, PmxReader
│   ├── anim/              # BoneSkinning, PhysicsWorld, VmdPlayer, MorphController
│   ├── render/opengl/     # ModelRenderer, GPU 封装, 调试可视化
│   └── math/              # Vec2/3/4, Quat
├── viewer/                # C++ 原生应用入口
├── wrapper/               # CPython 绑定（pybind11-free, 纯 C API, SABIModule）
├── package/               # 可安装 Python 包 (angeloid) + main.py
├── resources/             # 模型/纹理/shader/VMD/VPD
├── thirdparty/            # GLFW, glad, Bullet, stb, backward-cpp
└── docs/                  # 技术文档
```

## 模型版权

`resources/models/` 下模型为第三方资产，各有独立许可。详见各模型目录下 `readme.txt`。

- **姵儿** © 上海鹏拜信息技术有限公司（Playbox） — 模型：椛暗 / 设定：Pre / 原案：王乾龙Ashsteins
- **艾尔莎** © 虚研社 — 建模：悠米露 / 绑定：补骨脂

## 许可证

MIT License
