# Build Guide

## C++ Viewer

CMake 3.20+, C++20 (MSVC / GCC / Clang).

```bash
cmake -B build -DBUILD_VIEWER=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build --config RelWithDebInfo
```

Dependencies (git submodules):
- [GLFW](https://www.glfw.org/) — window and input
- [glad](https://glad.dav1d.de/) — OpenGL loader
- [stb_image](https://github.com/nothings/stb) — texture loading
- [Bullet Physics](https://github.com/bulletphysics/bullet3) — physics engine

System dependencies:
- [Vulkan SDK](https://vulkan.lunarg.com/) — Vulkan backend + shaderc for GLSL→SPIR-V compilation

### CLI

```bash
./build/viewer/RelWithDebInfo/viewer                 # default model (Vulkan)
./build/viewer/RelWithDebInfo/viewer -m 姵儿          # named model
./build/viewer/RelWithDebInfo/viewer --gl -m 姵儿      # OpenGL backend
./build/viewer/RelWithDebInfo/viewer -v 动作.vmd       # play VMD
./build/viewer/RelWithDebInfo/viewer -v a.vmd b.vmd    # multiple VMDs
```

| Flag | Description |
|------|-------------|
| `-m, --model` | Model name (matches entries in `resources/models.cfg`) |
| `-v, --vmd` | VMD file(s), accepts multiple |
| `--vulkan, --vk` | Vulkan backend (default) |
| `--opengl, --gl` | OpenGL backend |

### Keyboard

| Key | Action |
|------|--------|
| Left mouse | Rotate / orbit |
| W/A/S/D | Move |
| E/Q | Up/down |
| Scroll | Adjust speed |
| M | Toggle FPS/Orbit camera |
| X | World axis |
| G | Ground grid |
| B | Rigidbody wireframe |
| H | Model mesh |
| O | Outline |
| T | Toon shading |
| K | GPU skinning |
| P | VPD pose |
| Space | VMD play/pause |
| [ / ] | VMD ±30 frames |
| I | Idle blink |
| < / > | Switch morph |
| ↑ / ↓ | Morph weight |
| R | Reset camera |
| Esc | Quit |

## Technical

- **GPU skinning**: Bone matrices packed as RGBA32F texture, `texelFetch` in shader. Up to 1024 bones.
- **Physics** (Bullet 3.27): PMX-native space, gravity = `-9.8 / modelScale`. YXZ rotation order. Shape size = 1.0× PMX values.
- **Joints**: `btGeneric6DofSpringConstraint`. PMX springs used as-is; locked translation DOFs auto-compensated (k=10000/2000/500).
- **Bone feedback**: `bodyCurr · inv(bodyInit) · boneBind` matrix formulation.
- **VMD animation**: Bezier interpolation + quaternion SLERP, multi-layer blending.
- **Morph**: Group / Vertex / UV / Material / Bone types supported.
