---
name: rendering
description: Render passes, shaders, GL state, shadow map, bone texture, architecture
type: reference
---

## Render Passes (Pipeline::execute)

```
Frame:
  1. computeLightMatrix() — orthographic light VP matrix
  2. execute(proj, view):
     a. renderShadowPass()     — depth map (4096×4096)
     b. bindScreen()           — switch to default FBO
     c. Main pass              — ground + model (with shadow)
     d. Debug pass             — axis + physics overlay
```

## Renderables

Interface: `Renderable` (`framework/opengl/Renderable.h`)
- `onShadowPass(lightViewProj, model)` — depth-only
- `onMainPass(proj, view, model, lightViewProj, hasShadow)` — color + shadow
- `onDebugPass(proj, view, model)` — overlay (no shadow)

Registration order = draw order:
```cpp
pipe.addRenderable(&groundPlane);  // 1. ground (200×200 white quad)
pipe.addRenderable(&worldAxis);    // 2. axis + grid
pipe.addRenderable(&model);        // 3. PMX model (shadow caster + receiver)
```

## Shadow Map
- 4096×4096, `GL_DEPTH_COMPONENT32F`, no color attachment
- Orthographic: half-size 3.0, near=13, far=17, centered on origin
- Filter: `GL_LINEAR`, `GL_COMPARE_REF_TO_TEXTURE`, `GL_LEQUAL`
- Hardware 4-tap PCF (sampler2DShadow) in all shadow-receiving shaders (base/toon/ground)
- Slope-scale bias: `max(0.005 * (1 - NdotL), 0.001)` (base/toon); flat 0.002 (ground)
- Alpha-aware: `shadow_depth.frag` discards fragments with `u_alpha < 0.01`
  - Alpha = morph override (if active) else PMX material base alpha

## Shader Programs
All defined in `resources/effects.cfg`:
```
[shadow]    vert=shadow_depth.vert  frag=shadow_depth.frag
[outline]   vert=outline.vert       frag=outline.frag
[base]      vert=main.vert          frag=base.frag
[toon]      vert=main.vert          frag=toon.frag
[ground]    vert=ground.vert        frag=ground.frag
[axis]      vert=axis.vert          frag=axis.frag
[rigidbody] vert=rigidbody.vert     frag=rigidbody.frag
```
Loaded by `ShaderManager` singleton; each Renderable fetches its own shader via `ShaderManager::instance()`.

## GL State
| What | Where | Notes |
|------|-------|-------|
| `glFrontFace(GL_CW)` | `Pipeline::execute` start | PMX clockwise winding |
| `glEnable(GL_DEPTH_TEST)` | `Pipeline::execute` start | |
| `glDepthFunc(GL_LEQUAL)` | `Pipeline::execute` start | |
| `glEnable(GL_BLEND)` | `Pipeline::execute` start | `SRC_ALPHA, ONE_MINUS_SRC_ALPHA` |
| Shadow: `glDepthFunc(GL_LESS)` | `renderShadowPass` | saved/restored internally |
| Axis: `glEnable(GL_POLYGON_OFFSET_LINE)` | `WorldAxis::onDebugPass` | avoids z-fight with ground |
| Outline: `glCullFace(GL_FRONT)` | `renderMorphOutlinePass` | inverted hull |

## Bone Texture
- RGBA32F, 4 texels per matrix (column-major 4×4)
- Packed by `BoneSkinning::packBoneMatrices()`
- Created by `BoneTextureUtil::createBoneTexture()` (inline header)
- Uploaded by `ModelRenderer::uploadBoneData()` via `syncBoneTexture()`
- Shader reads via `texelFetch(u_boneTex, ivec2(col, row), 0)` — 4 consecutive texels

## GPU Backend (namespace Gpu)
`Vao`: VAO + VBOs + EBO, `render(mode)`, `destroy()`
`VboWrapper`: dynamic VBO, `write(data, bytes)`
`Texture`: 2D texture, `bind(unit)`, `write(data)`, `setFilter()`
`ShaderProgram`: GLSL program, `use()`, `setMat4()`, `setVec3()`, `setInt()`

## ModelRenderer VAOs
3 total: `mMorphVao`, `mMorphVaoNoToon`, `mMorphOutlineVao`. All include bone indices/weights + morph position/UV VBOs.

## Toon Shading
- Gradient 1D texture bound to GL_TEXTURE2
- Rim: `rim_power=4.0`, white, camera_pos from `Camera::instance().getEyePosition()`
- T key toggles between base.frag and toon.frag via `ModelRenderer::showToon`
