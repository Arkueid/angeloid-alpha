# Angeloid Alpha — Docs Index

## User-Facing
- [Build Guide](BUILD.md) — CMake build, viewer CLI, keyboard shortcuts

## Design & Architecture
- [Architecture Overview](architecture.md) — project structure, CMake, layers, key files, model registry
- [Refactoring Plan](ARCHITECTURE.md) — AssetStore, RenderContext design, migration steps
- [Pluggable Render Pipeline](pluggable-render-pipeline.md) — MME-style effect system: Pipeline/Slot/Effect/Pass design

## Subsystems
- [Physics Engine](physics.md) — Bullet world, shapes, joints, collision, body modes
- [Rendering Pipeline](rendering.md) — draw order, shaders, GL state, bone texture, model matrix
- [Animation System](animation.md) — VMD playback, VPD poses, idle blink, bone skinning
- [Morph System](morphs.md) — morph pipeline, types, material overrides, weight persistence

## Reference
- [PMX Format Notes](pmx-format.md) — shape sizes, joint types, bone flags, model-specific data
- [Model Reference](models.md) — ikaros-uniform/origin, 安比, aqua-sailor behavior differences
- [Debug Visualization](debug.md) — physics wireframes, world axis, console dump
- [Common Pitfalls](pitfalls.md) — GL state leaks, order dependencies, duplicate handlers
- [reference/](reference/) — MMD community specs (PMX format, joint constraints, with Chinese translations)
