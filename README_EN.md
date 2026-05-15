# Angeloid Alpha

[中文版](README.md)

> Master... this is a program for rendering PMX models. I will do my best to serve you.

## Overview

A PMX model renderer with GPU skeletal animation and VMD animation playback support.

## Features

- ✅ PMX model loading and rendering
- ✅ GPU skeletal skinning animation
- ✅ VPD pose loading
- ✅ VMD animation playback (bone + morph)
- ✅ Multiple VMD mixing
- ✅ Morph expression system (Vertex/UV/Bone/Material/Group)
- ✅ Toon shading
- ✅ Edge outline
- ✅ Rigidbody/Joint visualization
- ✅ Multiple model switching
- ✅ FPS display

## Project Structure

```
mmd-demo/
├── main.py
├── src/
│   ├── gpu/                  # GPU resource wrappers
│   │   ├── mesh.py           # VAO, VBOWrapper
│   │   └── texture.py        # Texture
│   ├── pmx_model.py          # PMX data model + parser
│   ├── vpd_loader.py         # VPD pose loader
│   ├── vmd_player.py         # VMD loader + mixer
│   ├── bone_math.py          # Bone matrices + physics mesh
│   ├── renderer.py           # Main renderer
│   ├── animation_controller.py
│   ├── morph_controller.py
│   ├── shader_manager.py
│   └── camera.py
└── resources/
    ├── shaders/
    ├── models/
    ├── toon/
    ├── vpd/
    └── motions/
```

## Usage

```bash
python main.py -m <model-name>
python main.py -m marine-swimwear -v resources/motions/xxx.vmd
```

### Keyboard Shortcuts

| Key | Function |
|-----|----------|
| Left mouse drag | Rotate view |
| W/A/S/D | Move forward/left/back/right |
| E/Q | Move up/down |
| Mouse scroll | Adjust speed |
| X | Toggle world axis |
| G | Toggle ground grid |
| B | Toggle rigidbody & joint |
| H | Toggle model mesh |
| O | Toggle outline |
| T | Toggle toon shading |
| K | Toggle GPU skinning |
| P | Toggle VPD pose |
| R | Reset camera |
| I | Toggle idle animation |
| M | Toggle morph mode |
| , / . | Switch morph |
| ↑ / ↓ | Adjust morph weight |
| Space | Play/Pause VMD |
| L | Toggle VMD loop |
| [ / ] | Step VMD ±30 frames |

## Technical Details

### GPU Skinning

Texture-driven: bone matrices packed into RGBA32F texture, shader samples via `texelFetch`.

### Bone Hierarchy

PMX stores absolute bone positions. Compute local = child - parent, accumulate hierarchically, final matrix = world × inv_bind.

### Toon Shading

Gradient texture mapping based on `N·L` dot product with rim lighting.

### Edge Outline

Two-pass: render front faces to depth, extrude back faces along normals.

### Rigidbody/Joint Visualization

Wireframe rendering of physics collision bodies, animated via the same bone texture as the mesh.

## Dependencies

- [PyOpenGL](https://pyopengl.sourceforge.net/)
- [glfw](https://www.glfw.org/)
- [numpy](https://numpy.org/)
- [Pillow](https://python-pillow.org/)
- [pymeshio](https://github.com/ousttrue/pymeshio)

## License

MIT License
