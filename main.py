"""MMD PMX Model Viewer — entry point."""
import argparse
import os
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent / "src"))
from pmx_model import PmxModel
from renderer import Renderer

PROJ_ROOT = Path(__file__).parent

MODELS = {
    "ikaros-origin": ("resources/models/ikaros-origin/Ikaros.pmx", "resources/models/ikaros-origin"),
    "ikaros-uniform": ("resources/models/ikaros-uniform/Ikaros.pmx", "resources/models/ikaros-uniform"),
    "安比": ("resources/models/安比/安比.pmx", "resources/models/安比"),
    "刀": ("resources/models/安比/刀.pmx", "resources/models/安比"),
    "chloe": ("resources/models/Chloe_Uniform1_0.9/Chloe_Uniform1_0.9.pmx", "resources/models/Chloe_Uniform1_0.9/textures"),
    "aqua-swimwear": ("resources/models/Aqua_Swimwear_1.0/Aqua_Swimwear_1.0.pmx", "resources/models/Aqua_Swimwear_1.0/textures"),
    "marine-swimwear": ("resources/models/Marine_Swmwear_1.01/Marine_Swmwear_1.01.pmx", "resources/models/Marine_Swmwear_1.01/textures"),
    "aqua-basebody": ("resources/models/Aqua_BaseBody_R15_0.9/Aqua_BaseBody_R15_0.9.pmx", "resources/models/Aqua_BaseBody_R15_0.9/textures"),
    "aqua-sailor": ("resources/models/Aqua_Sailor_0.8/Aqua_Sailor_0.8.pmx", "resources/models/Aqua_Sailor_0.8/textures"),
    "brujas": ("resources/models/Brujas/Brujas.pmx", "resources/models/Brujas"),
    "lamy-swimwear": ("resources/models/Lamy_Swimwear_1.0/Lamy_Swimwear_1.0.pmx", "resources/models/Lamy_Swimwear_1.0/textures"),
    "lulum": ("resources/models/lulum/lulum_1.0.pmx", "resources/models/lulum/textures"),
    "marine-jk1": ("resources/models/Marine_JK1_Set_1.01/Marine_JK1_1.0.pmx", "resources/models/Marine_JK1_Set_1.01/textures"),
    "marine-jk1-hi": ("resources/models/Marine_JK1_Set_1.01/Marine_JK1_Hi_1.0.pmx", "resources/models/Marine_JK1_Set_1.01/textures"),
    "rurudo-lion": ("resources/models/RurudoLion_1.0/RurudoLion_1.0.pmx", "resources/models/RurudoLion_1.0/textures"),
    "rurudo-lion-hi": ("resources/models/RurudoLion_1.0/RurudoLion_Hi_1.0.pmx", "resources/models/RurudoLion_1.0/textures"),
    "卢西娅": ("resources/models/卢西娅/卢西娅.pmx", "resources/models/卢西娅/textures"),
    "卢西娅-摘帽": ("resources/models/卢西娅/卢西娅_摘帽.pmx", "resources/models/卢西娅/textures"),
    "卢西娅-武器1": ("resources/models/卢西娅/武器1.pmx", "resources/models/卢西娅/textures"),
    "卢西娅-武器2": ("resources/models/卢西娅/武器2.pmx", "resources/models/卢西娅/textures"),
}


def main():
    parser = argparse.ArgumentParser(description="PMX Model Viewer")
    parser.add_argument("--model", "-m", default="ikaros-uniform", choices=MODELS.keys(), help="Model to load")
    parser.add_argument("--vmd", "-v", nargs="*", default=None, help="VMD animation file(s)")
    args = parser.parse_args()

    pmx_path, tex_dir = MODELS[args.model]
    model_path = PROJ_ROOT / pmx_path
    texture_dir = PROJ_ROOT / tex_dir
    toon_dir = PROJ_ROOT / "resources" / "toon"

    print(f"Loading model: {model_path}")
    try:
        model = PmxModel(model_path)
    except Exception as e:
        print(f"Failed to load model: {e}")
        return
    print(f"Model: {model.name}, Vertices: {model.vertex_count}, Faces: {model.face_count}")
    print(f"Textures: {model.textures}")

    print("Initializing renderer...")
    renderer = Renderer(1280, 720, f"PMX Viewer - {model.name}")

    print("Loading model to GPU...")
    vpd = PROJ_ROOT / "resources" / "vpd" / "自然站姿.vpd"
    renderer.load_model(model, str(texture_dir), toon_dir=str(toon_dir), vpd_path=str(vpd) if vpd.exists() else "")

    if args.vmd:
        print(f"Loading {len(args.vmd)} VMD file(s)...")
        for vmd_file in args.vmd:
            vmd_path = PROJ_ROOT / vmd_file
            if vmd_path.exists():
                renderer.animation_controller.load_vmd(str(vmd_path))
                print(f"  Loaded: {vmd_file}")
            else:
                print(f"  VMD file not found: {vmd_path}")
        if renderer.animation_controller.vmd_mixer and renderer.animation_controller.vmd_mixer.players:
            print(f"Total VMD layers: {len(renderer.animation_controller.vmd_mixer.players)}")
            renderer.animation_controller.play_vmd()

    renderer.print_help()
    print("Starting render loop...")
    renderer.render()
    renderer.close()


if __name__ == "__main__":
    main()
