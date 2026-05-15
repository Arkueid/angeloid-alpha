# Project: MMD PMX Viewer (angeloid-alpha)

## Run
```bash
python main.py -m <model-name>
```
Python environment: `.venv/`

## Project structure
- `src/` — pure library code, no hardcoded resource paths
- `main.py` — entry point, model paths dict, CLI args
- `resources/` — models, textures, toon, vpd, motions
- `src/gpu/` — GPU resource wrappers (VAO, Texture)

## Code conventions
- File naming: underscore for multi-word (`bone_math.py`, `vmd_player.py`)
- Format loaders: `_loader` suffix for single-format files, descriptive name for multi-role files (`pmx_model.py`)
- OpenGL matrices: always pass `.T` to `glUniformMatrix4fv`
- GPU objects go in `src/gpu/`; geometry generation stays in `src/`
- Prefer deduplication: extract helpers over repeated patterns
- No comments for obvious code; only comment non-obvious WHY

## Git
- Commit messages: concise summary line, bullet details
- Use `Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>`
- Keep commits focused, don't mix unrelated changes

## Problem solving
- Don't work around issues — fix root causes
- Debug with data: add prints to verify assumptions before changing logic
- Don't assume PMX data is wrong; verify the transform chain first
