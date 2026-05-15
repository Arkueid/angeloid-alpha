import glfw
from OpenGL.GL import *
import numpy as np
import ctypes
from gpu import VAO, VBOWrapper, Texture, create_vao_with_buffers
from pmx_model import PmxModel
from bone_math import create_rigid_body_batched, create_joint_batched
from PIL import Image
import os
import json

from camera import Camera
from shader_manager import ShaderManager
from animation_controller import AnimationController
from morph_controller import MorphController


class Renderer:
    def __init__(self, width=1280, height=720, title="PMX Viewer"):
        self.width = width
        self.height = height
        self.title = title

        if not glfw.init():
            raise RuntimeError("Failed to initialize GLFW")

        glfw.window_hint(glfw.CONTEXT_VERSION_MAJOR, 3)
        glfw.window_hint(glfw.CONTEXT_VERSION_MINOR, 3)
        glfw.window_hint(glfw.OPENGL_PROFILE, glfw.OPENGL_CORE_PROFILE)
        glfw.window_hint(glfw.DOUBLEBUFFER, True)
        glfw.window_hint(glfw.DEPTH_BITS, 24)

        self.window = glfw.create_window(width, height, title, None, None)
        if not self.window:
            glfw.terminate()
            raise RuntimeError("Failed to create window")

        glfw.make_context_current(self.window)
        glfw.set_framebuffer_size_callback(self.window, self._on_resize)

        glEnable(GL_DEPTH_TEST)

        self.show_outline = True
        self.outline_thickness = 0.001
        self.show_toon = True

        self.shader_manager = ShaderManager()
        self.animation_controller = AnimationController()
        self.morph_controller = MorphController()

        self._create_world_axis()

        self.camera = Camera()

        self.show_world_axis = True
        self.show_ground_grid = True
        self.show_rigidbody = False
        self.show_joint = False
        self.show_model = True

        glfw.set_mouse_button_callback(self.window, self._on_mouse_button)
        glfw.set_cursor_pos_callback(self.window, self._on_cursor_pos)
        glfw.set_key_callback(self.window, self._on_key)
        glfw.set_scroll_callback(self.window, self._on_scroll)

        self.model_center = [0, 0, 0]
        self.model_scale = 1.0
        self.view_history = []

        self.idle_animation_enabled = True
        self.idle_time = 0.0
        self.idle_breath_intensity = 0.005
        self.idle_sway_intensity = 0.01
        self.idle_head_intensity = 0.008
        self.idle_blink_enabled = True
        self.idle_blink_time = 0.0
        self.idle_blink_interval = 4.0
        self.idle_blink_duration = 0.15
        self.idle_blink_morph_names = ["blink", "blink_l", "blink_r", "まばたき", "まぶたき"]

        self.model_vao = None
        self.outline_vao = None
        self.toon_vao = None
        self.skinned_vao = None
        self.pmx_model = None
        self.textures = []
        self.default_toon_index = -1
        self.default_toon_texture = None
        self.material_batches = []
        self.index_count = 0
        
        self.bone_texture = None
        self.bone_texture_width = 0
        self.show_skinned = False
        
        self.skinned_morph_vao = None
        self.skinned_morph_vao_notoon = None
        self.skinned_morph_outline_vao = None
        self.rigidbody_vao = None
        self.show_morph = True
        
        self.material_colors = {}
        
        self.skinned_debug = False
        self.skinned_debug_scale = 1.5
        
        self.dummy_texture = None
        self._uniform_cache = {}
        self._fps_frame_count = 0
        self._fps_last_time = 0.0
        self._fps_display = 0.0
        self._base_title = title

    def _create_world_axis(self):
        axis_length = 5.0

        axis_vertices = []
        for axis in range(3):
            start = [0.0, 0.0, 0.0]
            end = [0.0, 0.0, 0.0]
            color = [0.0, 0.0, 0.0]

            end[axis] = axis_length
            color[axis] = 1.0

            axis_vertices.extend(start + color)
            axis_vertices.extend(end + color)

        axis_vertices = np.array(axis_vertices, dtype='f4')
        
        self.axis_vao = VAO()
        self.axis_vao.bind()
        vbo = glGenBuffers(1)
        glBindBuffer(GL_ARRAY_BUFFER, vbo)
        glBufferData(GL_ARRAY_BUFFER, axis_vertices.nbytes, axis_vertices, GL_STATIC_DRAW)
        glEnableVertexAttribArray(0)
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 24, ctypes.c_void_p(0))
        glEnableVertexAttribArray(1)
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 24, ctypes.c_void_p(12))
        self.axis_vao.vertex_count = len(axis_vertices) // 6

        self._create_ground_grid()

    def _create_ground_grid(self):
        grid_size = 50.0
        grid_divisions = 25
        grid_step = grid_size / grid_divisions

        grid_vertices = []
        for i in range(-grid_divisions // 2, grid_divisions // 2 + 1):
            x = i * grid_step
            grid_vertices.extend([x, 0.0, -grid_size / 2, 0.5, 0.5, 0.5])
            grid_vertices.extend([x, 0.0, grid_size / 2, 0.5, 0.5, 0.5])

            z = i * grid_step
            grid_vertices.extend([-grid_size / 2, 0.0, z, 0.5, 0.5, 0.5])
            grid_vertices.extend([grid_size / 2, 0.0, z, 0.5, 0.5, 0.5])

        grid_vertices = np.array(grid_vertices, dtype='f4')
        
        self.grid_vao = VAO()
        self.grid_vao.bind()
        vbo = glGenBuffers(1)
        glBindBuffer(GL_ARRAY_BUFFER, vbo)
        glBufferData(GL_ARRAY_BUFFER, grid_vertices.nbytes, grid_vertices, GL_STATIC_DRAW)
        glEnableVertexAttribArray(0)
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 24, ctypes.c_void_p(0))
        glEnableVertexAttribArray(1)
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 24, ctypes.c_void_p(12))
        self.grid_vao.vertex_count = len(grid_vertices) // 6

    def _select_default_toon(self, r, g, b):
        if not self.default_toons:
            return None
        brightness = 0.299 * r + 0.587 * g + 0.114 * b
        best_toon = None
        best_diff = float('inf')
        for toon_name, toon_info in self.default_toons.items():
            shadow = toon_info['shadow_brightness']
            diff = abs(brightness - shadow)
            if diff < best_diff:
                best_diff = diff
                best_toon = toon_name
        return best_toon

    def _on_resize(self, window, width, height):
        self.width = width
        self.height = height
        glViewport(0, 0, width, height)

    def _on_mouse_button(self, window, button, action, mods):
        self.camera.on_mouse_button(window, button, action, mods)

    def _on_cursor_pos(self, window, xpos, ypos):
        self.camera.on_cursor_pos(window, xpos, ypos)

    def _on_scroll(self, window, xoffset, yoffset):
        self.camera.on_scroll(window, xoffset, yoffset)
        print(f"Camera speed: {self.camera.speed:.3f}")

    def _on_key(self, window, key, scancode, action, mods):
        if key == glfw.KEY_ESCAPE and action == glfw.PRESS:
            glfw.set_input_mode(window, glfw.CURSOR, glfw.CURSOR_NORMAL)
            self.camera.is_panning = False

        if key == glfw.KEY_X and action == glfw.PRESS:
            self.show_world_axis = not self.show_world_axis
            print(f"World axis: {'ON' if self.show_world_axis else 'OFF'}")

        if key == glfw.KEY_G and action == glfw.PRESS:
            self.show_ground_grid = not self.show_ground_grid
            print(f"Ground grid: {'ON' if self.show_ground_grid else 'OFF'}")

        if key == glfw.KEY_O and action == glfw.PRESS:
            self.show_outline = not self.show_outline
            print(f"Outline: {'ON' if self.show_outline else 'OFF'}")

        if key == glfw.KEY_B and action == glfw.PRESS:
            self.show_rigidbody = not self.show_rigidbody
            self.show_joint = self.show_rigidbody
            print(f"Rigidbody & Joint: {'ON' if self.show_rigidbody else 'OFF'}")

        if key == glfw.KEY_H and action == glfw.PRESS:
            self.show_model = not self.show_model
            print(f"Model mesh: {'ON' if self.show_model else 'OFF'}")

        if key == glfw.KEY_T and action == glfw.PRESS:
            self.show_toon = not self.show_toon
            print(f"Toon shading: {'ON' if self.show_toon else 'OFF'}")

        if key == glfw.KEY_K and action == glfw.PRESS:
            self.show_skinned = not self.show_skinned
            if self.show_skinned:
                self.animation_controller.reload_bone_texture(1.0, use_pose=self.animation_controller.vpd_pose_applied)
                print(f"Skinned rendering: ON")
            else:
                self.animation_controller.reload_bone_texture(1.0, use_pose=False)
                print(f"Skinned rendering: OFF")

        if key == glfw.KEY_P and action == glfw.PRESS:
            if self.animation_controller.vpd_poses:
                self.animation_controller.vpd_pose_applied = not self.animation_controller.vpd_pose_applied
                if self.show_skinned:
                    self.animation_controller.reload_bone_texture(1.0, use_pose=self.animation_controller.vpd_pose_applied)
                print(f"VPD pose: {'ON' if self.animation_controller.vpd_pose_applied else 'OFF'}")
            else:
                print("No VPD pose loaded")

        if key == glfw.KEY_R and action == glfw.PRESS:
            self.camera.reset()
            print("Camera reset to default position")

        if key == glfw.KEY_I and action == glfw.PRESS:
            self.idle_animation_enabled = not self.idle_animation_enabled
            self.idle_blink_enabled = self.idle_animation_enabled
            if self.idle_animation_enabled:
                self.idle_blink_time = 0.0
            print(f"Idle animation: {'ON' if self.idle_animation_enabled else 'OFF'} (blink: {'ON' if self.idle_blink_enabled else 'OFF'})")

        if key == glfw.KEY_M and action == glfw.PRESS:
            self.show_morph = not self.show_morph
            if self.show_morph:
                self.show_skinned = True
                available_morphs = self.pmx_model.get_available_morphs() if self.pmx_model else []
                if available_morphs:
                    self.morph_controller.active_morph_index = available_morphs[0].index
                    print(f"Morph mode: ON (showing morph: {available_morphs[0].name})")
                else:
                    print("Morph mode: ON (no available morphs found)")
            else:
                self.morph_controller.clear_morphs()
                print("Morph mode: OFF")

        if key == glfw.KEY_COMMA and action in (glfw.PRESS, glfw.REPEAT):
            if self.show_morph and self.pmx_model:
                available_morphs = self.pmx_model.get_available_morphs()
                if available_morphs:
                    current_idx = next((i for i, m in enumerate(available_morphs) if m.index == self.morph_controller.active_morph_index), -1)
                    new_idx = (current_idx - 1) % len(available_morphs)
                    self.morph_controller.active_morph_index = available_morphs[new_idx].index
                    self.morph_controller.morph_weights.clear()
                    self.morph_controller.set_morph_weight(available_morphs[new_idx].name, self.morph_controller.morph_weight_value)
                    print(f"Active morph: {available_morphs[new_idx].name} (weight={self.morph_controller.morph_weight_value:.2f})")

        if key == glfw.KEY_PERIOD and action in (glfw.PRESS, glfw.REPEAT):
            if self.show_morph and self.pmx_model:
                available_morphs = self.pmx_model.get_available_morphs()
                if available_morphs:
                    current_idx = next((i for i, m in enumerate(available_morphs) if m.index == self.morph_controller.active_morph_index), -1)
                    new_idx = (current_idx + 1) % len(available_morphs)
                    self.morph_controller.active_morph_index = available_morphs[new_idx].index
                    self.morph_controller.morph_weights.clear()
                    self.morph_controller.set_morph_weight(available_morphs[new_idx].name, self.morph_controller.morph_weight_value)
                    print(f"Active morph: {available_morphs[new_idx].name} (weight={self.morph_controller.morph_weight_value:.2f})")

        if key == glfw.KEY_UP and action in (glfw.PRESS, glfw.REPEAT):
            if self.show_morph:
                self.morph_controller.morph_weight_value = min(1.0, self.morph_controller.morph_weight_value + 0.1)
                if self.pmx_model:
                    morph = self.pmx_model.get_morph(self.morph_controller.active_morph_index)
                    if morph:
                        self.morph_controller.set_morph_weight(morph.name, self.morph_controller.morph_weight_value)
                        print(f"Morph weight: {self.morph_controller.morph_weight_value:.2f}")

        if key == glfw.KEY_DOWN and action in (glfw.PRESS, glfw.REPEAT):
            if self.show_morph:
                self.morph_controller.morph_weight_value = max(0.0, self.morph_controller.morph_weight_value - 0.1)
                if self.pmx_model:
                    morph = self.pmx_model.get_morph(self.morph_controller.active_morph_index)
                    if morph:
                        self.morph_controller.set_morph_weight(morph.name, self.morph_controller.morph_weight_value)
                        print(f"Morph weight: {self.morph_controller.morph_weight_value:.2f}")

        if key == glfw.KEY_SPACE and action == glfw.PRESS:
            if self.animation_controller.vmd_mixer and self.animation_controller.vmd_mixer.players:
                if self.animation_controller.vmd_playing:
                    self.animation_controller.pause_vmd()
                else:
                    self.animation_controller.play_vmd()
            else:
                print("No VMD animation loaded")

        if key == glfw.KEY_L and action == glfw.PRESS:
            self.animation_controller.vmd_loop = not self.animation_controller.vmd_loop
            print(f"VMD loop: {'ON' if self.animation_controller.vmd_loop else 'OFF'}")

        if key == glfw.KEY_LEFT_BRACKET and action in (glfw.PRESS, glfw.REPEAT):
            if self.animation_controller.vmd_mixer and self.animation_controller.vmd_mixer.players:
                new_frame = max(0, self.animation_controller.vmd_mixer.current_frame - 30)
                self.animation_controller.vmd_mixer.set_frame(new_frame)
                self.animation_controller.apply_vmd_frame(self.morph_controller)
                print(f"VMD frame: {self.animation_controller.vmd_mixer.current_frame:.0f}/{self.animation_controller.vmd_mixer.max_frame}")

        if key == glfw.KEY_RIGHT_BRACKET and action in (glfw.PRESS, glfw.REPEAT):
            if self.animation_controller.vmd_mixer and self.animation_controller.vmd_mixer.players:
                new_frame = min(self.animation_controller.vmd_mixer.max_frame, self.animation_controller.vmd_mixer.current_frame + 30)
                self.animation_controller.vmd_mixer.set_frame(new_frame)
                self.animation_controller.apply_vmd_frame(self.morph_controller)
                print(f"VMD frame: {self.animation_controller.vmd_mixer.current_frame:.0f}/{self.animation_controller.vmd_mixer.max_frame}")

    def _render_axis_grid(self, projection, view):
        identity = np.eye(4, dtype='f4')
        if self.show_ground_grid:
            glDisable(GL_DEPTH_TEST)
            axis_program = self.shader_manager.get_program('axis')
            glUseProgram(axis_program)
            self._set_uniform_matrix(axis_program, "projection", projection.T)
            self._set_uniform_matrix(axis_program, "view", view.T)
            self._set_uniform_matrix(axis_program, "model", identity)
            glLineWidth(2.0)
            self.grid_vao.render(GL_LINES)
            glEnable(GL_DEPTH_TEST)

        if self.show_world_axis:
            glDisable(GL_DEPTH_TEST)
            axis_program = self.shader_manager.get_program('axis')
            glUseProgram(axis_program)
            self._set_uniform_matrix(axis_program, "projection", projection.T)
            self._set_uniform_matrix(axis_program, "view", view.T)
            self._set_uniform_matrix(axis_program, "model", identity)
            glLineWidth(3.0)
            self.axis_vao.render(GL_LINES)
            glEnable(GL_DEPTH_TEST)

    def _render_physics(self, projection, view, model):
        if (self.show_rigidbody and self.rigidbody_vao) or (self.show_joint and self.joint_vao):
            glClear(GL_DEPTH_BUFFER_BIT)

        if self.show_rigidbody and self.rigidbody_vao:
            glEnable(GL_DEPTH_TEST)
            glDepthFunc(GL_LEQUAL)
            glDisable(GL_CULL_FACE)
            glEnable(GL_BLEND)
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)
            rb_program = self.shader_manager.get_program('rigidbody')
            glUseProgram(rb_program)
            self._set_uniform_matrix(rb_program, "projection", projection.T)
            self._set_uniform_matrix(rb_program, "view", view.T)
            self._set_uniform_matrix(rb_program, "model", model.T)
            self._set_uniform(rb_program, "bone_texture_width", self.bone_texture_width)
            glActiveTexture(GL_TEXTURE1)
            glBindTexture(GL_TEXTURE_2D, self.bone_texture.texture_id)
            self._set_uniform(rb_program, "bone_texture", 1)
            glLineWidth(1.0)
            self.rigidbody_vao.render(GL_LINES)
            glDisable(GL_BLEND)
            glEnable(GL_CULL_FACE)

        if self.show_joint and self.joint_vao:
            glEnable(GL_DEPTH_TEST)
            glDepthFunc(GL_LEQUAL)
            glDisable(GL_CULL_FACE)
            glEnable(GL_BLEND)
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)
            rb_program = self.shader_manager.get_program('rigidbody')
            glUseProgram(rb_program)
            self._set_uniform_matrix(rb_program, "projection", projection.T)
            self._set_uniform_matrix(rb_program, "view", view.T)
            self._set_uniform_matrix(rb_program, "model", model.T)
            self._set_uniform(rb_program, "bone_texture_width", self.bone_texture_width)
            glActiveTexture(GL_TEXTURE1)
            glBindTexture(GL_TEXTURE_2D, self.bone_texture.texture_id)
            self._set_uniform(rb_program, "bone_texture", 1)
            glLineWidth(1.0)
            self.joint_vao.render(GL_LINES)
            glDisable(GL_BLEND)
            glEnable(GL_CULL_FACE)

    def _set_uniform(self, program, name, value):
        cache_key = (program, name)
        if cache_key in self._uniform_cache:
            location = self._uniform_cache[cache_key]
        else:
            location = glGetUniformLocation(program, name)
            self._uniform_cache[cache_key] = location
        
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

    def _set_uniform_matrix(self, program, name, matrix):
        cache_key = (program, name)
        if cache_key in self._uniform_cache:
            location = self._uniform_cache[cache_key]
        else:
            location = glGetUniformLocation(program, name)
            self._uniform_cache[cache_key] = location
        
        if location == -1:
            return
        glUniformMatrix4fv(location, 1, GL_FALSE, matrix.astype('f4').tobytes())

    def _transform_to_normalized(self, verts):
        cx, my, cz = self.model_center[0], self.model_min_pos[1], self.model_center[2]
        s = self.model_scale
        verts[:, 0] = (verts[:, 0] - cx) * s
        verts[:, 1] = (verts[:, 1] - my) * s
        verts[:, 2] = (verts[:, 2] - cz) * s

    @staticmethod
    def _create_wireframe_vao(verts, colors, bone_indices):
        vao = VAO()
        vao.bind()
        vao.add_vbo(verts, 0, 3)
        vao.add_vbo(colors, 1, 3)
        vao.add_vbo(bone_indices, 2, 1, dtype=GL_INT)
        vao.vertex_count = len(verts)
        vao.unbind()
        return vao

    @staticmethod
    def _add_vbos(vao, vbo_data):
        for location, data, size, dtype in vbo_data:
            vbo = glGenBuffers(1)
            glBindBuffer(GL_ARRAY_BUFFER, vbo)
            glBufferData(GL_ARRAY_BUFFER, data.nbytes, data, GL_STATIC_DRAW)
            glEnableVertexAttribArray(location)
            if dtype == GL_INT:
                glVertexAttribIPointer(location, size, dtype, 0, ctypes.c_void_p(0))
            else:
                glVertexAttribPointer(location, size, dtype, GL_FALSE, 0, ctypes.c_void_p(0))

    @staticmethod
    def _create_morph_vao(base_vbo_data, morph_offsets, uv_morph_offsets, indices):
        vao = VAO()
        vao.bind()
        Renderer._add_vbos(vao, base_vbo_data)
        morph_id = glGenBuffers(1)
        glBindBuffer(GL_ARRAY_BUFFER, morph_id)
        glBufferData(GL_ARRAY_BUFFER, morph_offsets.nbytes, morph_offsets, GL_DYNAMIC_DRAW)
        glEnableVertexAttribArray(5)
        glVertexAttribPointer(5, 3, GL_FLOAT, GL_FALSE, 0, ctypes.c_void_p(0))
        uv_id = glGenBuffers(1)
        glBindBuffer(GL_ARRAY_BUFFER, uv_id)
        glBufferData(GL_ARRAY_BUFFER, uv_morph_offsets.nbytes, uv_morph_offsets, GL_DYNAMIC_DRAW)
        glEnableVertexAttribArray(6)
        glVertexAttribPointer(6, 2, GL_FLOAT, GL_FALSE, 0, ctypes.c_void_p(0))
        ebo = glGenBuffers(1)
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo)
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.nbytes, indices, GL_STATIC_DRAW)
        vao.ebo = ebo
        vao.index_count = len(indices)
        vao.unbind()
        return vao, morph_id, uv_id

    @staticmethod
    def _create_mesh_vao(vertices, indices):
        vao = VAO()
        vao.bind()
        vbo = glGenBuffers(1)
        glBindBuffer(GL_ARRAY_BUFFER, vbo)
        glBufferData(GL_ARRAY_BUFFER, vertices.nbytes, vertices, GL_STATIC_DRAW)
        glEnableVertexAttribArray(0)
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 32, ctypes.c_void_p(0))
        glEnableVertexAttribArray(1)
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 32, ctypes.c_void_p(12))
        glEnableVertexAttribArray(2)
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 32, ctypes.c_void_p(24))
        ebo = glGenBuffers(1)
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo)
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.nbytes, indices, GL_STATIC_DRAW)
        vao.ebo = ebo
        vao.index_count = len(indices)
        vao.unbind()
        return vao

    def load_model(self, pmx_model: PmxModel, texture_dir: str = "", toon_dir: str = "", vpd_path: str = ""):
        self.pmx_model = pmx_model
        positions = np.array([(v.position[0], v.position[1], v.position[2]) for v in pmx_model.vertices], dtype='f4')

        if vpd_path and os.path.exists(vpd_path):
            self.animation_controller.load_vpd(vpd_path)

        min_pos = positions.min(axis=0)
        max_pos = positions.max(axis=0)
        center = (min_pos + max_pos) / 2
        size = max_pos - min_pos
        max_size = max(size)

        print(f"Model bounds: min={min_pos}, max={max_pos}")
        print(f"Model center: {center}, max dimension: {max_size}")

        self.model_center = center.tolist()
        self.model_min_pos = min_pos.tolist()
        self.model_scale = 2.0 / max_size if max_size > 0 else 1.0

        scaled_size = max_size * self.model_scale
        print(f"Model size after scaling: {scaled_size:.2f}")

        vertices = []
        for v in pmx_model.vertices:
            x = (v.position[0] - center[0]) * self.model_scale
            y = (v.position[1] - min_pos[1]) * self.model_scale
            z = (v.position[2] - center[2]) * self.model_scale
            nx = v.normal[0]
            ny = v.normal[1]
            nz = v.normal[2]
            vertices.extend([x, y, z, nx, ny, nz, v.uv[0], v.uv[1]])

        vertices = np.array(vertices, dtype='f4')
        indices = np.array(list(pmx_model.indices), dtype='i4')

        self.model_vao = self._create_mesh_vao(vertices, indices)
        self.toon_vao = self._create_mesh_vao(vertices, indices)
        self.outline_vao = self._create_mesh_vao(vertices, indices)

        print(f"\nLoading skeleton data...")
        skinning_data = pmx_model.get_all_skinning_vertex_data()
        transform_params = {
            'center': self.model_center,
            'min_y': self.model_min_pos[1],
            'scale': self.model_scale
        }
        bone_tex_data, tex_width, tex_height = pmx_model.get_bone_texture_data(transform_params=transform_params)
        
        print(f"  Bone count: {pmx_model.bone_count}")
        print(f"  Bone texture size: {tex_width}x{tex_height}")
        
        skinned_positions = np.zeros_like(skinning_data['positions'])
        for i in range(len(skinned_positions) // 3):
            skinned_positions[i*3] = (skinning_data['positions'][i*3] - center[0]) * self.model_scale
            skinned_positions[i*3 + 1] = (skinning_data['positions'][i*3 + 1] - min_pos[1]) * self.model_scale
            skinned_positions[i*3 + 2] = (skinning_data['positions'][i*3 + 2] - center[2]) * self.model_scale

        self.bone_texture = Texture(tex_width, tex_height, 4, bone_tex_data.tobytes(), dtype=GL_FLOAT)
        self.bone_texture.set_filter(GL_NEAREST, GL_NEAREST)
        self.bone_texture.set_repeat(False, False)
        self.bone_texture_width = tex_width
        
        self.skinned_vao = create_vao_with_buffers([
            (0, skinned_positions, 3, GL_FLOAT),
            (1, skinning_data['normals'], 3, GL_FLOAT),
            (2, skinning_data['uvs'], 2, GL_FLOAT),
            (3, skinning_data['bone_indices'], 4, GL_INT),
            (4, skinning_data['bone_weights'], 4, GL_FLOAT),
        ], indices)
        
        self.skinned_vao_notoon = create_vao_with_buffers([
            (0, skinned_positions, 3, GL_FLOAT),
            (1, skinning_data['normals'], 3, GL_FLOAT),
            (2, skinning_data['uvs'], 2, GL_FLOAT),
            (3, skinning_data['bone_indices'], 4, GL_INT),
            (4, skinning_data['bone_weights'], 4, GL_FLOAT),
        ], indices)

        self.skinned_outline_vao = create_vao_with_buffers([
            (0, skinned_positions, 3, GL_FLOAT),
            (1, skinning_data['normals'], 3, GL_FLOAT),
            (2, skinning_data['uvs'], 2, GL_FLOAT),
            (3, skinning_data['bone_indices'], 4, GL_INT),
            (4, skinning_data['bone_weights'], 4, GL_FLOAT),
        ], indices)

        morph_offsets = np.zeros(len(skinned_positions), dtype='f4')
        uv_morph_offsets = np.zeros(len(skinning_data['uvs']), dtype='f4')

        vbo_data = [
            (0, skinned_positions, 3, GL_FLOAT),
            (1, skinning_data['normals'], 3, GL_FLOAT),
            (2, skinning_data['uvs'], 2, GL_FLOAT),
            (3, skinning_data['bone_indices'], 4, GL_INT),
            (4, skinning_data['bone_weights'], 4, GL_FLOAT),
        ]

        self.skinned_morph_vao, m_id, uv_id = self._create_morph_vao(vbo_data, morph_offsets, uv_morph_offsets, indices)
        morph_vbo = VBOWrapper(m_id)
        uv_morph_vbo = VBOWrapper(uv_id)
        self.skinned_morph_vao_notoon, _, _ = self._create_morph_vao(vbo_data, morph_offsets, uv_morph_offsets, indices)
        self.skinned_morph_outline_vao, _, _ = self._create_morph_vao(vbo_data, morph_offsets, uv_morph_offsets, indices)

        print(f"\nLoading textures...")
        self.textures = []
        for i, tex_name in enumerate(pmx_model.textures):
            if not tex_name:
                self.textures.append(None)
                print(f"  [{i}] Empty texture slot")
                continue

            tex_path = os.path.join(texture_dir, os.path.basename(tex_name))
            if not os.path.exists(tex_path):
                tex_name_without_prefix = tex_name
                for prefix in ['texture/', 'textures/', 'tex/']:
                    if tex_name.lower().startswith(prefix):
                        tex_name_without_prefix = tex_name[len(prefix):]
                        break
                tex_path = os.path.join(texture_dir, tex_name_without_prefix)
                if os.path.exists(tex_path):
                    try:
                        img = Image.open(tex_path).convert('RGBA')
                        tex = Texture(img.size[0], img.size[1], 4, img.tobytes())
                        tex.set_filter(GL_LINEAR, GL_LINEAR)
                        tex.set_repeat(True, True)
                        self.textures.append(tex)
                        print(f"  [{i}] OK: {os.path.basename(tex_path)}")
                    except Exception as e:
                        print(f"  [{i}] Failed: {tex_name} - {e}")
                        self.textures.append(None)
                else:
                    print(f"  [{i}] Not found: {tex_name}")
                    self.textures.append(None)
            else:
                try:
                    img = Image.open(tex_path).convert('RGBA')
                    tex = Texture(img.size[0], img.size[1], 4, img.tobytes())
                    tex.set_filter(GL_LINEAR, GL_LINEAR)
                    tex.set_repeat(True, True)
                    self.textures.append(tex)
                    print(f"  [{i}] OK: {os.path.basename(tex_path)}")
                except Exception as e:
                    print(f"  [{i}] Failed: {tex_name} - {e}")
                    self.textures.append(None)

        self.default_toons = {}
        self.default_toon_textures = []
        default_toon_name = 'toon01.bmp'
        toon_path = os.path.join(toon_dir, default_toon_name) if toon_dir else ""
        if toon_dir and os.path.exists(toon_path):
            try:
                img = Image.open(toon_path).convert('RGBA')
                arr = np.array(img)
                left_half = arr[:, :arr.shape[1]//2, :]
                shadow_brightness = left_half.mean() / 255.0
                tex = Texture(img.size[0], img.size[1], 4, img.tobytes())
                tex.set_filter(GL_LINEAR, GL_LINEAR)
                tex.set_repeat(True, True)
                tex_idx = len(self.textures)
                self.textures.append(tex)
                self.default_toons[default_toon_name] = {
                    'index': tex_idx,
                    'texture': tex,
                    'shadow_brightness': shadow_brightness
                }
                print(f"  [toon] {default_toon_name}: shadow={shadow_brightness:.3f}, index={tex_idx}")
                self.default_toon_textures = [default_toon_name]
                print(f"  [toon] Using unified default toon: {default_toon_name}")
            except Exception as e:
                print(f"  [toon] {default_toon_name}: Failed - {e}")
                self.default_toon_textures = []
        else:
            print(f"  [toon] {default_toon_name}: Not found")
            self.default_toon_textures = []
        self.default_toon_index = -1
        self.default_toon_texture = None

        self.material_batches = []
        original_material_alphas = {}
        self.material_edges = {}
        self.material_specular = {}
        self.material_ambient = {}
        self.material_sphere = {}
        self.material_toon = {}
        index_offset = 0
        for i, mat in enumerate(pmx_model.materials):
            has_edge = mat.hasFlag(0x08)
            batch = {
                'first': index_offset,
                'count': mat.vertex_count,
                'texture_index': mat.texture_index,
                'material_index': i,
                'has_edge': has_edge
            }
            self.material_batches.append(batch)
            original_material_alphas[i] = mat.alpha
            self.material_colors[i] = (
                mat.diffuse_color.r,
                mat.diffuse_color.g,
                mat.diffuse_color.b
            )
            self.material_edges[i] = {
                'color': (mat.edge_color.r, mat.edge_color.g, mat.edge_color.b, mat.edge_color.a),
                'size': mat.edge_size,
                'has_edge': has_edge
            }
            self.material_specular[i] = {
                'color': (mat.specular_color.r, mat.specular_color.g, mat.specular_color.b),
                'factor': mat.specular_factor
            }
            self.material_ambient[i] = (
                mat.ambient_color.r,
                mat.ambient_color.g,
                mat.ambient_color.b
            )
            self.material_sphere[i] = {
                'texture_index': mat.sphere_texture_index,
                'mode': mat.sphere_mode
            }
            if mat.toon_texture_index >= 0:
                self.material_toon[i] = {
                    'texture_index': mat.toon_texture_index,
                    'sharing_flag': mat.toon_sharing_flag
                }
            else:
                default_toon = self.default_toons.get('toon01.bmp')
                self.material_toon[i] = {
                    'texture_index': default_toon['index'] if default_toon else -1,
                    'sharing_flag': 0
                }
            index_offset += mat.vertex_count

        print(f"Created {len(self.material_batches)} material batches, total indices: {index_offset}")
        
        print(f"\nMaterial sphere info:")
        for i, mat in enumerate(pmx_model.materials):
            sphere_idx = mat.sphere_texture_index
            sphere_mode = mat.sphere_mode
            if sphere_idx >= 0 and sphere_idx < len(self.textures) and self.textures[sphere_idx]:
                print(f"  Material {i} '{mat.name}': sphere_mode={sphere_mode}, sphere_tex_idx={sphere_idx}, texture={pmx_model.textures[sphere_idx] if sphere_idx < len(pmx_model.textures) else 'N/A'}")
        
        print(f"\nMaterial toon info:")
        for i, mat in enumerate(pmx_model.materials):
            toon_info = self.material_toon.get(i, {})
            toon_idx = toon_info.get('texture_index', -1)
            if toon_idx >= 0 and toon_idx >= len(pmx_model.textures):
                print(f"  Material {i} '{mat.name}': toon_idx={toon_idx} (default toon01.bmp)")

        self.animation_controller.set_model(
            pmx_model, self.bone_texture, self.model_center, self.model_min_pos, self.model_scale
        )
        self.animation_controller.reload_bone_texture(1.0, use_pose=False)
        
        self.morph_controller.set_model(
            pmx_model, morph_vbo, uv_morph_vbo, self.bone_texture, self.model_scale, original_material_alphas
        )

        rigidbodies = pmx_model.get_rigidbodies()
        self.rigidbodies = rigidbodies
        if rigidbodies:
            rb_verts, rb_cols, rb_bone_idx = create_rigid_body_batched(rigidbodies)
            self._transform_to_normalized(rb_verts)
            self.rigidbody_vao = self._create_wireframe_vao(rb_verts, rb_cols, rb_bone_idx)
            print(f"Created rigidbody VAO: {len(rb_verts)} vertices ({len(rigidbodies)} rigidbodies batched)")
        else:
            self.rigidbody_vao = None
            print(f"No rigidbodies found in model")

        joints = pmx_model.get_joints()
        self.joints = joints
        if joints:
            joint_marker_size = 0.04 / self.model_scale if self.model_scale > 0 else 0.04
            jt_verts, jt_cols, jt_bone_idx = create_joint_batched(joints, self.rigidbodies, joint_marker_size)
            self._transform_to_normalized(jt_verts)
            self.joint_vao = self._create_wireframe_vao(jt_verts, jt_cols, jt_bone_idx)
            print(f"Created joint VAO: {len(jt_verts)} vertices ({len(joints)} joints batched)")
        else:
            self.joint_vao = None
            print(f"No joints found in model")

        self.dummy_texture = Texture(1, 1, 1, np.array([255], dtype='u1').tobytes())

    def _save_screenshot(self):
        glReadBuffer(GL_FRONT)
        img_data = glReadPixels(0, 0, self.width, self.height, GL_RGB, GL_UNSIGNED_BYTE)
        img = Image.frombytes('RGB', (self.width, self.height), img_data)
        img = img.transpose(Image.FLIP_TOP_BOTTOM)
        img.save('render_output.png')
        print("Screenshot saved as: render_output.png")

    def _save_view_history(self):
        history_data = {
            'camera_x': float(self.camera.x),
            'camera_y': float(self.camera.y),
            'camera_z': float(self.camera.z),
            'camera_rot_x': float(self.camera.rot_x),
            'camera_rot_y': float(self.camera.rot_y),
            'view_history': [{k: float(v) for k, v in h.items()} for h in self.view_history]
        }
        with open('view_history.json', 'w') as f:
            json.dump(history_data, f, indent=2)
        print("View history saved as: view_history.json")

    def render(self):
        last_time = glfw.get_time()

        while not glfw.window_should_close(self.window):
            glfw.poll_events()

            current_time = glfw.get_time()
            delta_time = current_time - last_time
            last_time = current_time

            self.camera.update(self.window, delta_time)

            glClearColor(0.15, 0.15, 0.2, 0.0)
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)
            glEnable(GL_DEPTH_TEST)
            glEnable(GL_BLEND)
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)
            
            glFrontFace(GL_CW)
            glDisable(GL_CULL_FACE)

            projection = Camera.create_projection_matrix(self.width, self.height)
            view = self.camera.create_view_matrix()

            coord_convert = np.diag([1, 1, -1, 1]).astype('f4')
            model = coord_convert

            if self.idle_animation_enabled and not self.animation_controller.vmd_playing:
                self.idle_time += delta_time
                breath = np.eye(4, dtype='f4')
                breath[1, 3] = np.sin(self.idle_time * 1.5) * self.idle_breath_intensity
                sway_angle = np.sin(self.idle_time * 0.7) * self.idle_sway_intensity
                sway = np.array([
                    [np.cos(sway_angle), 0, np.sin(sway_angle), 0],
                    [0, 1, 0, 0],
                    [-np.sin(sway_angle), 0, np.cos(sway_angle), 0],
                    [0, 0, 0, 1]
                ], dtype='f4')
                model = model @ breath @ sway

                if self.idle_blink_enabled and self.pmx_model:
                    self.idle_blink_time += delta_time
                    blink_weight = 0.0
                    if self.idle_blink_time >= self.idle_blink_interval:
                        blink_progress = (self.idle_blink_time - self.idle_blink_interval) / self.idle_blink_duration
                        if blink_progress < 0.5:
                            blink_weight = blink_progress * 2.0
                        else:
                            blink_weight = (1.0 - blink_progress) * 2.0
                        if blink_progress >= 1.0:
                            self.idle_blink_time = 0.0
                    if blink_weight > 0.0:
                        for morph_name in self.idle_blink_morph_names:
                            self.morph_controller.set_morph_weight(morph_name, blink_weight)
                    else:
                        for morph_name in self.idle_blink_morph_names:
                            if morph_name in self.morph_controller.morph_weights:
                                del self.morph_controller.morph_weights[morph_name]
                        self.morph_controller._update_morph_offsets()

            self.animation_controller.update(delta_time)
            if self.animation_controller.vmd_playing:
                self.animation_controller.apply_vmd_frame(self.morph_controller)

            self._render_axis_grid(projection, view)

            if self.show_model and self.show_outline and self.outline_vao and self.material_batches:
                glEnable(GL_CULL_FACE)
                glCullFace(GL_FRONT)

                if self.show_morph and self.skinned_morph_outline_vao:
                    outline_shader = self.shader_manager.get_program('morph_outline')
                    outline_vao = self.skinned_morph_outline_vao
                    glUseProgram(outline_shader)
                    self._set_uniform(outline_shader, "bone_texture_width", self.bone_texture_width)
                    self._set_uniform(outline_shader, "morph_weight", 1.0)
                    self.bone_texture.bind(1)
                elif self.show_skinned and self.skinned_outline_vao:
                    outline_shader = self.shader_manager.get_program('outline_skinned')
                    outline_vao = self.skinned_outline_vao
                    glUseProgram(outline_shader)
                    self._set_uniform(outline_shader, "bone_texture_width", self.bone_texture_width)
                    self.bone_texture.bind(1)
                else:
                    outline_shader = self.shader_manager.get_program('outline')
                    outline_vao = self.outline_vao
                    glUseProgram(outline_shader)

                self._set_uniform_matrix(outline_shader, "projection", projection.T)
                self._set_uniform_matrix(outline_shader, "view", view.T)
                self._set_uniform_matrix(outline_shader, "model", model.T)

                for batch in self.material_batches:
                    if not batch['has_edge']:
                        continue
                    
                    mat_idx = batch['material_index']
                    edge_info = self.material_edges.get(mat_idx, {})
                    edge_color = edge_info.get('color', (0.0, 0.0, 0.0, 1.0))
                    edge_size = edge_info.get('size', 1.0)
                    
                    tex_idx = batch['texture_index']
                    if 0 <= tex_idx < len(self.textures) and self.textures[tex_idx]:
                        self.textures[tex_idx].bind(0)
                    else:
                        self.dummy_texture.bind(0)
                    
                    alpha = self.morph_controller.get_material_alpha(mat_idx)
                    self._set_uniform(outline_shader, "alpha", alpha)
                    self._set_uniform(outline_shader, "outline_color", edge_color[:3])
                    self._set_uniform(outline_shader, "outline_thickness", edge_size * 0.001)
                    
                    outline_vao.render(GL_TRIANGLES, count=batch['count'], first=batch['first'])

                glDisable(GL_CULL_FACE)

            if self.show_model and self.model_vao and self.material_batches:
                if self.show_morph and self.skinned_morph_vao:
                    if self.show_toon:
                        shader = self.shader_manager.get_program('morph')
                        vao = self.skinned_morph_vao
                        glUseProgram(shader)
                        self._set_uniform(shader, "camera_pos", (self.camera.x, self.camera.y, self.camera.z))
                        glActiveTexture(GL_TEXTURE2)
                        glBindTexture(GL_TEXTURE_2D, self.shader_manager.gradient_texture)
                    else:
                        shader = self.shader_manager.get_program('morph_notoon')
                        vao = self.skinned_morph_vao_notoon
                        glUseProgram(shader)
                    self._set_uniform(shader, "bone_texture_width", self.bone_texture_width)
                    self._set_uniform(shader, "morph_weight", 1.0)
                    self.bone_texture.bind(1)
                    self._set_uniform(shader, "tex", 0)
                    self._set_uniform(shader, "bone_texture", 1)
                elif self.show_skinned and self.skinned_vao:
                    if self.show_toon:
                        shader = self.shader_manager.get_program('skinned')
                        vao = self.skinned_vao
                        glUseProgram(shader)
                        self._set_uniform(shader, "camera_pos", (self.camera.x, self.camera.y, self.camera.z))
                        glActiveTexture(GL_TEXTURE2)
                        glBindTexture(GL_TEXTURE_2D, self.shader_manager.gradient_texture)
                    else:
                        shader = self.shader_manager.get_program('skinned_notoon')
                        vao = self.skinned_vao_notoon
                        glUseProgram(shader)
                    self._set_uniform(shader, "bone_texture_width", self.bone_texture_width)
                    self.bone_texture.bind(1)
                    self._set_uniform(shader, "tex", 0)
                    self._set_uniform(shader, "bone_texture", 1)
                else:
                    shader = self.shader_manager.get_program('toon') if self.show_toon else self.shader_manager.get_program('main')
                    vao = self.toon_vao if self.show_toon else self.model_vao
                    glUseProgram(shader)
                    if self.show_toon:
                        self._set_uniform(shader, "camera_pos", (self.camera.x, self.camera.y, self.camera.z))
                        self._set_uniform(shader, "gradient_map", 1)
                        self._set_uniform(shader, "shadow_thresh", 0.0)
                        self._set_uniform(shader, "rim_power", 4.0)
                        self._set_uniform(shader, "rim_color", (1.0, 1.0, 1.0))
                        glActiveTexture(GL_TEXTURE1)
                        glBindTexture(GL_TEXTURE_2D, self.shader_manager.gradient_texture)

                self._set_uniform_matrix(shader, "projection", projection.T)
                self._set_uniform_matrix(shader, "view", view.T)
                self._set_uniform_matrix(shader, "model", model.T)
                self._set_uniform(shader, "light_dir", (0.0, 0.5, 1.0))
                self._set_uniform(shader, "tex", 0)
                self._set_uniform(shader, "sphere_tex", 3)
                self._set_uniform(shader, "toon_tex", 4)

                for batch in self.material_batches:
                    tex_idx = batch['texture_index']
                    if 0 <= tex_idx < len(self.textures) and self.textures[tex_idx]:
                        self.textures[tex_idx].bind(0)
                        self._set_uniform(shader, "has_texture", True)
                    else:
                        self._set_uniform(shader, "has_texture", False)
                        mat_idx = batch['material_index']
                        if mat_idx in self.material_colors:
                            color = self.material_colors[mat_idx]
                            self._set_uniform(shader, "material_color", color)

                    mat_idx = batch['material_index']
                    alpha = self.morph_controller.get_material_alpha(mat_idx)
                    self._set_uniform(shader, "alpha", alpha)
                    
                    if mat_idx in self.material_specular:
                        spec = self.material_specular[mat_idx]
                        self._set_uniform(shader, "specular_color", spec['color'])
                        self._set_uniform(shader, "specular_factor", spec['factor'])
                    else:
                        self._set_uniform(shader, "specular_color", (0.0, 0.0, 0.0))
                        self._set_uniform(shader, "specular_factor", 1.0)
                    
                    if mat_idx in self.material_ambient:
                        self._set_uniform(shader, "ambient_color", self.material_ambient[mat_idx])
                    else:
                        self._set_uniform(shader, "ambient_color", (0.5, 0.5, 0.5))
                    
                    sphere_info = self.material_sphere.get(mat_idx, {'texture_index': -1, 'mode': 0})
                    sphere_tex_idx = sphere_info['texture_index']
                    sphere_mode = sphere_info['mode']
                    
                    if sphere_tex_idx >= 0 and sphere_tex_idx < len(self.textures) and self.textures[sphere_tex_idx]:
                        glActiveTexture(GL_TEXTURE3)
                        self.textures[sphere_tex_idx].bind(3)
                        self._set_uniform(shader, "sphere_mode", sphere_mode)
                    else:
                        self._set_uniform(shader, "sphere_mode", 0)
                    
                    toon_info = self.material_toon.get(mat_idx, {'texture_index': -1, 'sharing_flag': 0})
                    toon_tex_idx = toon_info['texture_index']
                    
                    if toon_tex_idx >= 0 and toon_tex_idx < len(self.textures) and self.textures[toon_tex_idx]:
                        glActiveTexture(GL_TEXTURE4)
                        self.textures[toon_tex_idx].bind(4)
                        self._set_uniform(shader, "has_toon", True)
                    else:
                        self._set_uniform(shader, "has_toon", False)

                    vao.render(GL_TRIANGLES, count=batch['count'], first=batch['first'])

            self._render_physics(projection, view, model)

            glfw.swap_buffers(self.window)

            self._fps_frame_count += 1
            now = glfw.get_time()
            if now - self._fps_last_time >= 0.5:
                self._fps_display = self._fps_frame_count / (now - self._fps_last_time)
                self._fps_frame_count = 0
                self._fps_last_time = now
                glfw.set_window_title(self.window, f"{self._base_title}  [{self._fps_display:.0f} FPS]")

            self.view_history.append({
                'x': self.camera.x,
                'y': self.camera.y,
                'z': self.camera.z,
                'rot_x': self.camera.rot_x,
                'rot_y': self.camera.rot_y
            })

        self._save_screenshot()
        self._save_view_history()

    @staticmethod
    def print_help():
        print("FPS Camera Controls:")
        print("  Left mouse drag: Rotate camera view")
        print("  W/A/S/D: Move forward/left/backward/right")
        print("  E/Q: Move up/down")
        print("  Mouse scroll: Adjust movement speed")
        print("  X key: Toggle world axis display")
        print("  G key: Toggle ground grid display")
        print("  B key: Toggle rigidbody & joint display")
        print("  H key: Toggle model mesh display")
        print("  O key: Toggle outline display")
        print("  T key: Toggle toon shading")
        print("  K key: Toggle GPU skinning")
        print("  P key: Toggle VPD pose")
        print("  R key: Reset camera to default position")
        print("  I key: Toggle idle animation")
        print("  M key: Toggle morph mode")
        print("  < / > keys: Switch between morphs")
        print("  Up/Down keys: Adjust morph weight")
        print("VMD Animation Controls:")
        print("  Space: Play/Pause VMD animation")
        print("  L key: Toggle VMD loop")
        print("  [ / ] keys: Step backward/forward 30 frames")

    def close(self):
        glfw.terminate()
