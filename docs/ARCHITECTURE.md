> ⚠️ **Historical planning document (2025).**  Many items have been superseded.
> Current architecture is documented in `CLAUDE.md` and `docs/rendering.md`.
> Key divergences: no AssetStore exists; ShaderManager is a separate singleton
> from RenderContext; Model implements Renderable interface; Pipeline is list-driven.

# MMD PMX Viewer — Architecture

## Overview

Single-window MMD model viewer. Loads PMX models, plays VMD animations, applies VPD poses, runs Bullet physics. Uses OpenGL 3.3+ with GLFW.

```
viewer/main.cpp        — Application shell (window, input, main loop)
mmd/Model.h/.cpp       — Facade: owns and orchestrates all subsystems
mmd/pmx/               — PMX file format (PmxModel, PmxReader)
mmd/anim/              — Animation (BoneSkinning, MorphController, VmdPlayer, VpdLoader, PhysicsWorld)
mmd/render/opengl/     — Rendering (ModelRenderer, ShaderManager, RenderContext, GPU wrappers)
mmd/math/              — Vec2/3/4, Quat, Mat4
mmd/encoding/          — Text encoding (cp932 <-> UTF-8)
resources/             — Shared assets (models, textures, shaders, VMD, VPD)
viewer/window/         — IWindow interface, GlfwWindow implementation
viewer/debug/          — WorldAxis, ground grid
```

---

## Viewer API Surface

The viewer only interacts with these public symbols:

```
Camera::instance()          — existing, unchanged
mmd::Model                  — existing, unchanged
mmd::init(args)             — NEW: initialize mmd module (GPU resources)
mmd::shader(name)           — NEW: get built-in shader (axis, etc.)
mmd::dispose()           — NEW: cleanup all global resources
```

Everything else (`AssetStore`, `RenderContext`, `ShaderManager`) is internal to `mmd/`. The viewer only needs `#include "MMD.h"` + `#include "Model.h"`.

---

## Resource Ownership — Current vs Target

### Current State (pre-refactor)

```
viewer/main.cpp
  |-- mmd::Model model                    (single instance)
  |     |-- PmxModel mData                (per-model — correct)
  |     |-- unique_ptr<ShaderManager>     (per-model — WASTE: same GLSL for all models)
  |     |-- ModelRenderer mRenderer
  |     |     |-- mTextures               (per-model — correct: unique per character)
  |     |     |-- mSharedToons[10]        (per-model — WASTE: same toon01-10.bmp for all)
  |     |     |-- mesh/VAO/VBO            (per-model — correct)
  |     |-- PhysicsWorld                  (per-model — correct)
  |     |-- MorphController              (per-model — correct)
  |     |-- unique_ptr<VmdMixer>
  |     |     |-- vector<VmdPlayer>       (per-model — VmdAnimation DATA is shareable)
  |     |-- map<string, VpdPose>          (per-model — VPD DATA is shareable)
  |-- WorldAxis                          (global — fine)
  |-- Gpu::ShaderProgram axisShader      (duplicated with ShaderManager's copy)
```

### Target State (post-refactor)

```
// --- Internal singletons, invisible to viewer ---

AssetStore (Meyer's singleton) — CPU-only, renderer-agnostic
  |-- VMD animations            map<path, unique_ptr<VmdAnimation>>
  |-- VPD poses                 map<path, unique_ptr<VpdPoseMap>>

RenderContext (Meyer's singleton) — OpenGL resources
  |-- ShaderManager             migrated from Model
  |     |-- 10+ shader programs (main, toon, skinned, morph, outline, axis, rigidbody)
  |     |-- gradient texture (4x1 ramp)
  |-- Shared toon textures[10]  migrated from ModelRenderer::mSharedToons

// --- Viewer-facing ---

mmd::Model (per-instance)
  |-- PmxModel mPmx             (renamed from mData)
  |-- ModelRenderer mRenderer   (mTextures + mesh stay; toons -> RenderContext)
  |-- PhysicsWorld mPhysics     (stays)
  |-- MorphController mMorphCtl (stays)
  |-- RigidBodyRenderer         (stays)
  |-- VmdMixer / VmdPlayState[]  (playback state per-model; data -> AssetStore)
  |-- VPD state                 (on/off per-model; data -> AssetStore)
```

---

## AssetStore Design

### Purpose

