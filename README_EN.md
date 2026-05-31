# Angeloid Alpha

[中文版](README.md)

> Master... this is a program for rendering PMX models. I, Ikaros, will do my best to serve you. I said that, but I'm not sure if it's the right thing to say.

Angeloid Alpha is an MMD PMX model renderer with a C++20 core and Python bindings. Features GPU skeletal skinning, VMD animation playback, and Bullet physics simulation.

![preview](./docs/assets/preview.gif)

## Python Quick Start

Master can use me directly from Python. You'll need to build the Python bindings first (`_angeloid.pyd` targets Python 3.14), then install the dependencies.

```bash
# 1. Build (generates _angeloid.pyd, must match your Python version)
cmake -B build -S .
cmake --build build --config Release

# 2. Install dependencies
cd package
pip install glfw PyOpenGL

# 3. Run
python main.py -m 姵儿
```

```python
from angeloid import init, glInit, dispose, Model, Camera

# Initialize
glInit()
init("resources/shaders", "resources/toon", ["blink", "まばたき"])

# Load model
model = Model()
model.load("resources/models/姵儿/椛暗式-姵儿ver1.2.pmx")

# Load VPD pose and VMD animation
vpd_id = model.loadVpd("resources/vpd/natural-stand.vpd")
model.applyVpd(vpd_id)
track_id = model.loadVmd("resources/vmd/dance.vmd")
model.playVmd(track_id)

# Game loop
cam = Camera()
while running:
    dt = compute_delta_time()
    cam.update(dt, w, a, s, d, e, q)
    model.update(dt)
    model.draw(width, height)

# Cleanup
dispose()
```

## Python API Reference

Master, this is everything I can offer. Please don't ask me to do things beyond my capabilities... although I will try to meet your expectations.

### Module Functions

| Function | Description |
|----------|-------------|
| `init(shaderDir, toonDir, blinkMorphs=[])` | Initialize renderer. `shaderDir` points to `.vert/.frag` files, `toonDir` to toon textures, `blinkMorphs` lists eye-blink morph names. |
| `glInit()` | Initialize OpenGL (call after `glfw.make_context_current()`) |
| `dispose()` | Release all GPU resources and model data |
| `initArgs() -> dict` | Return current configuration, including `blinkMorphs` |

### class `Model`

Physics engine, bone skinning, and animation playback are all encapsulated inside... Master doesn't need to know how complex it is.

**Loading & Update**

| Method | Description |
|--------|-------------|
| `load(path: str)` | Load a PMX model. Chinese paths are supported... although I don't understand why filenames use kanji. |
| `update(dt: float)` | Per-frame update: advance VMD → physics step → GPU bone upload |
| `draw(width: int, height: int)` | Render the model (handles skinned/static/morph passes automatically) |

**VMD Animation** (returns track ID; -1 on failure)

| Method | Description |
|--------|-------------|
| `loadVmd(path: str) -> int` | Load VMD file. Returns track ID. Multiple VMDs can be loaded simultaneously. |
| `playVmd(trackId, onEnd=None)` | Start playback. `onEnd` is an optional callback `(int) -> None`. |
| `pauseVmd(trackId)` / `stopVmd(trackId)` / `removeVmd(trackId)` | Pause/stop/remove a track |
| `playAllVmd()` / `pauseAllVmd()` / `stopAllVmd()` | Global control over all tracks |
| `isVmdPlaying() -> bool` | Whether any track is playing |
| `vmdCurrentFrame(trackId) -> float` | Current frame number. `vmdMaxFrame` exists but its value is unreliable—do not depend on it. |
| `setVmdFrame(trackId, frame)` | Jump to a specific frame |

**VPD Pose**

| Method | Description |
|--------|-------------|
| `loadVpd(path: str) -> int` | Load VPD pose file |
| `applyVpd(vpdId)` / `removeVpd(vpdId)` | Apply/remove a pose |
| `resetPose()` / `syncVpdPose()` | Reset to bind pose / re-sync VPD |
| `vpdApplied() -> bool` | Whether a VPD pose is active |

**Display Toggles**

| Method | Description |
|--------|-------------|
| `showModel(bool)` / `getShowModel() -> bool` | Toggle main model visibility |
| `showOutline(bool)` / `getShowOutline() -> bool` | Toggle edge outline |
| `showToon(bool)` / `getShowToon() -> bool` | Toggle toon shading |
| `setSkinning(bool)` / `isSkinned() -> bool` | GPU skinning toggle. Turn it off and the model goes to T-pose... that's its original form. |
| `showRigidBodies(bool)` | Show physics collision wireframes (debug). Blue = mode-2, orange = mode-1... do they look like my hair ornaments? |

**Physics**

| Method | Description |
|--------|-------------|
| `enablePhysics(bool)` / `physicsEnabled() -> bool` | Physics simulation toggle. When off, the skirt won't flutter... it will hang quietly. |

**Morphs**

| Method | Description |
|--------|-------------|
| `setMorphWeight(name: str, weight: float)` | Set morph weight (0.0 ~ 1.0) |
| `savedMorphWeight(name) -> float` | Query last-set weight |
| `clearMorphs()` | Clear all morphs |
| `setMorphWeights(dict)` | Batch set, e.g. `{"smile": 0.5, "frown": 0.3}` |
| `setIdleBlink(bool)` | Enable/disable auto-blink. My eyes... blink every 4 seconds. |
| `morphCount() -> int` | Total morph count |
| `interactableMorphs() -> list[int]` | Indices of interactable morphs |
| `morphName(index) -> str` | Get morph name by index |

