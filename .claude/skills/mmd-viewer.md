# MMD PMX Viewer — Complete Project Knowledge

## 1. Quick Reference

### Build & Run
```bash
cmake -B build -S . && cmake --build build --config Release
./build/viewer/Release/viewer.exe -m <model-name> [-v a.vmd b.vmd]
```

### Key Files
| File | Role |
|------|------|
| `mmd/Model.h/.cpp` | Facade: load/update/draw |
| `mmd/anim/PhysicsWorld.h/.cpp` | Bullet physics world |
| `mmd/anim/BoneSkinning.h/.cpp` | Bone matrix computation |
| `mmd/anim/MorphController.h/.cpp` | Morph offset computation |
| `mmd/anim/VmdPlayer.h/.cpp` | VMD animation parsing/playback |
| `mmd/render/opengl/ModelRenderer.h/.cpp` | Model drawing (9 VAOs, 6 render paths) |
| `mmd/render/opengl/gpu/Mesh.h/.cpp` | VAO/VBO wrappers |
| `mmd/render/opengl/ShaderManager.h/.cpp` | Shader loading |
| `mmd/render/opengl/debug/RigidBodyRenderer.h/.cpp` | Physics debug wireframes |
| `viewer/main.cpp` | Window + input + wiring (~290 lines) |
| `viewer/window/GlfwWindow.h/.cpp` | GLFW window (implements IWindow) |
| `viewer/window/Camera.h/.cpp` | FPS camera |

---

## 2. Project Architecture

```
angeloid-alpha/
├── CMakeLists.txt                # root: add_subdirectory(mmd) + add_subdirectory(viewer)
├── mmd/                          # computation + rendering library → mmd.lib
│   ├── CMakeLists.txt
│   ├── Model.h / Model.cpp       # facade class
│   ├── StbImage.cpp              # stb_image implementation
│   ├── math/VecMath.h            # Vec2/3/4, Quat
│   ├── pmx/                      # PmxModel.h, PmxReader.h/.cpp
│   ├── encoding/                 # Encoding.h/.cpp (UTF-16LE ↔ UTF-8)
│   ├── anim/                     # BoneSkinning, MorphController, PhysicsWorld, VmdPlayer, VpdLoader
│   └── render/opengl/
│       ├── ModelRenderer.h/.cpp
│       ├── ShaderManager.h/.cpp
│       ├── BoneTextureUtil.h
│       ├── StbImage.cpp
│       ├── gpu/                  # Mesh, Texture, Shader (raw GL wrappers)
│       └── debug/                # RigidBodyRenderer, WorldAxis
├── viewer/                       # thin shell → viewer.exe
│   ├── CMakeLists.txt
│   ├── main.cpp
│   └── window/                   # IWindow.h, GlfwWindow.h/.cpp, Camera.h/.cpp
├── thirdparty/                   # git submodules
│   ├── glfw/                     # git@github.com:glfw/glfw.git
│   ├── bullet/                   # https://github.com/bulletphysics/bullet3.git
│   ├── glad/                     # OpenGL loader
│   └── stb/                      # stb_image.h
├── resources/                    # models/, shaders/, toon/, vmd/, vpd/
└── prototype/                    # Python reference (legacy)
```

### Dependency Flow
```
mmlib (mmd) → viewer.exe
  │              │
  Bullet         GLFW + glad
  GL/glew.h
```

### Key design: Model facade class
```cpp
namespace mmd {
class Model {
    void load(pmxPath, texDir, toonDir);       // PmxReader + ModelRenderer + Physics build
    void loadVpd(vpdPath);                      // VPD pose loading
    void update(float dt);                      // VMD + physics + idle time
    void draw(ShaderManager&, proj, view, cam); // morph sync + outline + model
    void drawPhysicsDebug(ShaderManager&, proj, view);
    
    void enablePhysics(bool);
    void showModel(bool);
    void applyVpd(bool);
    void setMorphWeight(name, weight);
    void clearMorphs();
    
    // Internal:
    void syncBoneTexture();   // mPoseWorld → GPU bone texture
    void syncMorphOffsets();  // idle blink + VBO write
};
}
```

---

## 3. Physics — Complete Knowledge

### 3.1 World Setup
*File: `mmd/anim/PhysicsWorld.cpp`*