**Meyer's singleton** holding all loaded VMD/VPD data. **CPU-only** — renderer-agnostic. Internal to `mmd/`, not visible to viewer.

```cpp
class AssetStore {
public:
    static AssetStore& instance();

    const VmdAnimation* loadVmd(const std::filesystem::path& path);
    const VpdPoseMap* loadVpd(const std::filesystem::path& path);
    void release();  // Clears all loaded data

private:
    AssetStore() = default;
    ~AssetStore() = default;

    std::unordered_map<std::string, std::unique_ptr<VmdAnimation>> mVmdData;
    std::unordered_map<std::string, std::unique_ptr<VpdPoseMap>> mVpdData;
};
```

- **`loadVmd()` / `loadVpd()`:** If already loaded (same absolute path), return cached `const*`. Otherwise load, store, return.
- **`release()`:** Clears both maps. All returned `const*` become dangling.
- **Destructor:** Safety net if `release()` wasn't called.
- **Data size:** VMD ~0.5-2 MB, VPD ~12 KB. Load once, keep forever. No ref-counting, no stale entries.

---

## RenderContext Design

### Purpose

**Meyer's singleton** holding OpenGL GPU resources (shader programs, shared toon textures, gradient). Internal to `mmd/`, not visible to viewer.

```cpp
class RenderContext {
public:
    static RenderContext& instance();

    void init(const std::filesystem::path& shaderDir,
              const std::filesystem::path& toonDir);
    void release();  // Frees all GPU resources (must call while GL context alive)

    Gpu::ShaderProgram* shader(const std::string& name);
    Gpu::Texture* gradientTexture();
    Gpu::Texture* sharedToon(int index);  // 0-9

private:
    RenderContext() = default;
    ~RenderContext() = default;

    std::unique_ptr<ShaderManager> mShaderManager;
    std::unique_ptr<Gpu::Texture> mSharedToons[10];
};
```

- **`init()`:** Must call once while GL context is alive. Creates all shader programs and loads toon textures.
- **`release()`:** Frees all GPU resources (`glDelete*`). Must be called before GL context is destroyed.
- **Destructor:** Safety net — warns if GPU resources were leaked.

Switching to Vulkan: replace `RenderContext` internals. `AssetStore` and `Model` are unaffected.

---

## mmd::dispose()

A single free function that releases all global resources in the correct order:

```cpp
namespace mmd {
void dispose() {
    // GPU resources first (GL context still alive via viewer)
    RenderContext::instance().release();
    // CPU data
    AssetStore::instance().release();
}
}
```

Called once at the end of `main()`:

```cpp
int main() {
    GlfwWindow app(...);
    Camera::instance();

    RenderContext::instance().init(shaderDir, toonDir);

    mmd::Model model;
    model.load(...);
    model.loadVmd(...);

    app.run();

    mmd::dispose();  // GPU -> CPU, in order
    return 0;
}
```

---

## Per-Model VMD/VPD State

### Data Flow

```
AssetStore::instance()              RenderContext::instance()
       |                                    |
  const VmdAnimation*            Gpu::ShaderProgram*
  const VpdPoseMap*              Gpu::Texture*
       |                                    |
       v                                    v
  Model A        Model B           Model A::draw()
  VmdPlayState   VmdPlayState
  (frame=50)     (frame=120)
```

### VmdPlayState

```cpp
struct VmdPlayState {
    const VmdAnimation* animation = nullptr;  // non-owning, valid for session lifetime
    float currentFrame = 0;
    bool playing = true;
    bool loop = true;
};
```

- `loadVmd(path)` -> `AssetStore::loadVmd()` returns `const VmdAnimation*`, Model creates a `VmdPlayState` referencing it
- `vmdPlay()` / `vmdPause()` / `setVmdFrame()` -> operate on this model's tracks
- `update(dt)` -> advances each track's `currentFrame` independently

### VMD Mixer

Stays **per-model**. Refactored to use `VmdPlayState` instead of owning `VmdAnimation`.

```cpp
// Before:  std::vector<VmdPlayer>  mPlayers;
// After:   std::vector<VmdPlayState> mPlayStates;
```

---

## Key Design Decisions

