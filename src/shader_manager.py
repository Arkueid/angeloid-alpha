from OpenGL.GL import *
from OpenGL.GL import shaders
import numpy as np
from pathlib import Path


def _load_shader(filename):
    path = Path(__file__).parent.parent / "resources" / "shaders" / filename
    with open(path, "r", encoding="utf-8") as f:
        return f.read()


def _compile_shader(vertex_src: str, fragment_src: str):
    vertex_shader = shaders.compileShader(vertex_src, GL_VERTEX_SHADER)
    fragment_shader = shaders.compileShader(fragment_src, GL_FRAGMENT_SHADER)
    program = shaders.compileProgram(vertex_shader, fragment_shader)
    return program


class ShaderManager:
    def __init__(self):
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

    def _set_uniform(self, program, name, value):
        location = glGetUniformLocation(program, name)
        if location == -1:
            return
        if isinstance(value, (int, bool)):
            glUniform1i(location, int(value))
        elif isinstance(value, float):
            glUniform1f(location, value)
        elif isinstance(value, (tuple, list)):
            if len(value) == 2:
                glUniform2f(location, *value)
            elif len(value) == 3:
                glUniform3f(location, *value)
            elif len(value) == 4:
                glUniform4f(location, *value)

    def _create_main_shader(self):
        self.programs['main'] = _compile_shader(
            _load_shader("main.vert"),
            _load_shader("main.frag")
        )
        glUseProgram(self.programs['main'])
        self._set_uniform(self.programs['main'], "light_dir", (0.0, 0.5, -1.0))
        self._set_uniform(self.programs['main'], "has_texture", False)
        self._set_uniform(self.programs['main'], "tex", 0)
        self._set_uniform(self.programs['main'], "alpha", 1.0)
        self._set_uniform(self.programs['main'], "material_color", (1.0, 1.0, 1.0))

    def _create_axis_shader(self):
        self.programs['axis'] = _compile_shader(
            _load_shader("axis.vert"),
            _load_shader("axis.frag")
        )

    def _create_outline_shader(self):
        self.programs['outline'] = _compile_shader(
            _load_shader("outline.vert"),
            _load_shader("outline.frag")
        )
        glUseProgram(self.programs['outline'])
        self._set_uniform(self.programs['outline'], "outline_color", (0.0, 0.0, 0.0))
        self._set_uniform(self.programs['outline'], "outline_thickness", 0.001)
        self._set_uniform(self.programs['outline'], "tex", 0)
        self._set_uniform(self.programs['outline'], "alpha", 1.0)

        self.programs['outline_skinned'] = _compile_shader(
            _load_shader("outline_skinned.vert"),
            _load_shader("outline.frag")
        )
        glUseProgram(self.programs['outline_skinned'])
        self._set_uniform(self.programs['outline_skinned'], "outline_color", (0.0, 0.0, 0.0))
        self._set_uniform(self.programs['outline_skinned'], "outline_thickness", 0.001)
        self._set_uniform(self.programs['outline_skinned'], "tex", 0)
        self._set_uniform(self.programs['outline_skinned'], "bone_texture", 1)
        self._set_uniform(self.programs['outline_skinned'], "bone_texture_width", 64)
        self._set_uniform(self.programs['outline_skinned'], "alpha", 1.0)

    def _create_toon_shader(self):
        self.programs['toon'] = _compile_shader(
            _load_shader("toon.vert"),
            _load_shader("toon.frag")
        )
        glUseProgram(self.programs['toon'])
        self._set_uniform(self.programs['toon'], "light_dir", (0.0, 0.5, -1.0))
        self._set_uniform(self.programs['toon'], "shadow_thresh", 0.2)
        self._set_uniform(self.programs['toon'], "rim_power", 3.0)
        self._set_uniform(self.programs['toon'], "rim_color", (1.0, 0.95, 0.9))
        self._set_uniform(self.programs['toon'], "has_texture", False)
        self._set_uniform(self.programs['toon'], "tex", 0)
        self._set_uniform(self.programs['toon'], "gradient_map", 1)
        self._set_uniform(self.programs['toon'], "alpha", 1.0)
        self._set_uniform(self.programs['toon'], "material_color", (1.0, 1.0, 1.0))
        self._set_uniform(self.programs['toon'], "toon_tex", 4)
        self._set_uniform(self.programs['toon'], "has_toon", False)
        self._set_uniform(self.programs['toon'], "sphere_tex", 3)
        self._set_uniform(self.programs['toon'], "sphere_mode", 0)

    def _create_skinned_shader(self):
        self.programs['skinned'] = _compile_shader(
            _load_shader("skinned.vert"),
            _load_shader("toon.frag")
        )
        glUseProgram(self.programs['skinned'])
        self._set_uniform(self.programs['skinned'], "light_dir", (0.0, 0.5, -1.0))
        self._set_uniform(self.programs['skinned'], "has_texture", False)
        self._set_uniform(self.programs['skinned'], "tex", 0)
        self._set_uniform(self.programs['skinned'], "bone_texture", 1)
        self._set_uniform(self.programs['skinned'], "gradient_map", 2)
        self._set_uniform(self.programs['skinned'], "shadow_thresh", 0.2)
        self._set_uniform(self.programs['skinned'], "rim_power", 3.0)
        self._set_uniform(self.programs['skinned'], "rim_color", (1.0, 0.95, 0.9))
        self._set_uniform(self.programs['skinned'], "alpha", 1.0)
        self._set_uniform(self.programs['skinned'], "material_color", (1.0, 1.0, 1.0))
        
        self.programs['skinned_notoon'] = _compile_shader(
            _load_shader("skinned.vert"),
            _load_shader("main.frag")
        )
        glUseProgram(self.programs['skinned_notoon'])
        self._set_uniform(self.programs['skinned_notoon'], "light_dir", (0.0, 0.5, -1.0))
        self._set_uniform(self.programs['skinned_notoon'], "has_texture", False)
        self._set_uniform(self.programs['skinned_notoon'], "tex", 0)
        self._set_uniform(self.programs['skinned_notoon'], "bone_texture", 1)
        self._set_uniform(self.programs['skinned_notoon'], "alpha", 1.0)
        self._set_uniform(self.programs['skinned_notoon'], "material_color", (1.0, 1.0, 1.0))

    def _create_morph_shader(self):
        self.programs['morph'] = _compile_shader(
            _load_shader("skinned_morph.vert"),
            _load_shader("toon.frag")
        )
        glUseProgram(self.programs['morph'])
        self._set_uniform(self.programs['morph'], "light_dir", (0.0, 0.5, -1.0))
        self._set_uniform(self.programs['morph'], "has_texture", False)
        self._set_uniform(self.programs['morph'], "tex", 0)
        self._set_uniform(self.programs['morph'], "gradient_map", 2)
        self._set_uniform(self.programs['morph'], "shadow_thresh", 0.2)
        self._set_uniform(self.programs['morph'], "rim_power", 3.0)
        self._set_uniform(self.programs['morph'], "rim_color", (1.0, 0.95, 0.9))
        self._set_uniform(self.programs['morph'], "morph_weight", 0.0)
        self._set_uniform(self.programs['morph'], "bone_texture_width", 64)
        self._set_uniform(self.programs['morph'], "bone_texture", 1)
        self._set_uniform(self.programs['morph'], "alpha", 1.0)
        self._set_uniform(self.programs['morph'], "material_color", (1.0, 1.0, 1.0))

        self.programs['morph_notoon'] = _compile_shader(
            _load_shader("skinned_morph.vert"),
            _load_shader("main.frag")
        )
        glUseProgram(self.programs['morph_notoon'])
        self._set_uniform(self.programs['morph_notoon'], "light_dir", (0.0, 0.5, -1.0))
        self._set_uniform(self.programs['morph_notoon'], "has_texture", False)
        self._set_uniform(self.programs['morph_notoon'], "tex", 0)
        self._set_uniform(self.programs['morph_notoon'], "morph_weight", 0.0)
        self._set_uniform(self.programs['morph_notoon'], "bone_texture_width", 64)
        self._set_uniform(self.programs['morph_notoon'], "bone_texture", 1)
        self._set_uniform(self.programs['morph_notoon'], "alpha", 1.0)
        self._set_uniform(self.programs['morph_notoon'], "material_color", (1.0, 1.0, 1.0))
        
        self.programs['morph_outline'] = _compile_shader(
            _load_shader("outline_skinned_morph.vert"),
            _load_shader("outline.frag")
        )
        glUseProgram(self.programs['morph_outline'])
        self._set_uniform(self.programs['morph_outline'], "outline_color", (0.0, 0.0, 0.0))
        self._set_uniform(self.programs['morph_outline'], "outline_thickness", 0.001)
        self._set_uniform(self.programs['morph_outline'], "tex", 0)
        self._set_uniform(self.programs['morph_outline'], "morph_weight", 0.0)
        self._set_uniform(self.programs['morph_outline'], "bone_texture_width", 64)
        self._set_uniform(self.programs['morph_outline'], "bone_texture", 1)
        self._set_uniform(self.programs['morph_outline'], "alpha", 1.0)

    def _create_gradient_texture(self):
        gradient_data = np.array([
            [60, 60, 60],
            [120, 120, 120],
            [180, 180, 180],
            [220, 220, 220]
        ], dtype='u1')
        
        self.gradient_texture = glGenTextures(1)
        glBindTexture(GL_TEXTURE_2D, self.gradient_texture)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 4, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, gradient_data.tobytes())
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE)

    def get_program(self, name: str):
        return self.programs.get(name)

    def set_outline_thickness(self, thickness: float):
        if 'outline' in self.programs:
            glUseProgram(self.programs['outline'])
            self._set_uniform(self.programs['outline'], "outline_thickness", thickness)
        if 'outline_skinned' in self.programs:
            glUseProgram(self.programs['outline_skinned'])
            self._set_uniform(self.programs['outline_skinned'], "outline_thickness", thickness)
        if 'morph_outline' in self.programs:
            glUseProgram(self.programs['morph_outline'])
            self._set_uniform(self.programs['morph_outline'], "outline_thickness", thickness)
