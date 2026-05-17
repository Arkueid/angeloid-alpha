# Angeloid Alpha

[中文版](README.md)

> Master... this is a program for rendering PMX models. I will do my best to serve you.

A PMX model renderer with GPU skeletal animation and VMD animation playback. Rewritten in C++20.

## Build

```bash
cd mmd
cmake -B ../build -S .
cmake --build ../build --config Release
```

Requires CMake 3.20+, C++20 compiler (MSVC 2026+/GCC 13+/Clang 17+).

Third-party dependencies bundled:
- [GLFW](https://www.glfw.org/) — window and input
- [glad](https://glad.dav1d.de/) — OpenGL 4.6 Core loader
- [stb_image](https://github.com/nothings/stb) — texture loading

## Usage

```powershell
.\build\Release\mmd.exe                    # Default model: ikaros-uniform
.\build\Release\mmd.exe -m marine-swimwear # Named model
.\build\Release\mmd.exe -m 安比            # Chinese names work
.\build\Release\mmd.exe -v motion.vmd      # Load animation
.\build\Release\mmd.exe -v a.vmd b.vmd     # Mix multiple animations
```

## CLI Args

| Arg | Description |
|-----|-------------|
| `-m, --model` | Model name (see list below) |
| `-v, --vmd` | VMD animation file(s) |

## Model List

`ikaros-origin` `ikaros-uniform` `安比` `刀` `chloe` `aqua-swimwear` `marine-swimwear` `aqua-basebody` `aqua-sailor` `brujas` `lamy-swimwear` `lulum` `marine-jk1` `marine-jk1-hi` `rurudo-lion` `rurudo-lion-hi` `卢西娅` `卢西娅-摘帽` `卢西娅-武器1` `卢西娅-武器2`

## Keyboard Shortcuts

| Key | Function |
|-----|----------|
| Left mouse drag | Rotate view |
| W/A/S/D | Move forward/left/back/right |
| E/Q | Move up/down |
| Mouse scroll | Adjust speed |
| X | Toggle world axis |
| G | Toggle ground grid |
| B | Toggle rigidbody & joint |
| H | Toggle model mesh |
| O | Toggle outline |
| T | Toggle toon shading |
| K | Toggle GPU skinning |
| P | Toggle VPD pose |
| R | Reset camera |
| I | Toggle idle animation |
| M | Toggle morph mode |
| , / . | Switch morph |
| ↑ / ↓ | Adjust morph weight |
| Space | Play/Pause VMD |
| L | Toggle VMD loop |
| [ / ] | Step VMD ±30 frames |
| Esc | Exit |

## Project Structure

```
mmd-demo/
├── mmd/
│   ├── main.cpp
│   ├── CMakeLists.txt
│   ├── src/
│   │   ├── core/          # Application, Camera, Encoding
│   │   ├── gpu/           # VAO, VBO, Texture, Shader
│   │   ├── pmx/           # PmxModel, PmxReader
│   │   ├── anim/          # BoneSkinning, MorphController, VmdPlayer, VpdLoader
│   │   └── render/        # ModelRenderer, ShaderManager, WorldAxis, PhysicsDebug
│   └── thirdparty/        # GLFW, glad, stb_image
├── prototype/             # Python reference
├── resources/             # Models, textures, shaders, VMD, VPD
└── build/                 # Build output
```

## Technical Details

- GPU skinning via RGBA32F bone texture + texelFetch
- VMD bezier interpolation + quaternion SLERP
- Morph system: Group/Vertex/UV/Material/Bone types
- Cross-platform encoding: UTF-16LE↔UTF-8 in pure C++, CP932 via system API
- Toon shading + outline pass (back-face extrusion)

## License

MIT License
