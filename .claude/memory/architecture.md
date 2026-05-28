---
name: architecture
description: Project structure, CMake build, layers, key files, model registry
type: reference
---

## Build
```bash
cmake -B build -S . && cmake --build build --config Release
./build/viewer/Release/viewer.exe -m <model> [-v a.vmd b.vmd]
```

## Directory Layout
```
mmd/                     # computation + rendering library → mmd.lib
  Model.h/.cpp           # facade: load/update/draw
  math/VecMath.h         # Vec2/3/4, Quat
  pmx/                   # PmxModel.h, PmxReader.h/.cpp
  encoding/              # Encoding.h/.cpp (UTF-16LE ↔ UTF-8)
  anim/                  # BoneSkinning, MorphController, PhysicsWorld, VmdPlayer, VpdLoader
  render/opengl/         # OpenGL backend
    gpu/                 # Mesh, Texture, Shader (raw GL)
    debug/               # RigidBodyRenderer, WorldAxis
    ModelRenderer, ShaderManager, BoneTextureUtil
viewer/                  # thin shell → viewer.exe
  window/                # IWindow, GlfwWindow, Camera
  main.cpp
thirdparty/              # git submodules: glfw, bullet; + glad, stb
resources/               # models/, shaders/, toon/, vmd/, vpd/
```

## CMake Targets
- `mmd` (static lib) — links Bullet + glfw + glad
- `viewer` (exe) — links mmd

## Key Design
- **Model facade** (`mmd::Model`): wraps PmxModel, ModelRenderer, PhysicsWorld, MorphController. Provides `load/update/draw` API.
- **GPU modelMat**: VBO stores raw PMX coords; `modelMat = scale*translate` applied in vertex shader. Skinning matrices are pure `world * inv(bind)`.
- **IWindow**: abstract window interface; GlfwWindow implements it. Camera is GLFW-free (pure key states).

## Model Name Registry
Defined in `viewer/main.cpp` MODELS map. Keys: `ikaros-origin`, `ikaros-uniform`, `安比`, `刀`, `chloe`, `aqua-swimwear`, `marine-swimwear`, `aqua-basebody`, `aqua-sailor`, `brujas`, `lamy-swimwear`, `lulum`, `marine-jk1`, `marine-jk1-hi`, `rurudo-lion`, `rurudo-lion-hi`, `卢西娅`, `卢西娅-摘帽`, `卢西娅-武器1`, `卢西娅-武器2`.
