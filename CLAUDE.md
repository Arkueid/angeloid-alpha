# Project: MMD PMX Viewer (angeloid-alpha)

## ⛔ COMMIT POLICY (HIGHEST PRIORITY)

**NEVER commit anything without explicit user permission.** Always ask the user
for approval before running `git commit`. This overrides all other instructions.

---

## Build & Run

```bash
# Configure + build (MSVC requires vcvars64.bat sourced first)
cmake -B build -DBUILD_VIEWER=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build --config RelWithDebInfo

# Run
./build/viewer/RelWithDebInfo/viewer -m <model-name>
```

- `ENABLE_STACKTRACE` option: includes backward-cpp submodule for crash stack traces (printed to stderr, includes file/line/function)

## Project structure
- `mmd/` — computation + rendering library
  - `Model.h/.cpp` — facade class: load/update/draw
  - `pmx/` — PmxModel, PmxReader
  - `anim/` — BoneSkinning, MorphController, VmdPlayer, VpdLoader, PhysicsWorld (Bullet)
  - `encoding/` — text encoding
  - `math/` — Vec2/3/4, Quat
  - `render/opengl/` — ModelRenderer, Pipeline, Effect, ShaderManager, RenderContext, GPU wrappers
      - `gpu/` — Mesh, Texture, Shader (OpenGL primitives)
      - `debug/` — RigidBodyRenderer
  - `util/` — CfgParser, Log
- `viewer/` — application entry point, WorldAxis, ImGui debug panels
    - `imgui/` — `ImGuiManager` (lifecycle + GLFW event forwarding)
- `resources/` — assets
  - `core/` — engine resources (shaders, effects, toon)
  - `app/` — user content (models, VPD, VMD)
- `thirdparty/` — GLFW, glad, Bullet, stb, backward-cpp, imgui

## Rendering Pipeline
- Effects defined in `resources/effects.cfg` (INI-style sections)
- `Pipeline` class maps model state ("static"|"skinned"|"morph") → Effect → shader programs
- `Effect::loadAll()` compiles shaders via `Gpu::ShaderProgram`; `Pipeline::execute()` drives per-frame draw order: Outline → Opaque → Debug
- `ShaderManager` still owns gradient + shared toon textures (used by Pipeline)
- Shader standard (`ShaderStandard.h`) defines uniform/texture/attribute naming conventions but built-in shaders use legacy names — pending migration

## Physics (Bullet)
- Bullet Physics as git submodule at `thirdparty/bullet` (version 3.27)
- Runs in PMX-native space (no modelScale); gravity auto-scaled: `-9.8 / modelScale`
- Rotation order: YXZ (`Ry*Rx*Rz`), matching saba and MMD convention
- Shape size multipliers in `PhysicsWorld.h`: `kSphereShapeScale`, `kBoxShapeScale`, `kCapsuleShapeScale` (all **1.0**, matching saba)
- Collision mask passes PMX `no_collision_group` directly (matching saba)
- `btGeneric6DofSpringConstraint` for PMX joint types 0, 1, and default
- Mode 0 bodies: kinematic (`CF_KINEMATIC_OBJECT`), follow bones via `updateMode0Bodies()`
- Mode 1 bodies: fully dynamic, PMX damping as-is
- Mode 2 bodies: dynamic with corrective forces pulling toward bone target
- **Bone feedback** (physics→skeleton): uses full matrix multiplication matching saba
  - Formula: `boneMat = bodyCurr · inv(bodyInit) · boneBind`
  - `BulletBody` stores precomputed `invBodyInit` and `boneBindMat` (btTransform) at build time
- Body deactivation: enabled (0.5s timeout), with low thresholds (0.08 linear, 0.02 angular)

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
- `docs/BUILD.md` — build guide (CMake, CLI, keyboard shortcuts)
- `docs/ARCHITECTURE.md` — refactoring plan and design rationale
- `docs/pluggable-render-pipeline.md` — Pipeline/Slot/Effect architecture
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
