---
name: morphs
description: Morph system pipeline, types, material overrides, weight persistence, key fixes
type: reference
---

## Pipeline
```
setMorphWeight(name, w) → mMorphWeights[name] = w → updateMorphOffsets()
  → clear offsets → iterate weights → find morph in model.morphs → accumulate offsets*w
syncMorphOffsets() [Model] → idle blink injection → updateMorphOffsets() again
  → morphVbo->write(posOffsets) → uvMorphVbo->write(uvOffsets)
draw() → hasActiveMorphs() ? morph shader : skinned shader
  → vertex shader: morphed_pos = in_position + offset * morph_weight
```

## Morph Types (PMX)
| Type | Enum | Description |
|------|------|-------------|
| Group | 0 | Recursively applies child morphs × rate |
| Vertex | 1 | Per-vertex position offset |
| Bone | 2 | Bone transform (position + quaternion) |
| UV | 3 | UV offset |
| UV_EXT1-4 | 4-7 | Additional UV layers |
| Material | 8 | Diffuse/Specular/Ambient/Edge/Texture multipliers |
| Flip | 9 | Selects one morph by index |
| Impulse | 10 | Physics impulse on rigid body |

## Material Override
`morphCtl.getMaterialOverride(matIndex)` → `MatMorphOverride` (alpha, diffuse, specular, ambient). Applied as multipliers.

## Key Fixes
1. **setupSkinning() in load()** — initializes morph VBOs. Without it, all morph writes go to null.
2. **Morph shader selection** — `hasActiveMorphs()` gates shader choice. Without it, offsets never consumed.
3. **Weight persistence** — `savedWeights` map in main.cpp restores per-morph weights when switching with comma/period.

## Idle Blink Integration
- Directly modifies `morphWeights()["まばたき"] = w` then calls `updateMorphOffsets()`
- VBO write in `syncMorphOffsets()` applies blink offsets to GPU
