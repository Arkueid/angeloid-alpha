---
name: physics
description: Bullet physics world setup, shapes, joints, collision mask, body modes, bone feedback
type: reference
---

## World Setup
*File: `mmd/anim/PhysicsWorld.cpp`*
- Gravity: `-9.8 / modelScale` (~-99, matching saba's -98). Set per-model in `build()`.
- Solver: 15 iterations, ERP2=0.8, 10 substeps @ 240Hz, max step 1/30s
- Shape multipliers in `PhysicsWorld.h`: `kSphereShapeScale=0.9`, `kBoxShapeScale=0.9`, `kCapsuleShapeScale=0.9`

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
```cpp
// In getBoneTransforms():
boneNewPos = boneInitPos + (bodyPos - bodyInitPos);
boneNewRot = bodyDeltaRot * boneInitRot;
// Convert back: tx = boneNewPos.x() + center.x (no /s)
```

## Cloth-like Detection
Bodies with rotation springs + rotation limit range ≥ 0.01 → `clothLike = true`. Used by `debugTrackCloth()` (currently disabled).

## CCD
Dynamic bodies (mass>0) get CCD: swept sphere radius = min shape dim * shapeScale.

## Hair Joints
PMX data: rotation limits (0,0,0) for ponytail/hair chains. Rotation fully locked by model author. Bending comes from VMD bone animation keyframes, not physics.
