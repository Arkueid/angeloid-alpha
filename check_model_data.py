import sys
sys.path.insert(0, 'src')
from load_pmx import PmxModel
from PIL import Image
import numpy as np

model_path = 'resources/models/Marine_Swmwear_1.01/Marine_Swmwear_1.01.pmx'
pmx = PmxModel(model_path)

print('=' * 70)
print('PMX Model: Marine_Swmwear_1.01')
print('=' * 70)

print(f'\nTextures count: {len(pmx.textures)}')
print(f'Materials count: {len(pmx.materials)}')

print('\n' + '=' * 70)
print('All Toon Textures Analysis')
print('=' * 70)

toon_indices = set()
for mat in pmx.materials:
    if mat.toon_texture_index >= 0:
        toon_indices.add(mat.toon_texture_index)

for toon_idx in sorted(toon_indices):
    if toon_idx < len(pmx.textures):
        tex_path = 'resources/models/Marine_Swmwear_1.01/textures/' + pmx.textures[toon_idx].split('\\')[-1]
        try:
            img = Image.open(tex_path).convert('RGB')
            arr = np.array(img)

            unique_brightness = sorted(set(int(arr[y, x, :].mean()) for y in range(arr.shape[0]) for x in range(arr.shape[1])))

            print(f'\nToon {toon_idx}: {pmx.textures[toon_idx]}')
            print(f'  Size: {img.size}')
            print(f'  Unique brightness values: {unique_brightness}')
            print(f'  Min brightness: {min(unique_brightness)}, Max brightness: {max(unique_brightness)}')

            row_0_brightness = int(arr[0, :, :].mean())
            row_127_brightness = int(arr[127, :, :].mean())
            row_255_brightness = int(arr[255, :, :].mean())
            print(f'  Y-axis gradient: row0={row_0_brightness}, row127={row_127_brightness}, row255={row_255_brightness}')

            prev_b = None
            change_points = []
            for y in range(256):
                b = int(arr[y, 0, :].mean())
                if prev_b is not None and b != prev_b:
                    change_points.append((y, prev_b, b))
                prev_b = b
            if change_points:
                print(f'  Change points (y, from, to): {change_points[:5]}')
        except Exception as e:
            print(f'\nToon {toon_idx}: {pmx.textures[toon_idx]} - Error: {e}')

print('\n' + '=' * 70)
print('Material Classification')
print('=' * 70)

has_toon = []
no_toon = []
for i, mat in enumerate(pmx.materials):
    if mat.toon_texture_index >= 0:
        has_toon.append((i, mat.name, mat.toon_texture_index))
    else:
        no_toon.append((i, mat.name))

print(f'\nMaterials WITH toon ({len(has_toon)}):')
for idx, name, toon_idx in has_toon:
    print(f'  [{idx}] {name} -> toon_idx={toon_idx}')

print(f'\nMaterials WITHOUT toon ({len(no_toon)}):')
for idx, name in no_toon:
    print(f'  [{idx}] {name}')

print('\n' + '=' * 70)
print('Sphere Texture Analysis')
print('=' * 70)

sphere_info = {}
for i, mat in enumerate(pmx.materials):
    if mat.sphere_texture_index >= 0:
        sphere_info[mat.sphere_texture_index] = sphere_info.get(mat.sphere_texture_index, []) + [(i, mat.name, mat.sphere_mode)]

for sphere_idx in sorted(sphere_info.keys()):
    if sphere_idx < len(pmx.textures):
        tex_path = 'resources/models/Marine_Swmwear_1.01/textures/' + pmx.textures[sphere_idx].split('\\')[-1]
        try:
            img = Image.open(tex_path).convert('RGB')
            arr = np.array(img)
            unique_brightness = sorted(set(int(arr[y, x, :].mean()) for y in range(arr.shape[0]) for x in range(arr.shape[1])))
            print(f'\nSphere {sphere_idx}: {pmx.textures[sphere_idx]}')
            print(f'  Size: {img.size}, Unique brightness: {unique_brightness}')
            materials = sphere_info[sphere_idx][:3]
            print(f'  Used by: {[(m[0], m[1], f"mode={m[2]}") for m in materials]}')
        except Exception as e:
            print(f'\nSphere {sphere_idx}: {pmx.textures[sphere_idx]} - Error: {e}')