---
name: physics
description: Bullet physics world setup, shapes, joints, collision mask, body modes, bone feedback
type: reference
---

## World Setup
*File: `mmd/anim/PhysicsWorld.cpp`*
- Gravity: `-9.8 / modelScale` (~-99, matching saba's -98). Set per-model in `build()`.
- Solver: 15 iterations, ERP2=0.8, 10 substeps @ 240Hz, max step 1/30s
- Shape multipliers in `PhysicsWorld.h`: `kSphereShapeScale=1.0`, `kBoxShapeScale=1.0`, `kCapsuleShapeScale=1.0`
  - Changed from 0.9 (2026-05-31): shapes now match PMX-defined sizes and saba's reference implementation. The 0.9x scaling caused smaller collision volumes, contributing to skirt-leg clipping in models with tight-fitting clothing.

## PMX-native Space
All positions, shapes, joints, CCD at PMX-native scale. NO `modelScale` multiplication.
- Position: `rb.shape_position - center` (no `* s`)
- Shape: `size * kShapeScale` (no `* s`)
- Gravity auto-scaled: `-9.8 / modelScale`
- Why: original `* s (~0.1)` made shapes smaller than Bullet collision margin (0.04), causing precision loss.

## Rotation Order: YXZ
`Ry * Rx * Rz` = `qy * qx * qz`. Applied in: `addRigidBody()`, `addJoint()`, `PhysicsDebug buildPass()`. Matches saba/MMD. Original ZYX was wrong — breast capsules appeared sideways.

## Collision Mask
```cpp
mWorld->addRigidBody(body, 1 << collision_group, no_collision_group);
```
Raw `no_collision_group` passed directly (no `~` inversion). Matches saba convention. PMX spec says it's "non-collision flags" but models authored for saba behavior. Net effect: group-0 bodies don't collide with each other.

## setEquilibriumPoint()
NOT called. Matches saba. Springs use constraint-frame equilibrium.

## Joint Springs
`btGeneric6DofSpringConstraint` for types 0, 1, default. Tiered fallback for tight translation DOFs:
- `range < 0.001` → k=10000
- `range < 0.2` → k=2000
- `range < 0.5` → k=500
- Damping = 0.02

## Body Modes
| Mode | Mass | Bullet Flag | Behavior |
|------|------|-------------|----------|
| 0 | 0 | `CF_KINEMATIC_OBJECT` | Follows bone via `updateMode0Bodies()` every frame |
| 1 | PMX mass | Dynamic | Gravity + constraints |
| 2 | PMX mass | Dynamic | PID forces toward bone: `posErr*50 - vel*15` |

## Bone Feedback
Uses full matrix multiplication to correctly handle rotation-induced translation, exactly matching saba's `ReflectGlobalTransform`:

```cpp
// In getBoneTransforms():
// boneMat = bodyCurr · inv(bodyInit) · boneBind  (matrix multiply)
btTransform boneMat = bodyCurr * bb.invBodyInit * bb.boneBindMat;
// Mode 1: full position + rotation
// Mode 2: animation position + physics rotation only
```

`BulletBody` stores two precomputed btTransform at build time:
- `invBodyInit`: inverse of the body's initial world transform
- `boneBindMat`: the bone's bind-pose world transform (centered)

`computeBoneTarget()` (mode-0 kinematic bodies) uses the same formulation:
`animBone · inv(bindBone) · bodyInit`

**Why not vector addition?** The previous approach (`bonePos = initPos + displacement`) ignored body rotation. When gravity rotates a skirt panel, the pivot-to-COM vector rotates with it, but the simple displacement formula doesn't account for this. Verified by cross-comparison with saba: body positions matched but bone positions diverged by ~0.06 PMX units, causing ~0.06-0.07 vertex Y error at the skirt edge.

Memory cost: +128 bytes per body (2 btTransform × 64 bytes). For 134 bodies ≈ 17 KB total.
Perf cost: 3 matrix multiplies per active body per frame. ~21K float ops at 60fps. Negligible.

## Cloth-like Detection
Bodies with rotation springs + rotation limit range ≥ 0.01 → `clothLike = true`. Used by `debugTrackCloth()` (currently disabled).

## CCD
Dynamic bodies (mass>0) get CCD: swept sphere radius = min shape dim * shapeScale.

## Hair Joints
PMX data: rotation limits (0,0,0) for ponytail/hair chains. Rotation fully locked by model author. Bending comes from VMD bone animation keyframes, not physics.
