---
name: animation
description: VMD playback, VPD poses, idle blink, bone skinning matrix pipeline
type: reference
---

## VMD Playback
*Files: `mmd/anim/VmdPlayer.h/.cpp`*

```cpp
auto anim = VmdAnimation::load(path);
auto mixer = std::make_unique<VmdMixer>();
mixer->addVmd(std::move(anim));
mixer->play();
// Per frame: mixer->update(dt);
```

**Critical**: `VmdPlayer` constructor must set `mPlaying = true`. Default was `false` — VMD would never advance.

**Multi-layer**: `VmdMixer` holds multiple `VmdPlayer`. Each frame: all players update, bone transforms from first match returned. Morph weights summed across players (clamped 0-1).

**Interpolation**: Bezier curve interpolation for bone position/rotation. Linear interpolation for morph weights.

## VPD Pose
*File: `mmd/anim/VpdLoader.h/.cpp`*
- Static pose file (bone rotations + translations)
- `VpdLoader::load(path)` → `unordered_map<string, VpdPose>`
- P key: apply/revert VPD → recomputes `poseWorld`, calls `resetPhysics`, syncs bone texture
- **Must recompute poseWorld** — just toggling `mVpdApplied` flag does nothing

## Idle Blink
- Every 4 seconds, 0.15s triangle-wave blink
- Morph: `まばたき` (also tries `blink`, `blink_l`, etc.)
- Weight injected into `morphWeights()` map, then `updateMorphOffsets()` + VBO write
- Disabled during VMD playback
- `mIdleTime` accumulated in `Model::update(dt)`

## Bone Skinning Pipeline
```
PMX bone hierarchy
  → computeBindWorldMatrices(model)         // T-pose world matrices
  → computePoseWorldMatrices(model, vpd)    // VPD-modified
  → computePoseWorldMatrices(model, vpd, vmd)  // VPD + VMD
  → physics: getBoneTransforms(poseWorld)   // add physics deltas
  → recomputeAfterPhysicsBones             // for AFTER_PHYSICS bones
  → computeSkinningMatrices(model, poseWorld)  // world * inv(bind)
  → packBoneMatrices → RGBA32F texture
  → GPU: skin_matrix * in_position
```

## Key Functions
| Function | Input | Output |
|----------|-------|--------|
| `computeBindWorldMatrices` | PmxModel | bind world mats |
| `computePoseWorldMatrices` | PmxModel + VpdPoses | pose world mats |
| `computeSkinningMatrices` | PmxModel + poseWorld | world * inv(bind) |
| `packBoneMatrices` | float[16]*n | RGBA texel array |
