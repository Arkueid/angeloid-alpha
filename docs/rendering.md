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

Interface: `Renderable` (`framework/Renderable.h`)
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

## GL State (all routed through Gpu::device())
| State | Where | Notes |
|------|-------|-------|
| `setFrontFace(true)` (CW) | `Pipeline::execute` start | PMX clockwise winding |
| `setDepthTest(true)` | `Pipeline::execute` start | |
| `setDepthFunc(LEqual)` | `Pipeline::execute` start | |
| `setBlend(true)`, `SrcAlpha/OneMinusSrcAlpha` | `Pipeline::execute` start | |
| Shadow: `setDepthFunc(Less)` | `renderShadowPass` | saved/restored internally |
| Axis: `setPolygonOffset(-1,-1)` | `WorldAxis::onDebugPass` | avoids z-fight with ground |
| Outline: `setCullMode(Front)` | `renderMorphOutlinePass` | inverted hull |

## Bone Texture
- RGBA32F, 4 texels per matrix (column-major 4×4)
- Packed by `BoneSkinning::packBoneMatrices()`
- Created inline in `ModelRenderer.cpp` via `Gpu::device()->createTexture()`
- Uploaded by `ModelRenderer::uploadBoneData()` via `mBoneTexture->write()`
- Shader reads via `texelFetch(u_boneTex, ivec2(col, row), 0)` — 4 consecutive texels

## GPU Backend (namespace Gpu)
Abstract interfaces in `framework/gpu/`, two backends:
- `gpu/opengl/` — OpenGL implementation (GlDevice, GlTexture, etc.)
- `gpu/vulkan/` — Vulkan implementation (VulkanDevice, VkBuffer, etc.)

`IGpuDevice` — central device: resource creation, state, draw calls, `beginFrame()`/`endFrame()`, `needsDepthCorrection()`
`IGpuTexture` — 2D texture, `bind(unit)`, `write(data)`, `setFilter()`, `setMirrorWrap()`
`IGpuShader` — shader program, `use()`, `setMat4()`, `setVec3()`, `setInt()`
`IGpuVertexArray` — VAO, `draw(prim, count, first)`
`IGpuRenderTarget` — FBO, `bind()`, `colorTexture()`, `depthTexture()`

### Vulkan Backend Details
- **Dynamic Rendering** (`VK_KHR_dynamic_rendering`): avoids explicit render pass/framebuffer objects
- **SPIR-V Compilation**: shaderc (in-process, ~68ms for all 14 modules), Vulkan-specific GLSL with uniform blocks + `layout(binding=N, location=N)`
- **Pipeline Cache**: lazy creation keyed by (shader, VAO format, blend/depth/cull/polygon state)
- **Descriptor Sets**: binding 0 = UBO (uniform buffer, scalar layout), bindings 1-6 = combined image samplers
- **Depth Correction**: OpenGL NDC Z [-1,1] → Vulkan NDC Z [0,1] via projection matrix row transform
- **Y Convention**: viewport Y-flip (`height = -H, y = H`) to match OpenGL's Y-up screen mapping

## ModelRenderer VAOs
Single morph VAO (`mMorphVao`) with bone indices/weights + morph position/UV VBOs.
Toon/no-toon/outline all share the same VAO; the shader selection determines the look.

## Toon Shading
- Gradient 1D texture bound to GL_TEXTURE2
- Rim: `rim_power=4.0`, white, camera_pos from `Camera::instance().getEyePosition()`
- T key toggles between base.frag and toon.frag via `ModelRenderer::showToon`

## Quaternion-to-Matrix Convention

Bone rotation matrices use a **transposed** quaternion-to-matrix formula compared to the
standard textbook derivation. This is intentional and matches the MMD coordinate system
convention used by PMX and VMD data.

The correct formula (used in `BoneSkinning.cpp:applyVmdTransform` and `applyBoneMorphs`):

```
// r[9] row-major array = {R00, R01, R02, R10, R11, R12, R20, R21, R22}
float r[9] = {1-yy-zz,  xy+wz,   xz-wy,
              xy-wz,    1-xx-zz,  yz+wx,
              xz+wy,    yz-wx,    1-xx-yy};
```

Stored into column-major `Mat4` as:
```
local[0]=r[0], local[4]=r[3], local[8]=r[6],   // column 0 → rotation row 0
local[1]=r[1], local[5]=r[4], local[9]=r[7],   // column 1 → rotation row 1
local[2]=r[2], local[6]=r[5], local[10]=r[8];  // column 2 → rotation row 2
```

**Do not** replace with the standard formula (`xy-wz` on column 0, `xz+wy` on column 0, etc.) —
that will produce incorrect bone rotations for MMD models. The sign pattern
`(+,+,- / -,-,+ / +,-,-)` on off-diagonals is the MMD convention.

If extracting a shared helper, copy the original layout verbatim rather than
re-deriving from a textbook reference.
