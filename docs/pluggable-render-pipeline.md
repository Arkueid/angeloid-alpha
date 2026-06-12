# Pluggable Render Pipeline

All shader programs are defined in `resources/core/effects/effects.cfg`. Pipeline reads this config at init, compiles everything once, and dispatches per frame.

## effects.cfg

```ini
# Swappable — T key toggles between these
[base]
vert = main.vert
frag = base.frag

[toon]
vert = main.vert
frag = toon.frag

# Built-in — rendering infrastructure
[outline]
vert = outline.vert
frag = outline.frag

[rigidbody]
vert = rigidbody.vert
frag = rigidbody.frag
```

Each `[section]` defines a complete vert+frag pair. Swappable sections (`[base]`, `[toon]`) are the rendering styles users toggle. Built-in sections (`[outline]`, `[rigidbody]`) are infrastructure — always the same.

## Pipeline

Singleton (`Pipeline::instance()`). Initialized once in `mmd::init()`:

```cpp
Pipeline::instance().init(effectsCfg, shaderDir);
```

- Reads effects.cfg via `parseCfgSections()`
- Compiles each section's vert+frag pair via `Gpu::ShaderProgram`
- Stores raw pointers indexed by section name
- `execute()` runs each frame: Outline → Main (base or toon) → Rigidbody debug

## Per-frame

```cpp
Pipeline::FrameParams fp;
fp.proj    = &proj;
fp.view    = &view;
fp.modelMat = mRenderer.modelMatrix();
fp.camPosX  = camPos[0];
fp.camPosY  = camPos[1];
fp.camPosZ  = camPos[2];
fp.showToon = mRenderer.showToon;        // T key
fp.showRigidBodies = mShowRigidBodies;   // B key

Pipeline::instance().execute(mRenderer, fp);
```

`execute()` does:
1. GL state baseline (CW face, depth, blend)
2. Outline pass (always outline.vert + outline.frag)
3. Main pass (main.vert + base.frag or toon.frag depending on showToon)
4. Physics debug overlay (rigidbody.vert + rigidbody.frag)

## Adding a new rendering style

1. Write a GLSL fragment shader (e.g. `pbr.frag`)
2. Add a section to `resources/core/effects/effects.cfg`:
   ```ini
   [pbr]
   vert = main.vert
   frag = pbr.frag
   ```
3. Bind the section to toggle key or always-use in `Pipeline::execute()` — or expose it to the user

No C++ recompilation needed.

## Architecture

```
mmd::init()
├── RenderContext::init(toonDir)              // gradient + shared toon textures
└── Pipeline::instance().init(cfg, shaderDir) // compile all shader programs

Model::draw()
└── Pipeline::instance().execute(renderer, p) // outline → main → debug
```

Pipeline owns all `Gpu::ShaderProgram` objects via `std::vector<std::unique_ptr<>> mPrograms`. Per-frame execution uses raw pointers into this vector — no allocation, no hash lookup.

RenderContext owns GPU textures (gradient, shared toons). Bone texture lives in ModelRenderer (per-model).
