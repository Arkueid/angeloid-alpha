import moderngl
import numpy as np
from pathlib import Path


def _load_shader(filename):
    path = Path(__file__).parent.parent / "resources" / "shaders" / filename
    with open(path, "r", encoding="utf-8") as f:
        return f.read()


class ShaderManager:
    def __init__(self, ctx: moderngl.Context):
        self.ctx = ctx
        self.programs = {}
        self.gradient_texture = None
        self._create_all_shaders()
        self._create_gradient_texture()

    def _create_all_shaders(self):
        self._create_main_shader()
        self._create_axis_shader()
        self._create_outline_shader()
        self._create_toon_shader()
        self._create_skinned_shader()
        self._create_morph_shader()

    def _create_main_shader(self):
        self.programs['main'] = self.ctx.program(
            vertex_shader=_load_shader("main.vert"),
            fragment_shader=_load_shader("main.frag")
        )
        self.programs['main']["light_dir"] = (0.0, 0.5, -1.0)
        self.programs['main']["has_texture"] = False
        self.programs['main']["tex"] = 0
        self.programs['main']["alpha"] = 1.0
        self.programs['main']["material_color"] = (1.0, 1.0, 1.0)

    def _create_axis_shader(self):
        self.programs['axis'] = self.ctx.program(
            vertex_shader=_load_shader("axis.vert"),
            fragment_shader=_load_shader("axis.frag")
        )

    def _create_outline_shader(self):
        self.programs['outline'] = self.ctx.program(
            vertex_shader=_load_shader("outline.vert"),
            fragment_shader=_load_shader("outline.frag")
        )
        self.programs['outline']["outline_color"] = (0.0, 0.0, 0.0)
        self.programs['outline']["outline_thickness"] = 0.001
        self.programs['outline']["tex"] = 0
        self.programs['outline']["alpha"] = 1.0

        self.programs['outline_skinned'] = self.ctx.program(
            vertex_shader=_load_shader("outline_skinned.vert"),
            fragment_shader=_load_shader("outline.frag")
        )
        self.programs['outline_skinned']["outline_color"] = (0.0, 0.0, 0.0)
        self.programs['outline_skinned']["outline_thickness"] = 0.001
        self.programs['outline_skinned']["tex"] = 0
        self.programs['outline_skinned']["bone_texture"] = 1
        self.programs['outline_skinned']["bone_texture_width"] = 64
        self.programs['outline_skinned']["alpha"] = 1.0

    def _create_toon_shader(self):
        self.programs['toon'] = self.ctx.program(
            vertex_shader=_load_shader("toon.vert"),
            fragment_shader=_load_shader("toon.frag")
        )
        self.programs['toon']["light_dir"] = (0.0, 0.5, -1.0)
        self.programs['toon']["shadow_thresh"] = 0.2
        self.programs['toon']["rim_power"] = 3.0
        self.programs['toon']["rim_color"] = (1.0, 0.95, 0.9)
        self.programs['toon']["has_texture"] = False
        self.programs['toon']["tex"] = 0
        self.programs['toon']["gradient_map"] = 1
        self.programs['toon']["alpha"] = 1.0
        self.programs['toon']["material_color"] = (1.0, 1.0, 1.0)

    def _create_skinned_shader(self):
        self.programs['skinned'] = self.ctx.program(
            vertex_shader=_load_shader("skinned.vert"),
            fragment_shader=_load_shader("toon.frag")
        )
        self.programs['skinned']["light_dir"] = (0.0, 0.5, -1.0)
        self.programs['skinned']["has_texture"] = False
        self.programs['skinned']["tex"] = 0
        self.programs['skinned']["bone_texture"] = 1
        self.programs['skinned']["gradient_map"] = 2
        self.programs['skinned']["shadow_thresh"] = 0.2
        self.programs['skinned']["rim_power"] = 3.0
        self.programs['skinned']["rim_color"] = (1.0, 0.95, 0.9)
        self.programs['skinned']["alpha"] = 1.0
        self.programs['skinned']["material_color"] = (1.0, 1.0, 1.0)
        
        self.programs['skinned_notoon'] = self.ctx.program(
            vertex_shader=_load_shader("skinned.vert"),
            fragment_shader=_load_shader("main.frag")
        )
        self.programs['skinned_notoon']["light_dir"] = (0.0, 0.5, -1.0)
        self.programs['skinned_notoon']["has_texture"] = False
        self.programs['skinned_notoon']["tex"] = 0
        self.programs['skinned_notoon']["bone_texture"] = 1
        self.programs['skinned_notoon']["alpha"] = 1.0
        self.programs['skinned_notoon']["material_color"] = (1.0, 1.0, 1.0)

    def _create_morph_shader(self):
        self.programs['morph'] = self.ctx.program(
            vertex_shader=_load_shader("skinned_morph.vert"),
            fragment_shader=_load_shader("toon.frag")
        )
        self.programs['morph']["light_dir"] = (0.0, 0.5, -1.0)
        self.programs['morph']["has_texture"] = False
        self.programs['morph']["tex"] = 0
        self.programs['morph']["gradient_map"] = 2
        self.programs['morph']["shadow_thresh"] = 0.2
        self.programs['morph']["rim_power"] = 3.0
        self.programs['morph']["rim_color"] = (1.0, 0.95, 0.9)
        self.programs['morph']["morph_weight"] = 0.0
        self.programs['morph']["bone_texture_width"] = 64
        self.programs['morph']["bone_texture"] = 1
        self.programs['morph']["alpha"] = 1.0
        self.programs['morph']["material_color"] = (1.0, 1.0, 1.0)

        self.programs['morph_notoon'] = self.ctx.program(
            vertex_shader=_load_shader("skinned_morph.vert"),
            fragment_shader=_load_shader("main.frag")
        )
        self.programs['morph_notoon']["light_dir"] = (0.0, 0.5, -1.0)
        self.programs['morph_notoon']["has_texture"] = False
        self.programs['morph_notoon']["tex"] = 0
        self.programs['morph_notoon']["morph_weight"] = 0.0
        self.programs['morph_notoon']["bone_texture_width"] = 64
        self.programs['morph_notoon']["bone_texture"] = 1
        self.programs['morph_notoon']["alpha"] = 1.0
        self.programs['morph_notoon']["material_color"] = (1.0, 1.0, 1.0)
        
        self.programs['morph_outline'] = self.ctx.program(
            vertex_shader=_load_shader("outline_skinned_morph.vert"),
            fragment_shader=_load_shader("outline.frag")
        )
        self.programs['morph_outline']["outline_color"] = (0.0, 0.0, 0.0)
        self.programs['morph_outline']["outline_thickness"] = 0.001
        self.programs['morph_outline']["tex"] = 0
        self.programs['morph_outline']["morph_weight"] = 0.0
        self.programs['morph_outline']["bone_texture_width"] = 64
        self.programs['morph_outline']["bone_texture"] = 1
        self.programs['morph_outline']["alpha"] = 1.0

    def _create_gradient_texture(self):
        gradient_data = np.array([
            [60, 60, 60],
            [120, 120, 120],
            [180, 180, 180],
            [220, 220, 220]
        ], dtype='u1')
        self.gradient_texture = self.ctx.texture((4, 1), 3, gradient_data.tobytes())
        self.gradient_texture.filter = (moderngl.LINEAR, moderngl.LINEAR)

    def get_program(self, name: str):
        return self.programs.get(name)

    def set_outline_thickness(self, thickness: float):
        if 'outline' in self.programs:
            self.programs['outline']["outline_thickness"] = thickness
        if 'outline_skinned' in self.programs:
            self.programs['outline_skinned']["outline_thickness"] = thickness
        if 'morph_outline' in self.programs:
            self.programs['morph_outline']["outline_thickness"] = thickness
