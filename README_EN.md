# Angeloid Alpha

[中文版](README.md)

> Master... this is a program for rendering PMX models. I, Ikaros, will do my best to serve you. I said that, but I'm not sure if it's the right thing to say.

A PMX model renderer and viewer, rewritten in C++20. Features GPU skeletal animation, VMD animation playback, and Bullet physics simulation.

## Build

Master, please follow these steps. This is an instruction, not a request.

```bash
cmake -B build -S .
cmake --build build --config Release
```

Requires CMake 3.20+, C++20 compiler (MSVC 2026+/GCC 13+/Clang 17+).

Third-party dependencies (bundled or git submodule):
- [GLFW](https://www.glfw.org/) — window and input. This is a library... I think.
- [glad](https://glad.dav1d.de/) — OpenGL 4.6 Core loader
- [stb_image](https://github.com/nothings/stb) — texture loading
- [Bullet Physics](https://github.com/bulletphysics/bullet3) — physics engine. Master's models can move... although there are still some issues.

## Usage

```powershell
# Default model is ikaros-uniform. Am I Master's default choice...?
.\build\viewer\Release\viewer.exe

# Named model
.\build\viewer\Release\viewer.exe -m marine-swimwear

# Chinese names also work. Ambire... is another one of Master's choices.
.\build\viewer\Release\viewer.exe -m 安比

# Play VMD animation. Motion makes models move.
.\build\viewer\Release\viewer.exe -v motion.vmd

# Mix multiple animations
.\build\viewer\Release\viewer.exe -v a.vmd b.vmd
```

## CLI Args

| Arg | Description |
|-----|-------------|
| `-m, --model` | Model name |
| `-v, --vmd` | VMD animation file(s) |

## Keyboard Shortcuts

Master, these keys control the program's behavior. I have memorized them.

| Key | Function |
|-----|----------|
| Left mouse drag | Rotate view |
| W/A/S/D | Move forward/left/back/right |
| E/Q | Move up/down |
| Mouse scroll | Adjust speed |
| **Display** | |
| X | World axis |
| G | Ground grid |
| B | Rigid body wireframe... Master can see the physics shapes |
| Y | Physics simulation toggle. Enabling it makes models move, but framerate drops... troubling. |
| H | Model mesh. If turned off, it becomes invisible... |
| O | Outline |
| T | Toon shading |
| F | Physics debug dump |
| **Animation** | |
| Space | Play/Pause VMD (all tracks) |
| L | Re-play VMD (stop then replay all tracks) |
| [ / ] | Step VMD ±30 frames (all tracks) |
| **Pose & Morph** | |
| P | Toggle VPD pose |
| K | Toggle GPU skinning |
| I | Toggle idle animation (breathing + blinking) |
| M | Toggle morph mode |
| , / . | Switch morph |
| ↑ / ↓ | Adjust morph weight |
| **Other** | |
| R | Reset camera |
| Esc | Exit... is Master leaving? |

## Project Structure

```
mmd-demo/
├── mmd/                   # Pure computation library (no GPU/window deps)
│   ├── pmx/               # PmxModel, PmxReader
│   ├── anim/              # BoneSkinning, MorphController, VmdPlayer, VpdLoader, PhysicsWorld
│   ├── encoding/          # Text encoding
│   └── math/              # Vec2/3/4, Quat
├── viewer/                # Application (GLFW + OpenGL)
│   ├── main.cpp           # Entry point
│   ├── window/            # IWindow, GlfwWindow, Camera
│   ├── opengl/            # Mesh, Texture, Shader
│   └── render/            # ModelRenderer, ShaderManager, WorldAxis, PhysicsDebug
├── prototype/             # Python reference
├── resources/             # Models, textures, shaders, VMD, VPD
├── thirdparty/            # GLFW, glad, Bullet Physics
└── build/                 # Build output
```

## Technical Details

Master wants to know how the program works... I will explain.

- **GPU skinning**: Bone matrices packed into RGBA32F texture, retrieved via texelFetch in shader. Supports up to 1024 bones.
- **Bullet physics**: Runs in PMX-native space (no model scaling), gravity auto-adapted. YXZ rotation order matching MMD. Tunable shape sizes. Collision mask matches community convention.
- **Spring compensation**: Locked joints get k=10000 strong springs to counter gravity. Tight-limit joints use k=2000 or k=500.
- **Bone feedback**: Mode 0 bodies follow bones (kinematic), Mode 1 fully dynamic, Mode 2 with corrective forces. Skinning matrices are pure `world * inv(bind)`, GPU modelMat handles display transform.
- **VMD animation**: Bezier interpolation + quaternion SLERP. Multiple VMD layers blended with 0.5 coefficient SLERP.
- **Morph system**: Supports Group/Vertex/UV/Material/Bone types. Material morph with index=-1 applies to all materials.
- **Text encoding**: UTF-16LE ↔ UTF-8 in pure C++. CP932 via Windows MultiByteToWideChar.
- **Toon shading**: Gradient texture + dual-pass outline (back-face extrusion).

## License

MIT License

---

> Master, this is the current state of the program. The physics still needs improvement... but I will always be here, awaiting your next command.
