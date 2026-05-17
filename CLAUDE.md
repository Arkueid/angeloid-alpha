# Project: MMD PMX Viewer (angeloid-alpha)

## Build & Run
```bash
cd mmd && cmake -B ../build -S . && cmake --build ../build --config Release
../build/Release/mmd.exe -m <model-name>
```

## Project structure
- `mmd/` — C++ implementation (active)
  - `src/core/` — Application, Camera, Encoding
  - `src/gpu/` — VAO, VBO, Texture, ShaderProgram
  - `src/pmx/` — PmxModel, PmxReader
  - `src/anim/` — BoneSkinning, MorphController, VmdPlayer, VpdLoader
  - `src/render/` — ModelRenderer, ShaderManager, WorldAxis, PhysicsDebug
- `prototype/` — Python reference implementation
- `resources/` — shared assets (models, textures, shaders, VMD, VPD)

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
