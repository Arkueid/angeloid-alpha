---
name: pitfalls
description: Common bugs, GL state leaks, order dependencies, lessons learned
type: reference
---

## Initialization Order
1. **Window before model**: GL context must exist before `Model::load()` — it creates VAOs/textures via GL calls.
2. **setupSkinning in load()**: must be called to initialize morph VBOs. Without it, ALL morph operations silently fail (VBO writes go to null).
3. **VmdPlayer mPlaying**: default was `false` → VMD never advanced frames. Constructor now sets `true`.

## GL State
- `Pipeline::execute()` sets baseline state at start: `GL_CW`, depth+blend enabled
- Outline pass enables/disables `GL_CULL_FACE(GL_FRONT)` internally — cleans up before returning
- `glDisable(GL_BLEND)` leak from old `renderMainPass` no longer exists (state set once in Pipeline)

## Draw Order
Outline MUST draw before main model. Outline uses front-face culling + vertex extrusion; depth buffer must be empty for the shell to render correctly.

## Toggle Logic
- **VPD apply**: must recompute `poseWorld` + call `resetPhysics`. Just flipping `mVpdApplied` flag does nothing.
- **Physics enable**: just sets `mPhysics.enabled`. `updateMode0Bodies` runs regardless for debug display.
- **K key (skinning)**: non-skinned model is always T-pose. Rigid bodies use bind-pose when K is OFF.

## Duplicate Handler Bug
When refactoring main.cpp, duplicate `if (key == GLFW_KEY_X)` blocks can appear. Each runs, causing toggle-then-untoggle. Always check for duplicate key handlers.

## Sync Issues
- `syncBoneTexture()` must use `mPoseWorld` (which has physics applied), not recompute from VPD poses
- `syncMorphOffsets()` must be called in `draw()` (needs active GL context for VBO write)
- `applyVpd()` must remove `getBoneTransforms()` call (overwrites VPD pose with physics body data)

## Compiler Warnings
- C4819 (code page): add `/utf-8` to MSVC compile options
- LNK4098 (LIBCMT conflict): harmless CRT warning, ignore