| Decision | Rationale |
|---|---|
| AssetStore: Meyer's singleton | CPU-only — static destruction is safe. Convenient access without exposing to viewer. |
| RenderContext: Meyer's singleton | GPU resources; `release()` called explicitly while GL context alive. |
| `mmd::dispose()` for cleanup | Viewer only needs one cleanup call. Order enforced: GPU then CPU. |
| AssetStore: CPU-only, no GPU types | Switching renderer (OpenGL -> Vulkan) doesn't touch AssetStore. |
| VMD/VPD: load once, keep forever | ~1 MB per VMD, ~12 KB per VPD. No ref-counting, no stale cache. |
| VMD data shared, `VmdPlayState` per-model | Data is immutable; state is tiny. Independent playback per model. |
| ShaderManager -> RenderContext | GL shader programs are context-global; per-model compilation wastes GPU objects. |
| toon01-10.bmp -> RenderContext | PMX spec calls them "shared toon"; designed for global reuse. |
| Model-specific textures stay per-model | Each character has unique diffuse/normal/sphere textures. |
| `mData` -> `mPmx` | More specific than "data". |

---

## Implementation Steps

### Phase 1: New files

1. Create `mmd/AssetStore.h` / `mmd/AssetStore.cpp`
2. Create `mmd/render/opengl/RenderContext.h` / `mmd/render/opengl/RenderContext.cpp`
3. Move `ShaderManager` ownership from `Model` to `RenderContext`
4. Move shared toon loading from `ModelRenderer` to `RenderContext`
5. Move axis shader from `viewer/main.cpp` into `RenderContext` / `ShaderManager`

### Phase 2: Refactor VMD/VPD

6. Add `VmdPlayState` struct to `VmdPlayer.h`
7. Refactor `VmdMixer` to use `VmdPlayState` instead of `VmdPlayer`
8. Update `Model::loadVmd()` -> go through `AssetStore`
9. Update `Model::loadVpd()` -> go through `AssetStore`
10. Keep Model playback API identical

### Phase 3: Wire up viewer

11. `main()`: call `RenderContext::instance().init(shaderDir, toonDir)` once
12. Rename `mData` -> `mPmx` in Model
13. Remove `mShaders` from Model; use `RenderContext::instance()` for shaders
14. Update `Model::draw()` to use `RenderContext` for shader/toon access
15. Update `WorldAxis` to use shader from `RenderContext`
16. Add `mmd::dispose()` call before `main()` returns
17. Test parity with current behavior

---

## File Changes Summary

| File | Change |
|---|---|
| `mmd/AssetStore.h` | **NEW** — CPU-only singleton for VMD/VPD |
| `mmd/AssetStore.cpp` | **NEW** |
| `mmd/render/opengl/RenderContext.h` | **NEW** — GPU resource singleton |
| `mmd/render/opengl/RenderContext.cpp` | **NEW** |
| `mmd/release.h` | **NEW** — `dispose()` declaration |
| `mmd/release.cpp` | **NEW** — `dispose()` implementation |
| `mmd/Model.h` | `mData` -> `mPmx`; delete `mShaders`; `mVmdMixer` -> tracks; `mVpdPoses` -> `const*` |
| `mmd/Model.cpp` | `load()` no longer creates ShaderManager; `draw()` from RenderContext |
| `mmd/anim/VmdPlayer.h` | Add `VmdPlayState` struct; refactor `VmdMixer` |
| `mmd/anim/VmdPlayer.cpp` | `VmdPlayState` impl; mixer uses play states |
| `mmd/render/opengl/ModelRenderer.h` | Remove `mSharedToons` |
| `mmd/render/opengl/ModelRenderer.cpp` | Shared toon refs from RenderContext |
| `viewer/main.cpp` | Init RenderContext; remove inline axis shader; add `mmd::dispose()` |

---

## Invariants

- **Viewer only sees Camera, Model, and `mmd::dispose()`** — no other `mmd/` internals exposed
- **AssetStore is CPU-only** — no GL/Vulkan types, renderer-agnostic
- **RenderContext owns all GPU objects** — freed via `release()` before GL context destroyed
- **`dispose()` order: GPU then CPU** — RenderContext before AssetStore
- **Model never creates shaders or shared toons** — always via RenderContext
- **VMD/VPD: load once, keep forever** — `unique_ptr` in AssetStore, no stale entries
- **Model playback API unchanged** — `loadVmd()`, `vmdPlay()`, `vmdPause()` etc. work identically
- **Each model advances its own animation time** — no shared clock
