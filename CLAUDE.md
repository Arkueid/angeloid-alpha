# Project: MMD PMX Viewer (angeloid-alpha)

## Build & Run

MSVC compiler requires `vcvars64.bat` sourced first. Use the helper script:

```bat
build.bat                           # default: RelWithDebInfo
build.bat -DENABLE_STACKTRACE=ON   # with crash stack trace (requires thirdparty/backward-cpp)
```

Or manually (must run from VS Developer Command Prompt / after vcvars):
```bash
cmake -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -B build
cmake --build build
./build/viewer/viewer.exe -m <model-name>
```

- `ENABLE_STACKTRACE` option: includes backward-cpp submodule for crash stack traces (printed to stderr, includes file/line/function)

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
- Shape size multipliers in `PhysicsWorld.h`: `kSphereShapeScale`, `kBoxShapeScale`, `kCapsuleShapeScale` (all **1.0**, matching saba)
  - Changed from 0.9 to 1.0 (2026-05-31): 0.9 made collision bodies ~10% smaller than PMX-defined sizes, causing skirt-body clipping with leg capsules. Verified against saba reference implementation.
- Collision mask passes PMX `no_collision_group` directly (matching saba)
- `btGeneric6DofSpringConstraint` for PMX joint types 0, 1, and default
  - PMX springs applied as-is; tiered fallback for tight translation DOFs (locked→k=10000, tight→k=2000, narrow→k=500)
  - `setEquilibriumPoint()` not called (matching saba)
  - Capsule shape min-radius clamp (0.01) to prevent numerical instability
- Mode 0 bodies: kinematic (`CF_KINEMATIC_OBJECT`), follow bones via `updateMode0Bodies()`
- Mode 1 bodies: fully dynamic, PMX damping as-is
- Mode 2 bodies: dynamic with corrective forces pulling toward bone target
- **Bone feedback** (physics→skeleton): uses full matrix multiplication matching saba
  - Formula: `boneMat = bodyCurr · inv(bodyInit) · boneBind`
  - `BulletBody` stores precomputed `invBodyInit` and `boneBindMat` (btTransform) at build time
  - Previous vector-addition approach (`bonePos = initPos + displacement`) ignored rotation-induced translation, causing ~0.06 unit Y drift in skirt bones. Fixed 2026-05-31 after cross-validation against saba.
  - `computeBoneTarget()` (mode-0 kinematic bodies) uses the same matrix formulation: `animBone · inv(bindBone) · bodyInit`
- Body deactivation: enabled (0.5s timeout), with low thresholds (0.08 linear, 0.02 angular)
- `PhysicsWorld::debugDump()` (F key) prints displaced body positions
- `PhysicsWorld::debugFullDump()` (periodic, currently commented out) prints comprehensive per-body analysis
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

## Docs
- `docs/INDEX.md` — entry point indexing all domain knowledge files
- `docs/ARCHITECTURE.md` — refactoring plan and design rationale
- `docs/reference/` — MMD format reference materials (PMX spec, joint constraints)
- Before any non-trivial change, check relevant docs for context

## Model Credits
Models in `resources/models/` are third-party assets with their own licenses. See each model's `readme.txt` for details.

- **姵儿** (椛暗式-姵儿ver1.2): 模型作者 椛暗 | 人物设定 Pre | 企划原案 王乾龙Ashsteins | © 上海鹏拜信息技术有限公司 (Playbox) — 改造・二次配布可（需注明出处）
- **艾尔莎** (居家服): 建模 悠米露 | 绑定 补骨脂（卟咕子） | © 虚研社 — 非商业使用可・改造可（需注明出处）

## Git
- Commit messages: concise summary line, bullet details
- Keep commits focused, don't mix unrelated changes
- Only commit when the user explicitly asks
- When committing, update README.md / README_EN.md / CLAUDE.md if stale
