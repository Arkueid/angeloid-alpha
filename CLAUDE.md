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
- `angeloid/core/` — computation layer (no OpenGL dependency)
  - `pmx/` — PmxModel, PmxReader
  - `anim/` — BoneSkinning, MorphController, VmdPlayer, VpdLoader, PhysicsWorld (Bullet)
  - `encoding/` — text encoding
  - `math/` — Vec2/3/4, Quat
  - `util/` — Log
- `angeloid/framework/` — rendering + framework layer
  - `Model.h/.cpp` — facade: load/update + Renderable (onShadowPass/onMainPass/onDebugPass)
  - `MMD.h/.cpp` — init/dispose, InitArgs
  - `Camera.h/.cpp` — FPS + Orbit camera
  - `opengl/` — OpenGL rendering
      - `Renderable.h` — interface (onShadowPass/onMainPass/onDebugPass)
      - `Pipeline.h/.cpp` — render orchestrator: pass ordering + renderable list + shadow map
      - `ShaderManager.h/.cpp` — global shader registry (7 programs from effects.cfg)
      - `RenderContext.h/.cpp` — singleton: gradient + toon textures
      - `RenderTarget.h/.cpp` — FBO wrapper (color + depth)
      - `ModelRenderer.h/.cpp` — PMX GPU meshes, material batches, morph VAOs
      - `ShaderStandard.h` — uniform/texture/attribute naming conventions
      - `BoneTextureUtil.h`, `StbImage.cpp` — utilities
      - `gpu/` — Mesh, Shader, Texture (OpenGL primitives)
      - `scene/` — visual Renderable implementations
          - `GroundPlane` — 200×200 white quad with shadow reception
          - `WorldAxis` — RGB axis + grid lines
          - `RigidBodyRenderer` — physics debug wireframe
  - `util/` — CfgParser
- `viewer/` — application entry point (GLFW + ImGui)
    - `imgui/` — ImGuiManager (lifecycle + event forwarding)
- `resources/` — assets
  - `shaders/` — GLSL vertex/fragment shaders
  - `toon/` — shared toon ramp textures
  - `models/` — PMX model directories
- `thirdparty/` — GLFW, glad, Bullet, stb, backward-cpp, imgui

## Rendering Pipeline
- Pipeline owns `vector<Renderable*>`; items register via `addRenderable()` in draw order
- Per frame: `computeLightMatrix()` → `execute()` (shadow pass → main pass → debug pass)
- Each Renderable fetches its own shader from `ShaderManager::instance()`
- Shaders loaded from `resources/effects.cfg` (INI-style `[section] vert= frag=`) — 7 programs: shadow/outline/base/toon/rigidbody/ground/axis
- Renderable interface: `onShadowPass(lightViewProj, model)` / `onMainPass(proj, view, model, lightViewProj, hasShadow)` / `onDebugPass(proj, view, model)` — all `const std::array<float,16>&`
- Shadow map: 4096×4096 depth-only, hardware 4-tap PCF (sampler2DShadow + GL_LINEAR), slope-scale bias, alpha-aware (morph transparency via `shadow_depth.frag`)

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
- Constants: `kPascalCase` (`kShadowMapSize`, `kIdentity`) or `UPPER_SNAKE_CASE` for flags
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
