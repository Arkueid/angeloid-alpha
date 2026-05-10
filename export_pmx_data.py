import sys
sys.path.insert(0, 'src')
from load_pmx import PmxModel
import struct

def read_pmx_binary(path):
    with open(path, 'rb') as f:
        data = f.read()
    return data

def compare_pmx_data(path):
    print(f"=== PMX 数据对比: {path} ===\n")

    with open(path, 'rb') as f:
        raw = f.read()

    pmx = PmxModel(path)

    print(f"文件大小: {len(raw)} bytes")
    print(f"模型名称: {pmx.name}")
    print(f"材质数量: {pmx.material_count}")
    print(f"纹理数量: {pmx.texture_count}")
    print(f"顶点数量: {pmx.vertex_count}")
    print(f"面数量: {pmx.face_count}")
    print(f"骨骼数量: {pmx.bone_count}")
    print(f"Morph数量: {pmx.morph_count}")
    print()

    print("=== 纹理列表 ===")
    for i, tex in enumerate(pmx.textures):
        print(f"  [{i}] {tex}")
    print()

    print("=== 材质详细数据 ===")
    for i, mat in enumerate(pmx.materials):
        print(f"  [{i}] {mat.name}")
        print(f"      texture_index: {mat.texture_index}")
        print(f"      toon_texture_index: {mat.toon_texture_index}")
        print(f"      sphere_texture_index: {mat.sphere_texture_index}")
        print(f"      sphere_mode: {mat.sphere_mode}")
        print(f"      alpha: {mat.alpha}")
        print(f"      edge_size: {mat.edge_size}")
        print(f"      flag: {mat.flag} (0b{bin(mat.flag)})")
        print(f"      diffuse_color: ({mat.diffuse_color.r}, {mat.diffuse_color.g}, {mat.diffuse_color.b})")
        print(f"      ambient_color: ({mat.ambient_color.r}, {mat.ambient_color.g}, {mat.ambient_color.b})")
        print(f"      specular_color: ({mat.specular_color.r}, {mat.specular_color.g}, {mat.specular_color.b})")
        print(f"      specular_factor: {mat.specular_factor}")
        print(f"      toon_sharing_flag: {mat.toon_sharing_flag}")
        print(f"      vertex_count: {mat.vertex_count}")
        print()

if __name__ == '__main__':
    compare_pmx_data('resources/models/Marine_Swmwear_1.01/Marine_Swmwear_1.01.pmx')