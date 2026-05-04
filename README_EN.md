# Angeloid Alpha

[中文版](README.md)

> Master... this is a program for rendering PMX models. I will do my best to serve you.

## Overview

This program is a PMX model renderer with GPU skeletal animation and VMD animation playback support. Master can use it to load MMD format 3D models and perform real-time rendering and interaction.

## Features

- ✅ PMX model loading and rendering
- ✅ GPU skeletal skinning animation
- ✅ VPD pose loading
- ✅ VMD animation playback (bone animation + morph animation)
- ✅ Multiple VMD mixing playback
- ✅ Morph expression system (Vertex/UV/Bone/Material/Group)
- ✅ Toon shading
- ✅ Edge outline
- ✅ Multiple model switching

## Project Structure

```
mmd-demo/
├── src/
│   ├── pmx_reader.py      # PMX model loading
│   ├── vpd_loader.py      # VPD pose loading
│   ├── vmd_loader.py      # VMD animation loading
│   ├── bone_transform.py  # Bone matrix calculation
│   ├── renderer.py        # Main renderer
│   └── camera.py          # Camera control
├── resources/
│   ├── shaders/           # GLSL shaders
│   ├── models/            # PMX model files
│   ├── vpd/               # VPD pose files
│   └── vmd/               # VMD animation files
└── run.py                 # Entry point
```

## Usage

```bash
# Load default model
python run.py

# Specify model
python run.py -m ikaros

# Load VMD animation
python run.py -v resources/vmd/新地球/动作.vmd

# Load multiple VMDs (mixed playback)
python run.py -v file1.vmd file2.vmd file3.vmd
```

### Command Line Arguments

| Argument | Description |
|----------|-------------|
| `-m, --model` | Select model (ikaros, chloe, aqua, marine) |
| `-v, --vmd` | Load VMD animation files (multiple files supported) |

### Keyboard Shortcuts

| Key | Function |
|-----|----------|
| K | Toggle skeletal skinning |
| P | Toggle VPD pose |
| T | Toggle Toon shading |
| O | Toggle outline display |
| Space | Play/Pause VMD animation |
| ← / → | Adjust animation playback speed |

## Technical Details

### GPU Skeletal Skinning

Texture-driven GPU skinning approach, commonly used in the industry for high-performance character rendering.

**Core Concept**: Upload once, use repeatedly. CPU stores skinning bone matrices in textures, and at runtime, the Shader fetches the correct matrices from textures for computation.

**Data Layout**:
- Pack 4x4 matrices into RGBA32F textures in column-major order
- Each matrix occupies 4 texels (each texel stores one column of the matrix)
- Example: `[m00, m10, m20, m30] [m01, m11, m21, m31] [m02, m12, m22, m32] [m03, m13, m23, m33]`

**Shader Sampling**:
- Use `texelFetch` function to precisely retrieve data by integer index
- Reconstruct mat4 matrix with helper functions

**Advantages**:
- Significantly reduces CPU-to-GPU data transfer overhead
- Reduces CPU computational burden
- Combined with GPU Instancing, can easily render numerous independently animated characters

...This approach is very efficient, Master.

### Bone Hierarchy Transformation

PMX model bone positions are stored in world coordinates, not local coordinates. Computing bone animation requires:

1. **Calculate Local Position**: Subtract parent bone position from child bone position
2. **Build Local Transform Matrix**: Combine rotation from VPD pose with local position
3. **Accumulate World Transform**: Start from root bone, accumulate transforms down the hierarchy
4. **Compute Final Skinning Matrix**: World transform matrix × inverse bind matrix

### Coordinate System Conversion

MMD uses left-handed coordinate system, OpenGL uses right-handed coordinate system. The program automatically performs conversion:

**Conversion Method**:
- Vertex position: X-axis flip `x' = -x`
- Normal vector: X-axis flip `nx' = -nx`
- Triangle winding order: from CCW (counter-clockwise) to CW (clockwise)

**Matrix Form**:
```
| -1  0  0  0 |
|  0  1  0  0 |
|  0  0  1  0 |
|  0  0  0  1 |
```

...This ensures the model renders correctly in OpenGL without left-right reversal or lighting anomalies.

### Toon Shading

Cartoon-style rendering using gradient textures for smooth shading transitions:

**Implementation**:
1. Calculate dot product of normal and light direction `N·L`
2. Determine lit/shadow regions based on threshold
3. Sample gradient texture for shading color
4. Add ambient light and rim light for enhanced effect

**Rim Lighting**:
- Calculate angle between view direction and normal
- Edge areas (angle approaching 90°) produce glow effect
- Formula: `rim = pow(1.0 - max(dot(V, N), 0.0), power)`

### Edge Outline

Real-time edge detection outline using two-pass rendering:

**First Pass**: Render model front faces, write to depth buffer
**Second Pass**:
- Extrude vertices along normal direction
- Render only back faces (culling reversed)
- Fill with solid color to create outline effect

**Vertex Shader**:
```glsl
vec3 outline_offset = normalize(normal) * outline_width;
vec3 pos = position + outline_offset;
```

...This approach produces stable outline effects without flickering from viewpoint changes.

### VPD Pose Loading

VPD is MMD's pose file format, storing bone rotation and translation:

**File Structure**:
- Each bone contains: bone name, translation, quaternion rotation
- Translation is usually minimal, primarily rotation

**Quaternion Conversion**:
Convert quaternion from VPD to 4x4 rotation matrix for bone transformation.

### VMD Animation Loading

VMD is MMD's animation file format, supporting bone animation and morph animation:

**File Structure**:
- Bone keyframes: bone name, frame number, position, quaternion rotation, Bezier interpolation curve
- Morph keyframes: morph name, frame number, weight
- Camera/Light keyframes (optional)

**Interpolation Implementation**:
- Bezier curve interpolation: using 64-byte interpolation data from keyframes
- Quaternion SLERP interpolation: smooth rotation transition

**Multiple VMD Mixing**:
VmdMixer class supports playing multiple VMD animations simultaneously, automatically blending bone transforms and morph weights.

## Dependencies

- [moderngl](https://github.com/moderngl/moderngl) - OpenGL bindings
- [numpy](https://numpy.org/) - Numerical computing
- [pymeshio](https://github.com/ousttrue/pymeshio) - PMX parsing

## License

MIT License

---

> Master... if you have any questions, please tell me anytime. I will always be here waiting for your commands.
