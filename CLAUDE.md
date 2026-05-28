# Project: MMD PMX Viewer (angeloid-alpha)

## Build & Run
```bash
cmake -B build -S . && cmake --build build --config Release
./build/viewer/Release/viewer.exe -m <model-name>
```

## Project structure
- `mmd/` — computation + rendering library
  - `Model.h/.cpp` — facade class: load/update/draw
  - `pmx/` — PmxModel, PmxReader
  - `anim/` — BoneSkinning, MorphController, VmdPlayer, VpdLoader, PhysicsWorld (Bullet)
  - `encoding/` — text encoding
  - `math/` — Vec2/3/4, Quat
  - `render/opengl/` — ModelRenderer, ShaderManager, GPU wrappers
      - `gpu/` — Mesh, Texture, Shader (OpenGL primitives)
      - `debug/` — RigidBodyRenderer, WorldAxis
- `viewer/` — thin application shell
  - `window/` — IWindow, GlfwWindow, Camera
- `prototype/` — Python reference implementation
- `resources/` — shared assets (models, textures, shaders, VMD, VPD)

## Physics (Bullet)
- Bullet Physics as git submodule at `thirdparty/bullet` (version 3.27)
- Runs in PMX-native space (no modelScale); gravity auto-scaled: `-9.8 / modelScale`
- Rotation order: YXZ (`Ry*Rx*Rz`), matching saba and MMD convention
- Shape size multipliers in `PhysicsWorld.h`: `kSphereShapeScale`, `kBoxShapeScale`, `kCapsuleShapeScale` (all 0.9)
- Collision mask passes PMX `no_collision_group` directly (matching saba)
- `btGeneric6DofSpringConstraint` for PMX joint types 0, 1, and default
  - PMX springs applied as-is; tiered fallback for tight translation DOFs (locked→k=10000, tight→k=2000, narrow→k=500)
  - `setEquilibriumPoint()` not called (matching saba)
  - Capsule shape min-radius clamp (0.01) to prevent numerical instability
- Mode 0 bodies: kinematic (`CF_KINEMATIC_OBJECT`), follow bones via `updateMode0Bodies()`
- Mode 1 bodies: fully dynamic, PMX damping as-is
- Mode 2 bodies: dynamic with corrective forces pulling toward bone target
- `PhysicsWorld::debugDump()` (F key) prints body positions; `debugTrackCloth()` periodic cloth logging
- Model-space transform moved to GPU `modelMat`; skinning matrices are pure `world * inv(bind)`

## Code conventions
- File naming: PascalCase (`PmxModel.h`, `BoneSkinning.cpp`)
- Methods: `camelCase` (`readF32`, `hasFlag`, `vertexCount`)
- Members: `mPascalCase` (`mWindow`, `mData`, `mDeltaTime`)
- Constants: `UPPER_SNAKE_CASE` (`BONEFLAG_TAILPOS_IS_BONE`)
- Statics: `sPascalCase`
- Types: `PascalCase` (`PmxModel`, `BinaryReader`)
- Free functions → class static methods preferred
- No exceptions / try-catch in main flow (validate before use)
- No comments for obvious code; only comment non-obvious WHY

## Git
- Commit messages: concise summary line, bullet details
- Keep commits focused, don't mix unrelated changes
- Only commit when the user explicitly asks
- When committing, update README.md / README_EN.md / CLAUDE.md if stale
