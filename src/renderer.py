import glfw
import moderngl
import numpy as np
from load_pmx import PmxModel
from PIL import Image
import os
import json
from pathlib import Path

from camera import Camera


def _load_shader(filename):
    path = Path(__file__).parent.parent / "resources" / "shaders" / filename
    with open(path, "r", encoding="utf-8") as f:
        return f.read()


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

        self.window = glfw.create_window(width, height, title, None, None)
        if not self.window:
            glfw.terminate()
            raise RuntimeError("Failed to create window")

        glfw.make_context_current(self.window)
        glfw.set_framebuffer_size_callback(self.window, self._on_resize)

        self.ctx = moderngl.create_context()
        self.ctx.enable(moderngl.DEPTH_TEST)

        self.show_outline = True
        self.outline_thickness = 0.003
        self.show_toon = True

        self._create_shaders()
        self._create_toon_shaders()
        self._create_skinned_shaders()
        self._create_axis_shader()
        self._create_outline_shader()
        self._create_world_axis()

        self.camera = Camera()

        self.show_world_axis = True
        self.show_ground_grid = True

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

        self.model_vao = None
        self.outline_vao = None
        self.toon_vao = None
        self.skinned_vao = None
        self.pmx_model = None
        self.textures = []
        self.material_batches = []
        self.index_count = 0
        
        self.bone_texture = None
        self.bone_texture_width = 0
        self.show_skinned = False

    def _create_shaders(self):
        self.program = self.ctx.program(
            vertex_shader=_load_shader("main.vert"),
            fragment_shader=_load_shader("main.frag")
        )
        self.program["light_dir"] = (0.0, 0.5, -1.0)
        self.program["has_texture"] = False
        self.program["tex"] = 0

    def _create_axis_shader(self):
        self.axis_program = self.ctx.program(
            vertex_shader=_load_shader("axis.vert"),
            fragment_shader=_load_shader("axis.frag")
        )

    def _create_outline_shader(self):
        self.outline_program = self.ctx.program(
            vertex_shader=_load_shader("outline.vert"),
            fragment_shader=_load_shader("outline.frag")
        )
        self.outline_program["outline_color"] = (0.0, 0.0, 0.0)
        self.outline_program["outline_thickness"] = self.outline_thickness
        self.outline_program["tex"] = 0

        self.outline_skinned_program = self.ctx.program(
            vertex_shader=_load_shader("outline_skinned.vert"),
            fragment_shader=_load_shader("outline.frag")
        )
        self.outline_skinned_program["outline_color"] = (0.0, 0.0, 0.0)
        self.outline_skinned_program["outline_thickness"] = self.outline_thickness
        self.outline_skinned_program["tex"] = 0
        self.outline_skinned_program["bone_texture"] = 1
        self.outline_skinned_program["bone_texture_width"] = 64

    def _create_toon_shaders(self):
        self.toon_program = self.ctx.program(
            vertex_shader=_load_shader("toon.vert"),
            fragment_shader=_load_shader("toon.frag")
        )
        self.toon_program["light_dir"] = (0.0, 0.5, -1.0)
        self.toon_program["shadow_thresh"] = 0.2
        self.toon_program["rim_power"] = 3.0
        self.toon_program["rim_color"] = (1.0, 0.95, 0.9)
        self.toon_program["has_texture"] = False
        self.toon_program["tex"] = 0
        self.toon_program["gradient_map"] = 1

        gradient_data = np.array([
            [60, 60, 60],
            [120, 120, 120],
            [180, 180, 180],
            [220, 220, 220]
        ], dtype='u1')
        self.gradient_texture = self.ctx.texture((4, 1), 3, gradient_data.tobytes())
        self.gradient_texture.filter = (moderngl.LINEAR, moderngl.LINEAR)

    def _create_skinned_shaders(self):
        self.skinned_program = self.ctx.program(
            vertex_shader=_load_shader("skinned.vert"),
            fragment_shader=_load_shader("toon.frag")
        )
        self.skinned_program["light_dir"] = (0.0, 0.5, -1.0)
        self.skinned_program["has_texture"] = False
        self.skinned_program["tex"] = 0
        self.skinned_program["bone_texture"] = 1
        self.skinned_program["gradient_map"] = 2
        self.skinned_program["shadow_thresh"] = 0.2
        self.skinned_program["rim_power"] = 3.0
        self.skinned_program["rim_color"] = (1.0, 0.95, 0.9)
        self.skinned_debug = False
        self.skinned_debug_scale = 1.5

    def _reload_bone_texture(self, debug_scale=1.0):
        """重新加载骨骼纹理（用于调试模式）"""
        if self.pmx_model is None:
            return
        bone_tex_data, tex_width, tex_height = self.pmx_model.get_bone_texture_data(debug_scale)
        self.bone_texture.write(bone_tex_data.tobytes())

    def _create_world_axis(self):
        axis_length = 50.0
        arrow_size = 3.0

        axis_vertices = []
        for axis in range(3):
            start = [0.0, 0.0, 0.0]
            end = [0.0, 0.0, 0.0]
            color = [0.0, 0.0, 0.0]

            end[axis] = axis_length
            color[axis] = 1.0

            axis_vertices.extend(start + color)
            axis_vertices.extend(end + color)

            start_neg = [0.0, 0.0, 0.0]
            start_neg[axis] = -axis_length
            axis_vertices.extend(start + color)
            axis_vertices.extend(start_neg + color)

            base = end.copy()
            base[axis] -= arrow_size

            perp1 = [0.0, 0.0, 0.0]
            perp2 = [0.0, 0.0, 0.0]
            perp_indices = [(axis + 1) % 3, (axis + 2) % 3]
            perp1[perp_indices[0]] = arrow_size * 0.5
            perp2[perp_indices[1]] = arrow_size * 0.5

            arrow_tip1 = [base[i] + perp1[i] for i in range(3)]
            arrow_tip2 = [base[i] + perp2[i] for i in range(3)]

            axis_vertices.extend(end + color)
            axis_vertices.extend(arrow_tip1 + color)
            axis_vertices.extend(end + color)
            axis_vertices.extend(arrow_tip2 + color)

        axis_vertices = np.array(axis_vertices, dtype='f4')
        self.axis_vbo = self.ctx.buffer(axis_vertices)
        self.axis_vao = self.ctx.vertex_array(
            self.axis_program,
            [(self.axis_vbo, '3f 3f', 'in_position', 'in_color')],
        )

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
        self.grid_vbo = self.ctx.buffer(grid_vertices)
        self.grid_vao = self.ctx.vertex_array(
            self.axis_program,
            [(self.grid_vbo, '3f 3f', 'in_position', 'in_color')],
        )

    def _on_resize(self, window, width, height):
        self.width = width
        self.height = height
        self.ctx.viewport = (0, 0, width, height)

    def _on_mouse_button(self, window, button, action, mods):
        self.camera.on_mouse_button(window, button, action, mods)

    def _on_cursor_pos(self, window, xpos, ypos):
        self.camera.on_cursor_pos(window, xpos, ypos)

    def _on_scroll(self, window, xoffset, yoffset):
        self.camera.on_scroll(window, xoffset, yoffset)
        print(f"Camera speed: {self.camera.speed:.1f}")

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

        if key == glfw.KEY_T and action == glfw.PRESS:
            self.show_toon = not self.show_toon
            print(f"Toon shading: {'ON' if self.show_toon else 'OFF'}")

        if key == glfw.KEY_K and action == glfw.PRESS:
            self.show_skinned = not self.show_skinned
            self.skinned_debug = not self.skinned_debug
            if self.skinned_debug:
                self._reload_bone_texture(self.skinned_debug_scale)
                print(f"Skinned rendering: ON (debug scale={self.skinned_debug_scale})")
            else:
                self._reload_bone_texture(1.0)
                print(f"Skinned rendering: OFF")

        if key == glfw.KEY_R and action == glfw.PRESS:
            self.camera.reset()
            print("Camera reset to default position")

        if key == glfw.KEY_I and action == glfw.PRESS:
            self.idle_animation_enabled = not self.idle_animation_enabled
            print(f"Idle animation: {'ON' if self.idle_animation_enabled else 'OFF'}")

    def load_model(self, pmx_model: PmxModel, texture_dir: str = ""):
        self.pmx_model = pmx_model
        positions = np.array([(v.position[0], v.position[1], v.position[2]) for v in pmx_model.vertices], dtype='f4')

        min_pos = positions.min(axis=0)
        max_pos = positions.max(axis=0)
        center = (min_pos + max_pos) / 2
        size = max_pos - min_pos
        max_size = max(size)

        print(f"Model bounds: min={min_pos}, max={max_pos}")
        print(f"Model center: {center}, max dimension: {max_size}")

        self.model_center = center.tolist()
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

        self.model_vao = self.ctx.vertex_array(
            self.program,
            [
                (self.ctx.buffer(vertices), '3f 3f 2f', 'in_position', 'in_normal', 'in_uv'),
            ],
            self.ctx.buffer(indices)
        )

        self.toon_vao = self.ctx.vertex_array(
            self.toon_program,
            [
                (self.ctx.buffer(vertices), '3f 3f 2f', 'in_position', 'in_normal', 'in_uv'),
            ],
            self.ctx.buffer(indices)
        )

        self.outline_vao = self.ctx.vertex_array(
            self.outline_program,
            [
                (self.ctx.buffer(vertices), '3f 3f 2f', 'in_position', 'in_normal', 'in_uv'),
            ],
            self.ctx.buffer(indices)
        )

        print(f"\nLoading skeleton data...")
        skinning_data = pmx_model.get_all_skinning_vertex_data()
        bone_tex_data, tex_width, tex_height = pmx_model.get_bone_texture_data()
        
        print(f"  Bone count: {pmx_model.bone_count}")
        print(f"  Bone texture size: {tex_width}x{tex_height}")
        
        skinned_positions = np.zeros_like(skinning_data['positions'])
        for i in range(len(skinned_positions) // 3):
            skinned_positions[i*3] = (skinning_data['positions'][i*3] - center[0]) * self.model_scale
            skinned_positions[i*3 + 1] = (skinning_data['positions'][i*3 + 1] - min_pos[1]) * self.model_scale
            skinned_positions[i*3 + 2] = (skinning_data['positions'][i*3 + 2] - center[2]) * self.model_scale
        
        self.bone_texture = self.ctx.texture((tex_width, tex_height), 4, bone_tex_data.tobytes(), dtype='f4')
        self.bone_texture.filter = (moderngl.NEAREST, moderngl.NEAREST)
        self.bone_texture.repeat_x = False
        self.bone_texture.repeat_y = False
        self.bone_texture_width = tex_width
        
        self.skinned_vao = self.ctx.vertex_array(
            self.skinned_program,
            [
                (self.ctx.buffer(skinned_positions), '3f', 'in_position'),
                (self.ctx.buffer(skinning_data['normals']), '3f', 'in_normal'),
                (self.ctx.buffer(skinning_data['uvs']), '2f', 'in_uv'),
                (self.ctx.buffer(skinning_data['bone_indices']), '4i', 'in_bone_indices'),
                (self.ctx.buffer(skinning_data['bone_weights']), '4f', 'in_bone_weights'),
            ],
            self.ctx.buffer(indices)
        )

        self.skinned_outline_vao = self.ctx.vertex_array(
            self.outline_skinned_program,
            [
                (self.ctx.buffer(skinned_positions), '3f', 'in_position'),
                (self.ctx.buffer(skinning_data['normals']), '3f', 'in_normal'),
                (self.ctx.buffer(skinning_data['uvs']), '2f', 'in_uv'),
                (self.ctx.buffer(skinning_data['bone_indices']), '4i', 'in_bone_indices'),
                (self.ctx.buffer(skinning_data['bone_weights']), '4f', 'in_bone_weights'),
            ],
            self.ctx.buffer(indices)
        )

        self.index_count = len(indices)

        print(f"\nMaterial details:")
        for i, mat in enumerate(pmx_model.materials):
            print(f"  Mat {i}: tex={mat.texture_index}, sphere={getattr(mat, 'sphere_texture_index', -1)}, toon={getattr(mat, 'toon_texture_index', -1)}, name={getattr(mat, 'name', 'N/A')}")

        print(f"\nLoading {len(pmx_model.textures)} textures...")
        self.textures = []
        if texture_dir and os.path.exists(texture_dir):
            for i, tex_name in enumerate(pmx_model.textures):
                tex_name_normalized = tex_name.replace('\\', '/')
                tex_path = os.path.join(texture_dir, tex_name_normalized)
                if not os.path.exists(tex_path):
                    parts = tex_name_normalized.split('/')
                    if 'textures' in parts:
                        idx = parts.index('textures')
                        tex_name_without_prefix = '/'.join(parts[idx + 1:])
                        tex_path = os.path.join(texture_dir, tex_name_without_prefix)
                if os.path.exists(tex_path):
                    try:
                        img = Image.open(tex_path).convert('RGBA')
                        tex = self.ctx.texture(img.size, 4, img.tobytes())
                        tex.filter = (moderngl.LINEAR, moderngl.LINEAR)
                        tex.repeat_x = False
                        tex.repeat_y = False
                        self.textures.append(tex)
                        print(f"  [{i}] OK: {os.path.basename(tex_path)}")
                    except Exception as e:
                        print(f"  [{i}] Failed: {tex_name} - {e}")
                        self.textures.append(None)
                else:
                    print(f"  [{i}] Not found: {tex_name}")
                    self.textures.append(None)

        self.material_batches = []
        index_offset = 0
        for mat in pmx_model.materials:
            batch = {
                'first': index_offset,
                'count': mat.vertex_count,
                'texture_index': mat.texture_index
            }
            self.material_batches.append(batch)
            index_offset += mat.vertex_count

        print(f"Created {len(self.material_batches)} material batches, total indices: {index_offset}")

    def _save_screenshot(self):
        img_data = self.ctx.fbo.read(components=3)
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

            self.ctx.clear(0.15, 0.15, 0.2, 0.0)
            self.ctx.enable(moderngl.DEPTH_TEST)

            projection = Camera.create_projection_matrix(self.width, self.height)
            view = self.camera.create_view_matrix()
            model = np.eye(4, dtype='f4')

            if self.idle_animation_enabled:
                self.idle_time += delta_time
                breath = np.eye(4, dtype='f4')
                breath[3, 1] = np.sin(self.idle_time * 1.5) * self.idle_breath_intensity
                sway_angle = np.sin(self.idle_time * 0.7) * self.idle_sway_intensity
                sway = np.array([
                    [np.cos(sway_angle), 0, np.sin(sway_angle), 0],
                    [0, 1, 0, 0],
                    [-np.sin(sway_angle), 0, np.cos(sway_angle), 0],
                    [0, 0, 0, 1]
                ], dtype='f4')
                model = model @ breath @ sway

            if self.show_ground_grid:
                self.ctx.disable(moderngl.DEPTH_TEST)
                self.axis_program["projection"].write(projection.T.tobytes())
                self.axis_program["view"].write(view.T.tobytes())
                self.axis_program["model"].write(model.tobytes())
                self.ctx.line_width = 2.0
                self.grid_vao.render(moderngl.LINES)
                self.ctx.enable(moderngl.DEPTH_TEST)

            if self.show_world_axis:
                self.ctx.disable(moderngl.DEPTH_TEST)
                self.axis_program["projection"].write(projection.T.tobytes())
                self.axis_program["view"].write(view.T.tobytes())
                self.axis_program["model"].write(model.tobytes())
                self.ctx.line_width = 3.0
                self.axis_vao.render(moderngl.LINES)
                self.ctx.enable(moderngl.DEPTH_TEST)

            if self.show_outline and self.outline_vao and self.material_batches:
                self.ctx.enable(moderngl.CULL_FACE)
                self.ctx.cull_face = 'front'

                if self.show_skinned and self.skinned_outline_vao:
                    outline_shader = self.outline_skinned_program
                    outline_vao = self.skinned_outline_vao
                    outline_shader["bone_texture_width"] = self.bone_texture_width
                    self.bone_texture.use(1)
                else:
                    outline_shader = self.outline_program
                    outline_vao = self.outline_vao

                outline_shader["projection"].write(projection.T.tobytes())
                outline_shader["view"].write(view.T.tobytes())
                outline_shader["model"].write(model.tobytes())
                outline_shader["outline_thickness"] = self.outline_thickness

                for batch in self.material_batches:
                    tex_idx = batch['texture_index']
                    if 0 <= tex_idx < len(self.textures) and self.textures[tex_idx]:
                        self.textures[tex_idx].use(0)
                    else:
                        self.ctx.texture((1, 1), 1).use(0)
                    outline_vao.render(moderngl.TRIANGLES, vertices=batch['count'], first=batch['first'])

                self.ctx.disable(moderngl.CULL_FACE)

            if self.model_vao and self.material_batches:
                if self.show_skinned and self.skinned_vao:
                    shader = self.skinned_program
                    vao = self.skinned_vao
                    shader["bone_texture_width"] = self.bone_texture_width
                    self.bone_texture.use(1)
                    shader["camera_pos"] = (self.camera.x, self.camera.y, self.camera.z)
                    self.gradient_texture.use(2)
                else:
                    shader = self.toon_program if self.show_toon else self.program
                    vao = self.toon_vao if self.show_toon else self.model_vao
                    if self.show_toon:
                        shader["camera_pos"] = (self.camera.x, self.camera.y, self.camera.z)
                        self.gradient_texture.use(1)

                shader["projection"].write(projection.T.tobytes())
                shader["view"].write(view.T.tobytes())
                shader["model"].write(model.tobytes())
                shader["light_dir"] = (0.0, 0.5, -1.0)

                for batch in self.material_batches:
                    tex_idx = batch['texture_index']
                    if 0 <= tex_idx < len(self.textures) and self.textures[tex_idx]:
                        self.textures[tex_idx].use(0)
                        shader["has_texture"] = True
                    else:
                        shader["has_texture"] = False

                    vao.render(moderngl.TRIANGLES, vertices=batch['count'], first=batch['first'])

            glfw.swap_buffers(self.window)

            self.view_history.append({
                'x': self.camera.x,
                'y': self.camera.y,
                'z': self.camera.z,
                'rot_x': self.camera.rot_x,
                'rot_y': self.camera.rot_y
            })

        self._save_screenshot()
        self._save_view_history()

    def close(self):
        glfw.terminate()


MODELS = {
    "ikaros-origin": ("resources/models/ikaros-origin/Ikaros.pmx", "resources/models/ikaros-origin"),
    "ikaros-uniform": ("resources/models/ikaros-uniform/Ikaros.pmx", "resources/models/ikaros-uniform"),
    "安比": ("resources/models/安比/安比.pmx", "resources/models/安比"),
    "刀": ("resources/models/安比/刀.pmx", "resources/models/安比"),
    "chloe": ("resources/models/Chloe_Uniform1_0.9/Chloe_Uniform1_0.9.pmx", "resources/models/Chloe_Uniform1_0.9/textures"),
    "aqua-swimwear": ("resources/models/Aqua_Swimwear_1.0/Aqua_Swimwear_1.0.pmx", "resources/models/Aqua_Swimwear_1.0/textures"),
    "marine-swimwear": ("resources/models/Marine_Swmwear_1.01/Marine_Swmwear_1.01.pmx", "resources/models/Marine_Swmwear_1.01/textures"),
    "aqua-basebody": ("resources/models/Aqua_BaseBody_R15_0.9/Aqua_BaseBody_R15_0.9.pmx", "resources/models/Aqua_BaseBody_R15_0.9/textures"),
}


def main():
    import argparse

    parser = argparse.ArgumentParser(description="PMX Model Viewer")
    parser.add_argument("--model", "-m", default="ikaros-origin", choices=MODELS.keys(), help="Model to load")
    args = parser.parse_args()

    proj_root = Path(__file__).parent.parent
    pmx_path, tex_dir = MODELS[args.model]
    model_path = proj_root / pmx_path
    texture_dir = proj_root / tex_dir

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
    renderer.load_model(model, texture_dir)

    print("FPS Camera Controls:")
    print("  Left mouse drag: Rotate camera view")
    print("  W/A/S/D: Move forward/left/backward/right")
    print("  E/Q: Move up/down")
    print("  Mouse scroll: Adjust movement speed")
    print("  X key: Toggle world axis display")
    print("  G key: Toggle ground grid display")
    print("  O key: Toggle outline display")
    print("  T key: Toggle toon shading")
    print("  K key: Toggle GPU skinning")
    print("  R key: Reset camera to default position")
    print("  I key: Toggle idle animation")
    print("Starting render loop...")
    renderer.render()
    renderer.close()


if __name__ == "__main__":
    main()
