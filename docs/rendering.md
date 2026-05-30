---
name: rendering
description: Draw order, shader variants, GL state management, bone texture, model matrix
type: reference
---

## Draw Order (CRITICAL)
```
1. syncMorphOffsets() + material overrides
2. OUTLINE first (front-face cull + extrusion needs empty depth buffer)
3. MAIN MODEL second (fills depth, occludes outline interior)
4. Physics debug overlay last
```

## Shader Variants
| Morphs | Skinned | Toon | Main Shader | Outline Shader |
|--------|---------|------|-------------|----------------|
| No | No | No | "main" | "outline" |
| No | No | Yes | "toon" | "outline" |
| No | Yes | No | "skinned_notoon" | "outline_skinned" |
| No | Yes | Yes | "skinned" | "outline_skinned" |
| Yes | Yes | No | "morph_notoon" | "morph_outline" |
| Yes | Yes | Yes | "morph" | "morph_outline" |

Selection: `bool useMorph = mMorphCtl.hasActiveMorphs();`

## GPU Model Matrix
VBO stores raw PMX coordinates. `modelMat` = `{s,0,0,0, 0,s,0,0, 0,0,-s,0, -cx*s, -my*s, cz*s, 1}` (column-major Z-flip + scale + translate). Skinning matrices are pure `world * inv(bind)` (no scale/offset baked in).

## GL State
| What | Where | Notes |
|------|-------|-------|
| `glFrontFace(GL_CW)` | `onRender` start | PMX clockwise winding |
| `glEnable(GL_DEPTH_TEST)` | `onRender` start | |
| `glEnable(GL_BLEND)` | `onRender` start | `SRC_ALPHA, ONE_MINUS_SRC_ALPHA` |
| `glDisable(GL_BLEND)` | `renderMainPass` end | **LEAKS** — re-enable before debug overlays |
| `glDisable(GL_DEPTH_TEST)` | **REMOVED** from renderMainPass | Was breaking debug overlay depth |

Physics debug must re-enable: `glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LEQUAL);`

## Bone Texture
- RGBA32F, 4 texels per matrix (column-major 4x4)
- Packed by `BoneSkinning::packBoneMatrices()`
- Created by `BoneTextureUtil::createBoneTexture()` (inline header)
- Shader reads via `texelFetch(bone_texture, ivec2(col, row), 0)` — 4 consecutive texels

## GPU Backend (namespace Gpu)
`Vao`: VAO + VBOs + EBO, `render(mode)`, `destroy()`
`VboWrapper`: dynamic VBO, `write(data, bytes)`
`Texture`: 2D texture, `bind(unit)`, `write(data)`, `setFilter()`
`ShaderProgram`: GLSL program, `use()`, `setMat4()`, `setVec3()`, `setInt()`

## ModelRenderer VAOs
9 total: `mModelVao`, `mToonVao`, `mOutlineVao` (static); `mSkinnedVao`, `mSkinnedVaoNoToon`, `mSkinnedOutlineVao` (skinned); `mMorphVao`, `mMorphVaoNoToon`, `mMorphOutlineVao` (skinned+morph).

## Toon Shading
- Gradient 1D texture bound to GL_TEXTURE1/2
- Rim: `rim_power=4.0`, white, camera_pos uniform
