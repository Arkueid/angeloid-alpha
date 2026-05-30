---
name: pmx-format
description: PMX file format notes — shapes, joints, bone flags, model-specific data
type: reference
---

## Shape Sizes
| PMX Shape | size meaning | Bullet param | Code |
|-----------|-------------|--------------|------|
| Sphere (0) | `size.x` = radius | `btSphereShape(r)` | `r = size.x * kSphereShapeScale` |
| Box (1) | `size = (w, h, d)` | `btBoxShape(half)` | `half = size * kBoxShapeScale` |
| Capsule (2) | `size.x` = radius, `size.y` = height | `btCapsuleShape(r, h)` | `r = size.x * kCapsuleShapeScale, h = size.y * kCapsuleShapeScale` |

History: originally all `*0.5` (wrong per spec for sphere/capsule, wrong visually). Compromise at `*0.9`.

## Joint Types (PMX 2.1)
| Type | Name | Bullet Constraint |
|------|------|-------------------|
| 0 | Spring 6DOF | `btGeneric6DofSpringConstraint` |
| 1 | 6DOF | same, springs disabled |
| 2 | P2P | `btPoint2PointConstraint` |
| 3 | ConeTwist | `btConeTwistConstraint` |
| 4 | Slider | `btSliderConstraint` |
| 5 | Hinge | `btHingeConstraint` |

Each joint has: `rigidbody_index_a/b`, `position`, `rotation` (radians), `translation_limit_min/max`, `rotation_limit_min/max` (radians), `spring_constant_translation/rotation`.

## Rigid Body Modes
| Mode | Name | Bullet |
|------|------|--------|
| 0 | Bone Follow (Static) | Kinematic, mass=0 |
| 1 | Dynamic | Full mass + gravity |
| 2 | Dynamic + Bone Merge | Dynamic + PID toward bone |

Each body has: `bone_index` (-1=none), `collision_group`, `no_collision_group`, `shape_type`, `shape_size`, `shape_position`, `shape_rotation` (radians), `mass`, `linear_damping`, `angular_damping`, `restitution`, `friction`, `mode`.

## Bone Flags (key ones)
| Flag | Value | Meaning |
|------|-------|---------|
| `TAILPOS_IS_BONE` | 0x0001 | Connection target is bone index |
| `ROTATABLE` | 0x0002 | User can rotate |
| `MOVABLE` | 0x0004 | User can translate |
| `IK` | 0x0020 | IK chain |
| `ROTATION_GRANT` | 0x0100 | Rotation grant to children |
| `AFTER_PHYSICS` | 0x1000 | Recompute after physics step |

## Encoding
PMX text encoding: UTF-16LE (mode 0) or UTF-8 (mode 1). Header byte[0] specifies.
`mmd/encoding/Encoding.cpp` handles CP932 ↔ UTF-8 on Windows.
