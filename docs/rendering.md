---
name: rendering
description: Draw order, shaders, GL state, bone texture, model matrix
type: reference
---

## Draw Order
```
1. syncMorphOffsets() + material overrides
2. OUTLINE first (front-face cull + extrusion)
3. MAIN MODEL second (fills depth, occludes outline interior)
4. Physics debug overlay last
```

## Rendering Path
Always GPU-skinning (skinned_morph.vert). T key toggles between base.frag (diffuse) and toon.frag (cel-shading).

```
Pipeline::execute()
  ├─ Outline: outline.vert + outline.frag
  ├─ Main:    main.vert + base.frag   or  main.vert + toon.frag
  └─ Debug:   rigidbody.vert + rigidbody.frag
```

## Shader Programs
All defined in `resources/effects.cfg`. Pipeline reads the config and compiles programs once at init.

```
effects.cfg:
  [base]      vert=main.vert      frag=base.frag
  [toon]      vert=main.vert      frag=toon.frag
  [outline]   vert=outline.vert   frag=outline.frag
  [rigidbody] vert=rigidbody.vert frag=rigidbody.frag
```

## GPU Model Matrix
VBO stores raw PMX coordinates. `modelMat` = `{s,0,0,0, 0,s,0,0, 0,0,-s,0, -cx*s, -my*s, cz*s, 1}` (column-major Z-flip + scale + translate). Skinning matrices are pure `world * inv(bind)` (no scale/offset baked in).

## GL State
| What | Where | Notes |
|------|-------|-------|
| `glFrontFace(GL_CW)` | `Pipeline::execute` start | PMX clockwise winding |
| `glEnable(GL_DEPTH_TEST)` | `Pipeline::execute` start | |
| `glEnable(GL_BLEND)` | `Pipeline::execute` start | `SRC_ALPHA, ONE_MINUS_SRC_ALPHA` |
| outline: `glCullFace(GL_FRONT)` | `renderMorphOutlinePass` | inverted hull technique |
| outline: `glDisable(GL_CULL_FACE)` | `renderMorphOutlinePass` end | cleans up |

## Bone Texture
- RGBA32F, 4 texels per matrix (column-major 4x4)
- Packed by `BoneSkinning::packBoneMatrices()`
- Created by `BoneTextureUtil::createBoneTexture()` (inline header)
- Uploaded by `ModelRenderer::uploadBoneData()`
- Shader reads via `texelFetch(bone_texture, ivec2(col, row), 0)` — 4 consecutive texels

## GPU Backend (namespace Gpu)
`Vao`: VAO + VBOs + EBO, `render(mode)`, `destroy()`
`VboWrapper`: dynamic VBO, `write(data, bytes)`
`Texture`: 2D texture, `bind(unit)`, `write(data)`, `setFilter()`
`ShaderProgram`: GLSL program, `use()`, `setMat4()`, `setVec3()`, `setInt()`

## ModelRenderer VAOs
3 total: `mMorphVao`, `mMorphVaoNoToon`, `mMorphOutlineVao`. All include bone indices/weights + morph position/UV VBOs.

## Toon Shading
- Gradient 1D texture bound to GL_TEXTURE2
- Rim: `rim_power=4.0`, white, camera_pos uniform
- T key toggles between base.frag and toon.frag via Pipeline
