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
int trackId = mixer->addVmd(anim.get());
mixer->play(trackId);
// Per frame: mixer->update(dt);
```

**Multi-track**: `VmdMixer::addVmd()` returns a track ID. Each track independently controlled via `play(trackId)`, `pause(trackId)`, `stop(trackId)`, `setFrame(trackId, frame)`. Use `playAll()` / `pauseAll()` / `stopAll()` for batch control.

**Interpolation**: Bezier curve interpolation for bone position/rotation. Linear interpolation for morph weights.

**Callbacks**: `play(trackId, onEnd)` accepts `std::function<void(int)>` — invoked when VMD reaches end (non-looping). Default loop is off; re-play via `stopAll()` + `playAll()`.

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
