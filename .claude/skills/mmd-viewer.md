# MMD PMX Viewer — Project Knowledge

## Build & Run
```bash
cmake -B build -S . && cmake --build build --config Release
./build/viewer/Release/viewer.exe -m <model-name> [-v a.vmd b.vmd]
```

## Architecture

```
mmd/                     # computation + rendering library (mmd.lib)
  Model.h/.cpp           # facade: load/update/draw
  pmx/                   # PmxModel, PmxReader (binary parser)
  anim/                  # BoneSkinning, MorphController, PhysicsWorld, VmdPlayer, VpdLoader
  encoding/              # UTF-16LE ↔ UTF-8
  math/                  # Vec2/3/4, Quat
  render/opengl/         # OpenGL backend
    gpu/                 # Mesh, Texture, Shader (raw GL wrappers)
    debug/               # RigidBodyRenderer, WorldAxis
    ModelRenderer, ShaderManager, BoneTextureUtil

viewer/                  # thin shell (viewer.exe)
  window/                # IWindow, GlfwWindow, Camera
  main.cpp (~290 lines)
```

## Key Design Decisions

### Physics in PMX-native space
- No `modelScale` applied to shapes/positions/joints
- Gravity auto-scaled: `-9.8 / modelScale` (~-99, matching saba's -98)
- Shapes: `size * kShapeScale` (no modelScale), default 0.9
- Positions: `shape_position - center` directly (no `* s`)

### Rotation order: YXZ (Ry * Rx * Rz)
- Matches saba and MMD convention
- All three places: rigid body rotation, joint rotation, debug display

### Collision mask: raw no_collision_group (no inversion)
- Matches saba convention (passes PMX field directly)
- `addRigidBody(body, 1 << group, no_collision_group)`
- This means group-0 bodies generally don't collide with each other

### setEquilibriumPoint() NOT called
- Matches saba behavior
- Springs default to constraint-frame equilibrium

### GPU modelMat
- VBO stores raw PMX coordinates (not pre-transformed)
- Model matrix handles display transform on GPU
- Skinning matrices are pure `world * inv(bind)` (no scale/offset baked in)

### Model class API
```cpp
mmd::Model model;
model.load(pmxPath, texDir, toonDir);
model.loadVpd(vpdPath);
model.update(dt);
model.draw(shaders, proj, view, camPos);
model.drawPhysicsDebug(shaders, proj, view);
model.enablePhysics(true);
```

## Physics Details

### Shape multipliers (PhysicsWorld.h)
```cpp
kSphereShapeScale  = 0.9f;
kBoxShapeScale     = 0.9f;
kCapsuleShapeScale = 0.9f;
```
- 0.5 = original (too small), 1.0 = PMX spec (too large due to collision)
- Compromise at 0.9 after removing modelScale

### Body modes
- Mode 0: kinematic, follows bones via `updateMode0Bodies()` (runs every frame)
- Mode 1: fully dynamic, PMX damping as-is
- Mode 2: dynamic + PID corrective forces toward bone target

### Joint constraints
- `btGeneric6DofSpringConstraint` for types 0, 1, default
- Tiered spring fallback for tight translation DOFs
- Hair/ponytail joints have rotation limits (0,0,0) = locked by PMX design

## Rendering

### Draw order (critical)
1. Outline pass (front-face cull + extrusion, must draw first)
2. Main model pass
3. Physics debug overlay

### Shader variants
| Morphs | Skinning | Toon | Shader |
|--------|----------|------|--------|
| No | No | No | "main" |
| No | No | Yes | "toon" |
| No | Yes | No | "skinned_notoon" |
| No | Yes | Yes | "skinned" |
| Yes | Yes | No | "morph_notoon" |
| Yes | Yes | Yes | "morph" |

### GL state leaks to watch
- `renderMainPass` calls `glDisable(GL_BLEND)` at end (non-skinned only)
- `renderMainPass` used to call `glDisable(GL_DEPTH_TEST)` — removed
- Physics debug must re-enable `GL_DEPTH_TEST` before rendering

## Morph System

### Pipeline
1. `mMorphCtl.setMorphWeight(name, w)` → stores weight, calls `updateMorphOffsets()`
2. `updateMorphOffsets()` → computes per-vertex offsets from all active morphs
3. `syncMorphOffsets()` → writes offsets to VBO via `morphVbo->write()`
4. Morph shader (`morph.vert`) applies: `morphed_pos = in_position + offset * morph_weight`

### Key fixes
- `setupSkinning()` must be called in `load()` (initializes morph VBOs)
- Draw must use morph shader variant when `hasActiveMorphs()` is true
- `morphWeight` persistence: save per-morph weights in a map, restore on switch

## Common Pitfalls

1. GL context must exist before any `glGen*` calls → create window first, then load model
2. `renderMainPass` leaks `glDisable(GL_BLEND)` → re-enable before debug overlays
3. VPD apply must recompute `mPoseWorld` and call `resetPhysics` → not just toggle flag
4. `updateMode0Bodies` must run even when physics is off → debug wireframes need it
5. Morph VBO writes go to null if `setupSkinning` not called
6. VmdPlayer defaults to `mPlaying = false` — must set `true` in constructor

## Model Names (registry in main.cpp)
`ikaros-origin`, `ikaros-uniform`, `安比`, `aqua-sailor`, `chloe`, `marine-jk1`, etc.
- ikaros-origin: breasts use AH bodies + mode-0 control bodies
- ikaros-uniform: 476 mode-1 bodies, 510 total, biggest model
- 安比: skirts use mode-2 (80 bodies)