```cpp
// Constants
kGravityY = -9.8f           // scaled by 1/modelScale in build()
kSolverIterations = 15
kSubsteps = 10
kFixedTimestep = 1.0f / 240.0f
kMaxTimestep = 1.0f / 30.0f
ERP2 = 0.8f

// Shape multipliers (in PhysicsWorld.h)
kSphereShapeScale  = 0.9f   // btSphereShape radius
kBoxShapeScale     = 0.9f   // btBoxShape half-extents
kCapsuleShapeScale = 0.9f   // btCapsuleShape radius & height
```

### 3.2 PMX-native Space (no modelScale)
**History**: Originally physics scaled everything by `modelScale` (~0.1). Shapes became tiny (half-extent 0.025 < Bullet margin 0.04), precision broke. Fixed by removing all `* s` / `/ s` from positions, shapes, joints, CCD. Gravity auto-scaled to compensate: `-9.8/modelScale ≈ -99` (matches saba's -98).

```
Positions: shape_position - center (no *s)
Shapes: size * kShapeScale (no modelScale)
Gravity: -9.8 / modelScale
```

### 3.3 Rotation Order: YXZ
**History**: Originally used ZYX (`setEulerZYX(z,y,x)` = Rz*Ry*Rx). Found to be wrong — breast capsules appeared sideways instead of front-back. saba uses `Ry * Rx * Rz`. Fixed in all three places:
- `addRigidBody()`: `qy * qx * qz`
- `addJoint()`: joint rotation
- `PhysicsDebug.cpp buildPass()`: rotation matrix

### 3.4 Collision Mask
**History**: Originally used `~no_collision_group & 0xFFFF` (invert non-collision mask). This was technically correct per PMX spec. But saba passes raw `no_collision_group` directly. All group-0 bodies then don't collide with each other. PMX models were authored assuming saba's behavior. Changed to match saba.

```cpp
mWorld->addRigidBody(body, 1 << collision_group, no_collision_group);
// NOT: ~no_collision_group & 0xFFFF
```

### 3.5 setEquilibriumPoint()
**History**: Originally called. Caused chest vibration issues. saba does NOT call it. Commented out. Springs default to constraint-frame equilibrium, which is the initial pose at joint creation time.

### 3.6 Joint Springs
```cpp
btGeneric6DofSpringConstraint for types 0, 1, default
Tiered fallback for tight translation DOFs:
  range < 0.001 → k = 10000
  range < 0.2   → k = 2000
  range < 0.5   → k = 500
  damping = 0.02
```

### 3.7 Body Modes
- **Mode 0**: `CF_KINEMATIC_OBJECT`, mass=0. Follows bone via `updateMode0Bodies()`. Runs EVERY frame (even when physics off — needed for debug wireframes).
- **Mode 1**: Full dynamic, PMX damping as-is. Affected by gravity + constraints.
- **Mode 2**: Dynamic + PID corrective forces pulling toward bone target. `posErr * 50.0 - velocity * 15.0`.

### 3.8 Bone Feedback
```cpp
// In getBoneTransforms():
boneNewPos = boneInitPos + (bodyPos - bodyInitPos);  // delta applied to bind pos
boneNewRot = bodyDeltaRot * boneInitRot;

// Convert back to MMD space:
tx = boneNewPos.x() + center.x;  // no /s (PMX-native)
```

### 3.9 Cloth-like Body Detection
*File: `mmd/anim/PhysicsWorld.cpp` build()*
- Bodies with rotation springs + rotation limit range ≥ 0.01 → clothLike
- clothLike bodies: debugTrackCloth() periodic logging

### 3.10 Post-Physics Bones
```cpp
BoneSkinning::recomputeAfterPhysicsBones(model, vpdPoses, poseWorld);
// Only processes bones with BONEFLAG_IS_AFTER_PHYSICS_DEFORM (0x1000)
// Recomputes local transform relative to parent, applies VPD pose
```

### 3.11 CCD (Continuous Collision Detection)
```cpp
if (mass > 0) {
    ccdRadius = min dimension * shapeScale (or *0.5 for box)
    setCcdMotionThreshold(ccdRadius * 0.5f);
    setCcdSweptSphereRadius(ccdRadius);
}
```

### 3.12 Hair/Ponytail Joints
PMX data has rotation limits (0,0,0) — all DOFs locked by the model author. This is intentional: ponytail bending comes from VMD animation keyframes on bone rotation, not from physics joint rotation. Unlocking would cause gravity droop without spring-backs.

---

## 4. Rendering — Complete Knowledge

### 4.1 GPU Model Matrix
**History**: Originally vertices were pre-transformed on CPU: `(v-center)*scale`. Skinning matrices had `t*scale + rotOff - offsetI` baked in. Moved to GPU side:
- VBO stores raw PMX coordinates
- Skinning matrices are pure `world * inv(bind)`
- `modelMat` = `{s,0,0,0, 0,s,0,0, 0,0,-s,0, -cx*s, -my*s, cz*s, 1}` applied in vertex shader

### 4.2 Draw Order (CRITICAL)
```cpp
// In Model::draw():
1. syncMorphOffsets()      // compute + upload morph offsets
2. sync material overrides  // from morph material changes
3. IF skinned:
   a. Outline pass (morph_outline or outline_skinned)  // MUST BE FIRST
   b. Main pass (morph/morph_notoon or skinned/skinned_notoon)
4. IF non-skinned:
   a. Outline pass (outline)
   b. Main pass (toon or main)
```

**Why outline first**: Outline uses front-face culling + vertex extrusion along normals. It draws a shell that must be in the depth buffer BEFORE the main model. If main model draws first, depth test culls the outline shell.

### 4.3 Shader Variants Matrix
| hasMorphs | useSkinning | showToon | Main Shader | Outline Shader |
|-----------|-------------|----------|-------------|----------------|
| No | No | No | "main" | "outline" |
| No | No | Yes | "toon" | "outline" |
| No | Yes | No | "skinned_notoon" | "outline_skinned" |
| No | Yes | Yes | "skinned" | "outline_skinned" |
| Yes | Yes | No | "morph_notoon" | "morph_outline" |
| Yes | Yes | Yes | "morph" | "morph_outline" |

### 4.4 GL State Management
**Known leaks:**
- `renderMainPass()` calls `glDisable(GL_BLEND)` at end — must re-enable for subsequent passes
- `renderMainPass()` USED to call `glDisable(GL_DEPTH_TEST)` — **REMOVED** (was breaking debug overlays)
- Outline passes call `glEnable(GL_CULL_FACE)` / `glCullFace(GL_FRONT)` / `glDisable(GL_CULL_FACE)` — properly reset
- `onRender` sets `glFrontFace(GL_CW)` — PMX uses clockwise winding

**Physics debug restore:**
```cpp
glEnable(GL_DEPTH_TEST);
glDepthFunc(GL_LEQUAL);
```

### 4.5 GPU Backend (namespace Gpu)
| Class | Wraps | Key Methods |
|-------|-------|-------------|
| `Vao` | `glGenVertexArrays` | `bind()`, `render(mode)`, `setEbo()`, `destroy()` |
| `VboWrapper` | `glGenBuffers` | `write(data, bytes)` → `glBufferData(GL_DYNAMIC_DRAW)` |
| `Texture` | `glGenTextures` | `bind(unit)`, `write(data)`, `setFilter()` |
| `ShaderProgram` | `glCreateProgram` | `use()`, `setMat4()`, `setVec3()`, `setInt()` |

### 4.6 ModelRenderer VAOs
9 VAOs total:
- `mModelVao`, `mToonVao`, `mOutlineVao` — static (non-skinned) rendering
- `mSkinnedVao`, `mSkinnedVaoNoToon`, `mSkinnedOutlineVao` — GPU skinning
- `mMorphVao`, `mMorphVaoNoToon`, `mMorphOutlineVao` — skinning + morph offsets

### 4.7 Bone Texture Format
- RGBA32F texture, 4 texels per matrix (column-major 4x4)
- `BoneSkinning::packBoneMatrices()` packs float[16] into texel rows
- `BoneTextureUtil::createBoneTexture()` creates GPU texture from CPU data
- Shader: `fetch_bone_matrix(index)` uses `texelFetch()` to read 4 consecutive texels

### 4.8 Toon Shading
- Gradient 1D texture (4 levels of gray) bound to GL_TEXTURE1 or GL_TEXTURE2
- Rim lighting: `rim_power=4.0`, white rim color
- Camera position passed as uniform for rim calculation

---

## 5. Animation — Complete Knowledge

### 5.1 VMD Playback
*Files: `mmd/anim/VmdPlayer.h/.cpp`*

```cpp
// Load and play:
auto vmdAnim = VmdAnimation::load(path);
auto mixer = std::make_unique<VmdMixer>();
mixer->addVmd(std::move(vmdAnim));
mixer->play();

// Per frame:
mixer->update(dt);  // advances mCurrentFrame
mixer->getBoneTransform(boneName, pos, rot);  // Bezier interpolation
mixer->getMorphWeight(morphName);              // linear interpolation
```

**Critical bug fix**: `VmdPlayer` constructor defaulted `mPlaying = false`. Changed to `mPlaying = true`. Otherwise VMD would never advance frames. Also, `VmdMixer::playing()` returns mPlaying which is set by `play()/pause()`.

**Multi-layer VMD**: Weighted mixing via `VmdMixer`. Each player's bone transform contributes equally (first match wins, or blending if overlapping).

### 5.2 VPD Pose
*File: `mmd/anim/VpdLoader.h/.cpp`*
- VPD = static pose file (bone rotations + translations)
- `VpdLoader::load(path)` → `unordered_map<string, VpdPose>`
- Applied via `BoneSkinning::computePoseWorldMatrices(model, vpdPoses)`
- P key toggles: recompute poseWorld with/without VPD, reset physics

### 5.3 Idle Blink
- Every 4 seconds, 0.15s triangle-wave blink
- Morph name: `まばたき` (ikaros-origin), also tries `blink`, `blink_l`, `blink_r`, `ウィンク` etc.
- Weight set directly in `morphWeights()` map → `updateMorphOffsets()` → VBO write
- Disabled during VMD playback (`!mVmdMixer->playing()`)
- `mIdleTime` accumulated in `Model::update(dt)`, consumed in `syncMorphOffsets()`

### 5.4 VMD Expression Morphs
- VMD contains morph keyframes for facial expressions
- `VmdMixer::getMorphWeight(name)` iterates all players, sums weights (clamped 0-1)
- Applied in `Model::update()` via `morphCtl.setMorphWeights(vmdMorphs)`

---

## 6. Morph System — Complete Knowledge

### 6.1 Pipeline
```
setMorphWeight(name, w)
  → mMorphWeights[name] = w
  → updateMorphOffsets()
      → clear mPosOffsets/mUvOffsets
      → iterate mMorphWeights
      → for each: find morph in model.morphs
      → accumulate vertex offsets × weight
  → syncMorphOffsets() [in Model]
      → idle blink weight injection
      → updateMorphOffsets() again
      → morphVbo->write(mPosOffsets)
      → uvMorphVbo->write(mUvOffsets)
  → draw() selects morph shader variant
      → shader reads in_morph_offset (location 5)
      → morphed_pos = in_position + offset * morph_weight
```

### 6.2 Morph Types (PMX spec)
| Type | Enum | Description |
|------|------|-------------|
| Group | 0 | Recursively applies child morphs × rate |
| Vertex | 1 | Per-vertex position offset |
| Bone | 2 | Bone transform (position + quaternion rotation) |
| UV | 3 | UV offset (base UV) |
| UV_EXT1-4 | 4-7 | Additional UV layers |
| Material | 8 | Material properties (diffuse, specular, etc.) |
| Flip | 9 | Selects one morph from list by index |
| Impulse | 10 | Physics impulse on rigid body |

### 6.3 Key Fixes
1. **setupSkinning() must be called in load()** — initializes morph VBO wrappers. Without it, all morph writes go to null.
2. **Morph shader must be selected** — `hasActiveMorphs()` check gates shader choice. Without it, morph offsets are never consumed by vertex shader.
3. **Weight persistence** — `savedWeights` map in main.cpp tracks per-morph weights across comma/period switching.

### 6.4 Material Morph Override
- `morphCtl.getMaterialOverride(matIndex)` → `MatMorphOverride` (alpha, diffuse, specular, ambient multipliers)
- `renderer.setMaterialOverride(i, *ov)` per frame
- Applied as multipliers: `materialValue * overrideValue`

---

## 7. Bone Skinning — Complete Knowledge

### 7.1 Matrix Pipeline
```
PMX bone hierarchy
  → computeBindWorldMatrices(model)       // T-pose world matrices
  → computePoseWorldMatrices(model, vpd)  // VPD-modified world matrices
  → computePoseWorldMatrices(model, vpd, vmd)  // VPD + VMD world matrices
  → physics: getBoneTransforms(poseWorld)  // modify with physics deltas
  → recomputeAfterPhysicsBones            // for BONEFLAG_AFTER_PHYSICS
  → computeSkinningMatrices(model, poseWorld)  // world * inv(bind)
  → packBoneMatrices → RGBA32F texture
  → GPU: skin_matrix * in_position
```

### 7.2 Key Functions
| Function | Input | Output |
|----------|-------|--------|
| `computeBindWorldMatrices` | PmxModel | bind world mats |
| `computePoseWorldMatrices` | PmxModel + VpdPoses | pose world mats |
| `computeSkinningMatrices` | PmxModel + poseWorld | world * inv(bind) |
| `packBoneMatrices` | float[16]*n | RGBA texel array |
| `extractSkinningData` | PmxModel | positions, normals, uvs, bone indices, weights |

---

## 8. PMX File Format Notes

### Shape Sizes (PMX spec vs Bullet)
| PMX Shape | PMX size meaning | Bullet param | Conversion |
|-----------|-----------------|--------------|------------|
| Sphere (0) | size.x = radius | `btSphereShape(r)` | `r = size.x * kSphereShapeScale` |
| Box (1) | size = (w, h, d) | `btBoxShape(half)` | `half = size * kBoxShapeScale` |
| Capsule (2) | size.x = radius, size.y = height | `btCapsuleShape(r, h)` | `r = size.x * kCapsuleShapeScale, h = size.y * kCapsuleShapeScale` |

**History**: Originally all shapes multiplied by 0.5 (thinking PMX sizes were diameters/full dimensions). This was wrong for sphere and capsule per PMX spec, but ended up being correct visually (matching community convention and saba behavior). The 0.9 multiplier was a compromise to avoid collision issues at PMX-native scale.

### Joint Types (PMX 2.1)
| Type | Name | Bullet Constraint |
|------|------|-------------------|
| 0 | Spring 6DOF | `btGeneric6DofSpringConstraint` |
| 1 | 6DOF | `btGeneric6DofSpringConstraint` (springs disabled) |
| 2 | P2P | `btPoint2PointConstraint` |
| 3 | ConeTwist | `btConeTwistConstraint` |
| 4 | Slider | `btSliderConstraint` |
| 5 | Hinge | `btHingeConstraint` |

### Rigid Body Operation Modes
| Mode | Name | Bullet Behavior |
|------|------|-----------------|
| 0 | Bone Follow (Static) | Kinematic, mass=0, follows bone |
| 1 | Dynamic | Full mass, gravity, constraints |
| 2 | Dynamic + Bone Merge | Dynamic + PID pull toward bone |

### Bone Flags (key ones)
| Flag | Value | Meaning |
|------|-------|---------|
| `BONEFLAG_TAILPOS_IS_BONE` | 0x0001 | Connection target is bone index |
| `BONEFLAG_IS_AFTER_PHYSICS_DEFORM` | 0x1000 | Recompute after physics step |

### Morph Types
See Section 6.2 above.

---

## 9. Debug Visualization

### 9.1 RigidBodyRenderer (formerly PhysicsDebug)
- Shows rigid bodies as wireframe shapes + joints as lines
- B key toggles display
- Lazy-initialized on first draw (needs active GL context)
- Reads body transforms from Bullet via `updateFromPhysics()`
- Mode 0 bodies follow pose via `updateMode0Bodies()` (runs every frame)

### 9.2 WorldAxis
- X key: toggle RGB axis lines
- G key: toggle ground grid
- Both use "axis" shader

### 9.3 Physics Debug Dump
- F key: `physicsWorld.debugDump()` — prints displaced bodies to console

---

## 10. Model-Specific Knowledge

### ikaros-uniform
- 510 bodies, 846 joints, 655 bones, 105k vertices
- 476 mode-1 dynamic bodies (largest model)
- 340 skirt bodies (裙_X_Y): box shapes, degree=4 grid, mass=10, mode=1
- Breast: 6 clothLike bodies (左胸上/下, 右胸上/下, 左胸上2, 右胸上2) with rotation springs
- Ponytail: 马尾_0-18 chains, rotation limits (0,0,0) = rigid rods

### ikaros-origin
- 255 bodies, 377 joints
- Breast: uses mode-0 control bodies (左胸操作) + mode-1 auxiliary (AH1/AH2)
- Breast capsule 左胸操作接続: tall capsule (r=0.1, h=1.515), rot=(0, 1.57, -1.57)
- modelScale ≈ 0.099

### 安比 (Anby)
- 125 bodies, 153 joints
- 80 mode-2 skirt bodies (only model using mode-2 for skirts)
- Skirts: 裙_X_Y, box shapes, mass=0.7-1.0

### aqua-sailor
- 106 bodies, 103 joints
- Skirt: スカート1/2/3 bodies, mode-0 parent (スカート親) controls shape
- Light mass (0.1) for skirt bodies

### chloe / marine-jk1
- Similar structure: mode-0 skirt wall (スカート壁/スカート親) + light mode-1 children

---

## 11. Common Pitfalls & Lessons Learned

### Must-haves
1. **GL context before GL calls** — create window first, then load model. Model loading creates VAOs, textures via GL.
2. **setupSkinning in load()** — initializes morph VBOs. Without it, ALL morphs silently fail.
3. **updateMode0Bodies every frame** — even when physics is off (debug wireframes need it).
4. **VPD toggle must recompute poseWorld** — just flipping `mVpdApplied` flag does nothing.
5. **VmdPlayer must default mPlaying=true** — otherwise VMD never advances frames.

### State Leaks
- `renderMainPass` → `glDisable(GL_BLEND)` → re-enable before overlay passes
- `renderMainPass` used to have `glDisable(GL_DEPTH_TEST)` — **removed**

### Order Dependencies
- Outline pass MUST draw before main model (depth buffer)
- `syncMorphOffsets` must be called in `draw()` (not `update()`) — GL commands need active context

### Duplicate Handler Bug
- Multiple `if (key == GLFW_KEY_K)` handlers in the same switch cause toggle-then-untoggle
- Always check for duplicate key handlers when refactoring main.cpp

### Rotation Order
- Must be YXZ everywhere: addRigidBody, addJoint, PhysicsDebug buildPass
- Wrong order causes breast capsules to point sideways instead of front-back

### Collision Groups
- saba convention: pass raw `no_collision_group` (no inversion)
- PMX spec says it's "non-collision flags" (should be inverted), but models authored for saba
- Group-0 bodies don't collide with each other (by convention)

### Morph System
- Uses separate VAO with morph offset VBO (location 5 for position, 6 for UV)
- Morph shader variant must be selected (`hasActiveMorphs()` check)
- Weight 0 morphs stay in map but have no visual effect (filtered in updateMorphOffsets)

### Font/Warning
- C4819 (code page warning) → add `/utf-8` compile option
- LNK4098 (LIBCMT conflict) → harmless CRT warning, ignore

---

## 12. Build System Notes

- Root `CMakeLists.txt` builds `mmd` and `viewer` subdirectories
- `mmd.lib` links: BulletDynamics, BulletCollision, LinearMath, glfw, glad
- `viewer.exe` links: mmd
- Third-party: glfw (submodule), bullet (submodule), glad, stb_image (header-only)
- `MMD_PROJECT_ROOT` compile definition for runtime resource paths
- All source files in mmd/ and viewer/ flat — `.h` and `.cpp` side by side, no `include/`/`src/` separation

---

## 13. Input Keys Reference

| Key | Action |
|-----|--------|
| Mouse drag | Rotate camera |
| W/A/S/D | Move camera |
| E/Q | Up/down |
| Scroll | Adjust speed |
| X | Toggle world axis |
| G | Toggle ground grid |
| B | Toggle rigid body wireframe |
| H | Toggle model mesh |
| O | Toggle outline |
| T | Toggle toon shading |
| K | Toggle GPU skinning |
| P | Toggle VPD pose |
| Y | Toggle physics |
| F | Physics debug dump |
| R | Reset camera |
| I | Toggle idle blink |
| , / . | Switch morph |
| ↑ / ↓ | Adjust morph weight |
| Space | VMD play/pause |
| L | VMD loop toggle |
| [ / ] | VMD skip ±30 frames |
| Esc | Exit |