**Properties (read-only)**

| Property | Type | Description |
|----------|------|-------------|
| `modelName()` | `str` | PMX model name |
| `modelScale()` | `float` | Model scale (`2 / maxDimension`) |
| `modelMatrix()` | `list[float]` | 16-float column-major 4×4 matrix |

### class `Camera`

| Property/Method | Description |
|------|------|
| `x, y, z` | Camera position... Master can freely move the view. |
| `rotX, rotY` | Rotation angles (radians) |
| `speed` | Movement speed, scroll wheel adjusts |
| `update(dt, w, a, s, d, e, q)` | Per-frame update. WASD movement, EQ vertical. |
| `onMouseButton(pressed)` / `onCursorPos(x, y)` / `onScroll(yoffset)` | Mouse input |
| `reset()` | Reset to default position |
| `viewMatrix() -> list[float]` | 16-float column-major View matrix |
| `projectionMatrix(w, h, fov=45, near=0.1, far=1000) -> list[float]` | Static method, column-major Projection matrix |

## C++ Build

Master, if you want to build from source... these commands can bring the program into existence.

```bash
cmake -B build -S .
cmake --build build --config Release
```

Requires CMake 3.20+, C++20 compiler.

Dependencies (bundled or git submodule):
- [GLFW](https://www.glfw.org/) — window and input. This is a library... I think.
- [glad](https://glad.dav1d.de/) — OpenGL 4.6 Core loader
- [stb_image](https://github.com/nothings/stb) — texture loading
- [Bullet Physics](https://github.com/bulletphysics/bullet3) — physics engine. Master's models can move... after many fixes, it should be fine now.

## C++ CLI

```bash
.\build\viewer\Release\viewer.exe         # default model
.\build\viewer\Release\viewer.exe -m 姵儿  # named model
.\build\viewer\Release\viewer.exe -v motion.vmd          # play VMD
.\build\viewer\Release\viewer.exe -v a.vmd b.vmd         # multiple VMDs
```

| Flag | Description |
|------|-------------|
| `-m, --model` | Model name (matches `resources/models/` subdir) |
| `-v, --vmd` | VMD file(s), accepts multiple |

## Keyboard Shortcuts

| Key | Function |
|-----|----------|
| Left mouse | Rotate view |
| W/A/S/D | Move |
| E/Q | Vertical |
| Scroll | Adjust speed |
| X | World axis |
| B | Rigid body wireframe |
| Y | Toggle physics |
| H | Toggle model mesh |
| O | Toggle outline |
| T | Toggle toon |
| Space | VMD play/pause |
| [ / ] | VMD ±30 frames |
| P | Toggle VPD pose |
| I | Auto blink |
| K | Toggle GPU skinning |
| < / > | Switch morph |
| ↑ / ↓ | Adjust morph weight |
| R | Reset camera |
| Esc | Exit |

## Technical Details

- **GPU skinning**: Bone matrices packed into RGBA32F texture, sampled via `texelFetch` in shader. Up to 1024 bones.
- **Physics** (Bullet 3.27): PMX-native space, gravity = `-9.8 / modelScale`. YXZ rotation order. Shape sizes at 1.0× PMX-defined values.
- **Joints**: `btGeneric6DofSpringConstraint`. PMX springs used as-is; tight translation DOFs get automatic compensation (k=10000/2000/500 tiers).
- **Bone feedback**: Matrix multiply `bodyCurr · inv(bodyInit) · boneBind`, correctly handling rotation-induced translation. Mode-2 bodies retain animation position, rotation-only feedback.
- **VMD animation**: Bezier interpolation + quaternion SLERP, multi-layer blending.
- **Morphs**: Supports Group / Vertex / UV / Material / Bone types. Material morph index=-1 applies to all materials.

## Project Structure

```
mmd-demo/
├── mmd/                   # C++ core library (no GPU dep)
│   ├── pmx/               # PmxModel, PmxReader
│   ├── anim/              # BoneSkinning, PhysWorld, VmdPlayer, MorphCtl
│   ├── render/opengl/     # ModelRenderer, GPU wrappers, debug viz
│   └── math/              # Vec2/3/4, Quat
├── viewer/                # C++ native app entry
├── wrapper/               # CPython bindings (pybind11-free, pure C API)
├── package/               # Python app + angeloid package
├── resources/             # Models/textures/shaders/VMD/VPD
├── thirdparty/            # GLFW, glad, Bullet, stb
└── docs/                  # Technical docs (architecture/physics/format ref)
```

## Model Credits

Models under `resources/models/` are third-party assets with independent licenses. See each model's `readme.txt` for details.

- **姵儿** © Shanghai Playbox Information Technology Co., Ltd. — Model: 椛暗 / Design: Pre / Planning: 王乾龙Ashsteins
- **艾尔莎** © Xuyan Studio (虚研社) — Modeling: 悠米露 / Rigging: 补骨脂

## License

MIT License

---

> Master, after all the fixes... the physics engine now matches saba's reference implementation. The skirt won't clip through anymore—you can use it with confidence. If you need anything else, I am always at your service.
