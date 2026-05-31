# Angeloid Alpha — Docs Index

## User-Facing
- [Python API Reference](PYTHON_API.md) — Model, Camera, module functions (full API)
- [Build Guide](BUILD.md) — pip / CMake build, C++ viewer CLI, keyboard shortcuts, tech details

## Design & Architecture
- [Architecture Overview](architecture.md) — project structure, CMake, layers, key files, model registry
- [Refactoring Plan](ARCHITECTURE.md) — AssetStore, RenderContext design, migration steps

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
