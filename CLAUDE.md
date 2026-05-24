# Project: MMD PMX Viewer (angeloid-alpha)

## Build & Run
```bash
cd mmd && cmake -B ../build -S . && cmake --build ../build --config Release
../build/Release/mmd.exe -m <model-name>
```

## Project structure
- `mmd/` — C++20 implementation (active)
  - `src/core/` — Application, Camera, Encoding
  - `src/gpu/` — VAO, VBO, Texture, ShaderProgram
  - `src/pmx/` — PmxModel, PmxReader
  - `src/anim/` — BoneSkinning, MorphController, VmdPlayer, VpdLoader, PhysicsWorld (Bullet)
  - `src/render/` — ModelRenderer, ShaderManager, WorldAxis, PhysicsDebug
- `prototype/` — Python reference implementation
- `resources/` — shared assets (models, textures, shaders, VMD, VPD)

## Physics (Bullet)
- Bullet Physics as git submodule at `mmd/thirdparty/bullet` (version 3.27)
- `btGeneric6DofSpring2Constraint` for PMX joint types 0, 1, and default
  - PMX springs applied as-is (auto-limiting keeps them numerically stable)
  - Tiered spring fallback for tight translation DOFs (locked→k=2000, tight→k=500, narrow→k=100) with critical damping
- Mode 0 bodies: kinematic (`CF_KINEMATIC_OBJECT`), follow bones via `updateMode0Bodies()`
- Mode 1 bodies: fully dynamic, PMX damping as-is
- Mode 2 bodies: dynamic with delta-time-scaled corrective forces pulling toward bone target
- Solver: 20 iterations, ERP=0.6, CFM=0.0 (tighter constraint enforcement for light-body chains)
- Collision groups wired via `addRigidBody(group, mask)`
- `PhysicsWorld::debugDump()` (F key) prints MMD-space body positions and displacements

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
