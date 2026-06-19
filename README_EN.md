# Angeloid Alpha

[中文版](README.md)

> Master... this is a program for rendering PMX models. I, Ikaros, will do my best to serve you.

Angeloid Alpha is a pure C++20 MMD PMX model renderer. Features GPU skeletal skinning, VMD animation playback, and Bullet physics simulation.

<p align="center">
    <img title="Windows" src="https://github.com/Arkueid/angeloid-alpha/actions/workflows/build-windows.yml/badge.svg">
    <img title="macOS" src="https://github.com/Arkueid/angeloid-alpha/actions/workflows/build-macos.yml/badge.svg">
    <img title="Linux" src="https://github.com/Arkueid/angeloid-alpha/actions/workflows/build-linux.yml/badge.svg">
    <br>
    <img title="Release" src="https://img.shields.io/github/v/release/Arkueid/angeloid-alpha">
</p>

![preview](./docs/assets/preview.gif)

![preview2](./docs/assets/Snipaste_2026-06-14_17-55-29.png)

## Quick Start

```bash
# Build
cmake -B build -DBUILD_VIEWER=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build --config RelWithDebInfo

# Run
./build/viewer/RelWithDebInfo/viewer -m ikaros-uniform
```

## Documentation

| Document | Contents |
|----------|----------|
| [Build Guide](docs/BUILD.md) | C++ Viewer build, CLI, keyboard shortcuts, technical details |
| [Architecture](docs/ARCHITECTURE.md) | Design rationale |
| [Pluggable Render Pipeline](docs/pluggable-render-pipeline.md) | Pipeline/Slot/Effect system design |
| [Physics](docs/physics.md) | Bullet integration, bone feedback, joint constraints |
| [PMX Format](docs/pmx-format.md) | PMX file format reference |
| [Morphs](docs/morphs.md) | Expression/blend shape implementation |
| [Animation](docs/animation.md) | VMD playback, Bezier interpolation |
| [Rendering](docs/rendering.md) | GPU skinning, shaders, materials |

## Project Structure

```
angeloid/
├── core/                   # Computation layer (no GPU dependency)
│   ├── pmx/                #   PMX format (PmxModel, PmxReader)
│   ├── anim/               #   Animation (BoneSkinning, PhysicsWorld, VmdPlayer, MorphController)
│   └── math/               #   Vec2/3/4, Quat
├── framework/              # Rendering + framework layer
│   ├── gpu/                #   GPU abstraction (IGpuDevice, IGpuTexture, IGpuShader...)
│   │   ├── opengl/         #     OpenGL backend (GlDevice, GlTexture, GlShader...)
│   │   └── vulkan/         #     Vulkan backend (VulkanDevice, VkBuffer, VkTexture...)
│   ├── scene/              #   Debug visualization (GroundPlane, WorldAxis, RigidBodyRenderer)
│   ├── util/               #   Utilities (CfgParser, StbImage)
│   ├── Model.h/.cpp        #   Model facade
│   ├── Pipeline.h/.cpp     #   Render orchestrator
│   └── MMD.h/.cpp          #   Module init/dispose
├── viewer/                 # C++ app entry point (ImGui control panel)
├── resources/
│   ├── shaders/opengl/     # OpenGL GLSL shaders
│   ├── shaders/vulkan/     # Vulkan GLSL shaders (SPIR-V)
│   ├── toon/               # Shared toon textures
│   └── models/             # PMX model directories
├── thirdparty/             # GLFW, glad, Bullet, stb, backward-cpp, imgui
└── docs/                   # Technical docs
```

## Model Credits

Models under `resources/models/` are third-party assets with independent licenses. See each model's `readme.txt` for details.

- **姵儿** © Shanghai Playbox Information Technology Co., Ltd. — Model: 椛暗 / Design: Pre / Planning: 王乾龙Ashsteins
- **艾尔莎** © Xuyan Studio (虚研社) — Modeling: 悠米露 / Rigging: 补骨脂

## License

MIT License
