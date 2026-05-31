# Angeloid Alpha

[中文版](README.md)

> Master... this is a program for rendering PMX models. I, Ikaros, will do my best to serve you.

Angeloid Alpha is an MMD PMX model renderer with a C++20 core and Python bindings. Features GPU skeletal skinning, VMD animation playback, and Bullet physics simulation.

<p align="center">
    <img title="Windows" src="https://github.com/Arkueid/angeloid-alpha/actions/workflows/build-windows.yml/badge.svg">
    <img title="macOS" src="https://github.com/Arkueid/angeloid-alpha/actions/workflows/build-macos.yml/badge.svg">
    <img title="Linux" src="https://github.com/Arkueid/angeloid-alpha/actions/workflows/build-linux.yml/badge.svg">
    <br>
    <img title="Release" src="https://img.shields.io/github/v/release/Arkueid/angeloid-alpha">
    <img title="Python" src="https://img.shields.io/badge/python-3.10+-blue">
</p>

![preview](./docs/assets/preview.gif)

## Quick Start

Requires Python 3.10+.

```bash
pip install .                        # Install (builds C++ extension automatically)
pip install glfw PyOpenGL            # Extra deps for main.py
cd package && python main.py -m 姵儿  # Run
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

## Documentation

| Document | Contents |
|----------|----------|
| [Python API Reference](docs/PYTHON_API.md) | Full Model / Camera / module function API |
| [Build Guide](docs/BUILD.md) | C++ Viewer build, CLI, keyboard shortcuts, technical details |
| [Architecture](docs/ARCHITECTURE.md) | Refactoring plan and design rationale |
| [Physics](docs/physics.md) | Bullet integration, bone feedback, joint constraints |
| [PMX Format](docs/pmx-format.md) | PMX file format reference |
| [Morphs](docs/morphs.md) | Expression/blend shape implementation |
| [Animation](docs/animation.md) | VMD playback, Bezier interpolation |
| [Rendering](docs/rendering.md) | GPU skinning, shaders, materials |

## Project Structure

```
mmd-demo/
├── mmd/                   # C++ core library
│   ├── pmx/               # PmxModel, PmxReader
│   ├── anim/              # BoneSkinning, PhysicsWorld, VmdPlayer, MorphController
│   ├── render/opengl/     # ModelRenderer, GPU wrappers, debug viz
│   └── math/              # Vec2/3/4, Quat
├── viewer/                # C++ native app entry
├── wrapper/               # CPython bindings (pybind11-free, pure C API, SABIModule)
├── package/               # Installable Python package (angeloid) + main.py
├── resources/             # Models/textures/shaders/VMD/VPD
├── thirdparty/            # GLFW, glad, Bullet, stb, backward-cpp
└── docs/                  # Technical docs
```

## Model Credits

Models under `resources/models/` are third-party assets with independent licenses. See each model's `readme.txt` for details.

- **姵儿** © Shanghai Playbox Information Technology Co., Ltd. — Model: 椛暗 / Design: Pre / Planning: 王乾龙Ashsteins
- **艾尔莎** © Xuyan Studio (虚研社) — Modeling: 悠米露 / Rigging: 补骨脂

## License

MIT License
