import glfw
import moderngl
import numpy as np
from load_pmx import PmxModel
from PIL import Image
import os
import json


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
        # self.ctx.enable(moderngl.CULL_FACE)
        # self.ctx.cull_face = 'back'

        self._create_shaders()
        self._create_axis_shader()
        self._create_world_axis()

        self.camera_x = 0.0
        self.camera_y = 0.0
        self.camera_z = 10.0
        self.camera_rot_x = 0.0
        self.camera_rot_y = 0.0
        self.camera_speed = 20.0
        self.mouse_sensitivity = 0.05
        self.is_dragging = False
        self.is_panning = False
        self.last_mouse_x = 0
        self.last_mouse_y = 0
        self.view_history = []
        self.show_world_axis = True
        self.show_ground_grid = True

        glfw.set_mouse_button_callback(self.window, self._on_mouse_button)
        glfw.set_cursor_pos_callback(self.window, self._on_cursor_pos)
        glfw.set_key_callback(self.window, self._on_key)
        glfw.set_scroll_callback(self.window, self._on_scroll)

        self.model_center = [0, 0, 0]
        self.model_scale = 1.0

    def _create_shaders(self):
        vertex_shader = """
        #version 330 core

        in vec3 in_position;
        in vec3 in_normal;
        in vec2 in_uv;

        uniform mat4 model;
        uniform mat4 view;
        uniform mat4 projection;

        out vec3 v_normal;
        out vec2 v_uv;
        out vec3 v_position;

        void main() {
            vec4 world_pos = model * vec4(in_position, 1.0);
            gl_Position = projection * view * world_pos;
            v_normal = normalize(mat3(model) * in_normal);
            v_uv = in_uv;
            v_position = world_pos.xyz;
        }
        """

        fragment_shader = """
        #version 330 core

        in vec3 v_normal;
        in vec2 v_uv;
        in vec3 v_position;

        uniform vec3 light_dir;
        uniform sampler2D tex;
        uniform bool has_texture;

        out vec4 fragColor;

        void main() {
            vec3 normal = normalize(v_normal);
            vec3 light = normalize(light_dir);
            
            float diff = max(dot(normal, light), 0.0);
            float ambient = 0.6;
            
            vec3 color;
            if (has_texture) {
                vec4 tex_color = texture(tex, v_uv);
                color = tex_color.rgb;
                // 处理透明区域
                if (tex_color.a < 0.1) {
                    discard;
                }
            } else {
                float height_shade = (v_position.y + 10.0) / 20.0;
                color = vec3(0.9 * height_shade + 0.3, 0.7 * height_shade + 0.2, 0.6 * height_shade + 0.2);
            }
            
            vec3 result = color * (ambient + diff * 0.4);
            result = clamp(result, 0.0, 1.0);
            fragColor = vec4(result, 1.0);
        }
        """

        self.program = self.ctx.program(vertex_shader=vertex_shader, fragment_shader=fragment_shader)
        self.program["light_dir"] = (1.0, 1.0, 0.5)
        self.program["has_texture"] = False
        self.program["tex"] = 0

    def _create_axis_shader(self):
        vertex_shader = """
        #version 330 core

        in vec3 in_position;
        in vec3 in_color;

        uniform mat4 model;
        uniform mat4 view;
        uniform mat4 projection;

        out vec3 v_color;

        void main() {
            gl_Position = projection * view * model * vec4(in_position, 1.0);
            v_color = in_color;
        }
        """

        fragment_shader = """
        #version 330 core

        in vec3 v_color;
        out vec4 fragColor;

        void main() {
            fragColor = vec4(v_color, 1.0);
        }
        """

        self.axis_program = self.ctx.program(vertex_shader=vertex_shader, fragment_shader=fragment_shader)

    def _create_world_axis(self):
        axis_length = 50.0
        
        axis_vertices = []
        arrow_size = 3.0
        
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

        for i in range(-grid_divisions//2, grid_divisions//2 + 1):
            x = i * grid_step
            grid_vertices.extend([x, 0.0, -grid_size/2, 0.5, 0.5, 0.5])
            grid_vertices.extend([x, 0.0, grid_size/2, 0.5, 0.5, 0.5])

            z = i * grid_step
            grid_vertices.extend([-grid_size/2, 0.0, z, 0.5, 0.5, 0.5])
            grid_vertices.extend([grid_size/2, 0.0, z, 0.5, 0.5, 0.5])

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
        if button == glfw.MOUSE_BUTTON_LEFT:
            if action == glfw.PRESS:
                self.is_panning = True
                self.last_mouse_x, self.last_mouse_y = glfw.get_cursor_pos(window)
            else:
                self.is_panning = False

    def _on_cursor_pos(self, window, xpos, ypos):
        if self.is_panning:
            dx = xpos - self.last_mouse_x
            dy = ypos - self.last_mouse_y

            self.camera_rot_y -= dx * self.mouse_sensitivity
            self.camera_rot_x -= dy * self.mouse_sensitivity

            self.camera_rot_x = max(-89, min(89, self.camera_rot_x))

            self.last_mouse_x = xpos
            self.last_mouse_y = ypos

    def _on_scroll(self, window, xoffset, yoffset):
        self.camera_speed += yoffset * 0.5
        self.camera_speed = max(1.0, min(20.0, self.camera_speed))
        print(f"Camera speed: {self.camera_speed:.1f}")

    def _on_key(self, window, key, scancode, action, mods):
        if key == glfw.KEY_ESCAPE and action == glfw.PRESS:
            glfw.set_input_mode(window, glfw.CURSOR, glfw.CURSOR_NORMAL)
            self.is_panning = False
        
        if key == glfw.KEY_X and action == glfw.PRESS:
            self.show_world_axis = not self.show_world_axis
            print(f"World axis: {'ON' if self.show_world_axis else 'OFF'}")
        
        if key == glfw.KEY_G and action == glfw.PRESS:
            self.show_ground_grid = not self.show_ground_grid
            print(f"Ground grid: {'ON' if self.show_ground_grid else 'OFF'}")
        
        if key == glfw.KEY_R and action == glfw.PRESS:
            self.camera_x = 0.0
            self.camera_y = 0.0
            self.camera_z = 10.0
            self.camera_rot_x = 0.0
            self.camera_rot_y = 0.0
            print("Camera reset to default position")

    def _update_camera(self, delta_time):
        speed = self.camera_speed * delta_time
        
        rot_y_rad = np.radians(self.camera_rot_y)
        rot_x_rad = np.radians(self.camera_rot_x)
        
        front_x = -np.sin(rot_y_rad) * np.cos(rot_x_rad)
        front_y = np.sin(rot_x_rad)
        front_z = -np.cos(rot_y_rad) * np.cos(rot_x_rad)
        
        right_x = np.cos(rot_y_rad)
        right_y = 0
        right_z = -np.sin(rot_y_rad)
        
        if glfw.get_key(self.window, glfw.KEY_W) == glfw.PRESS:
            self.camera_x += front_x * speed
            self.camera_y += front_y * speed
            self.camera_z += front_z * speed
        if glfw.get_key(self.window, glfw.KEY_S) == glfw.PRESS:
            self.camera_x -= front_x * speed
            self.camera_y -= front_y * speed
            self.camera_z -= front_z * speed
        if glfw.get_key(self.window, glfw.KEY_A) == glfw.PRESS:
            self.camera_x -= right_x * speed
            self.camera_z -= right_z * speed
        if glfw.get_key(self.window, glfw.KEY_D) == glfw.PRESS:
            self.camera_x += right_x * speed
            self.camera_z += right_z * speed
        if glfw.get_key(self.window, glfw.KEY_E) == glfw.PRESS:
            self.camera_y += speed
        if glfw.get_key(self.window, glfw.KEY_Q) == glfw.PRESS:
            self.camera_y -= speed
        
        # 调试：打印相机位置
        if glfw.get_key(self.window, glfw.KEY_W) == glfw.PRESS or            glfw.get_key(self.window, glfw.KEY_S) == glfw.PRESS or            glfw.get_key(self.window, glfw.KEY_A) == glfw.PRESS or            glfw.get_key(self.window, glfw.KEY_D) == glfw.PRESS or            glfw.get_key(self.window, glfw.KEY_E) == glfw.PRESS or            glfw.get_key(self.window, glfw.KEY_Q) == glfw.PRESS:
            print(f"Camera: pos=({self.camera_x:.2f}, {self.camera_y:.2f}, {self.camera_z:.2f}), speed={speed:.4f}")

    def _create_projection(self):
        aspect = self.width / self.height
        fov = 45.0
        near = 0.01
        far = 1000.0

        f = 1.0 / np.tan(np.radians(fov) / 2)
        proj = np.array([
            [f / aspect, 0, 0, 0],
            [0, f, 0, 0],
            [0, 0, -(far + near) / (far - near), -2.0 * far * near / (far - near)],
            [0, 0, -1, 0]
        ], dtype='f4')
        return proj

    def _create_view(self):
        rot_x = np.radians(self.camera_rot_x)
        rot_y = np.radians(self.camera_rot_y)

        cx, cy = np.cos(rot_x), np.cos(rot_y)
        sx, sy = np.sin(rot_x), np.sin(rot_y)

        # 构建正确的FPS相机视图矩阵
        # Y轴旋转矩阵
        rot_y_mat = np.array([
            [cy, 0, -sy, 0],
            [0, 1, 0, 0],
            [sy, 0, cy, 0],
            [0, 0, 0, 1]
        ], dtype='f4')
        
        # X轴旋转矩阵
        rot_x_mat = np.array([
            [1, 0, 0, 0],
            [0, cx, sx, 0],
            [0, -sx, cx, 0],
            [0, 0, 0, 1]
        ], dtype='f4')
        
        # 平移矩阵
        trans = np.array([
            [1, 0, 0, -self.camera_x],
            [0, 1, 0, -self.camera_y],
            [0, 0, 1, -self.camera_z],
            [0, 0, 0, 1]
        ], dtype='f4')
        
        # 视图矩阵 = 旋转 * 平移
        return rot_x_mat @ rot_y_mat @ trans

    def load_model(self, pmx_model: PmxModel, texture_dir: str = ""):
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
            y = (v.position[1] - center[1]) * self.model_scale
            z = (v.position[2] - center[2]) * self.model_scale
            nx = v.normal[0]
            ny = v.normal[1]
            nz = v.normal[2]
            vertices.extend([x, y, z, nx, ny, nz, v.uv[0], v.uv[1]])

        vertices = np.array(vertices, dtype='f4')
        indices = np.array(list(pmx_model.indices), dtype='i4')

        self.vao = self.ctx.vertex_array(
            self.program,
            [
                (self.ctx.buffer(vertices), '3f 3f 2f', 'in_position', 'in_normal', 'in_uv'),
            ],
            self.ctx.buffer(indices)
        )

        self.index_count = len(indices)

        # 调试：打印材质详细信息
        print(f"\nMaterial details:")
        for i, mat in enumerate(pmx_model.materials):
            print(f"  Mat {i}: tex={mat.texture_index}, sphere={getattr(mat, 'sphere_texture_index', -1)}, toon={getattr(mat, 'toon_texture_index', -1)}, name={getattr(mat, 'name', 'N/A')}")
        
        # 加载所有纹理
        print(f"\nLoading {len(pmx_model.textures)} textures...")
        self.textures = []
        if texture_dir and os.path.exists(texture_dir):
            for i, tex_name in enumerate(pmx_model.textures):
                tex_path = os.path.join(texture_dir, tex_name)
                if os.path.exists(tex_path):
                    try:
                        img = Image.open(tex_path).convert('RGBA')
                        tex = self.ctx.texture(img.size, 4, img.tobytes())
                        tex.filter = (moderngl.LINEAR, moderngl.LINEAR)
                        tex.repeat_x = False
                        tex.repeat_y = False
                        self.textures.append(tex)
                        print(f"  [{i}] {tex_name}")
                    except Exception as e:
                        print(f"  [{i}] Failed: {tex_name} - {e}")
                        self.textures.append(None)
                else:
                    print(f"  [{i}] Not found: {tex_name}")
                    self.textures.append(None)
        
        # 构建材质渲染批次
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
        
        # 调试：检查body材质
        for i, batch in enumerate(self.material_batches):
            mat = pmx_model.materials[i]
            if 'body' in mat.name.lower() or 'dress' in mat.name.lower():
                tex_idx = batch['texture_index']
                tex_name = pmx_model.textures[tex_idx] if 0 <= tex_idx < len(pmx_model.textures) else 'N/A'
                print(f"  {mat.name}: uses texture[{tex_idx}] = {tex_name}, indices {batch['first']}-{batch['first']+batch['count']}")

    def _save_screenshot(self):
        img_data = self.ctx.fbo.read(components=3)
        img = Image.frombytes('RGB', (self.width, self.height), img_data)
        img = img.transpose(Image.FLIP_TOP_BOTTOM)
        img.save('render_output.png')
        print("Screenshot saved as: render_output.png")

    def _save_view_history(self):
        history_data = {
            'camera_x': float(self.camera_x),
            'camera_y': float(self.camera_y),
            'camera_z': float(self.camera_z),
            'camera_rot_x': float(self.camera_rot_x),
            'camera_rot_y': float(self.camera_rot_y),
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
            
            self._update_camera(delta_time)

            self.ctx.clear(0.15, 0.15, 0.2)
            self.ctx.enable(moderngl.DEPTH_TEST)

            projection = self._create_projection()
            view = self._create_view()
            model = np.eye(4, dtype='f4')

            # 再渲染辅助线条（禁用深度测试）
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
            
            # 先渲染模型（需要深度测试）
            if hasattr(self, 'vao') and hasattr(self, 'material_batches'):
                self.program["projection"].write(projection.T.tobytes())
                self.program["view"].write(view.T.tobytes())
                self.program["model"].write(model.tobytes())
                
                # 按材质分批渲染
                for i, batch in enumerate(self.material_batches):
                    tex_idx = batch['texture_index']
                    if 0 <= tex_idx < len(self.textures) and self.textures[tex_idx]:
                        self.textures[tex_idx].use(0)
                        self.program["has_texture"] = True
                    else:
                        self.program["has_texture"] = False
                    
                    self.vao.render(moderngl.TRIANGLES, vertices=batch['count'], first=batch['first'])

            glfw.swap_buffers(self.window)

            self.view_history.append({
                'x': self.camera_x,
                'y': self.camera_y,
                'z': self.camera_z,
                'rot_x': self.camera_rot_x,
                'rot_y': self.camera_rot_y
            })

        self._save_screenshot()
        self._save_view_history()

    def close(self):
        glfw.terminate()


def main():
    model_path = "resources/ikaros-origin/Ikaros.pmx"
    texture_dir = "resources/ikaros-origin"

    print(f"Loading model: {model_path}")
    model = PmxModel(model_path)
    print(f"Model: {model.name}, Vertices: {model.vertex_count}, Faces: {model.face_count}")
    print(f"Textures: {model.textures}")

    print("Initializing renderer...")
    renderer = Renderer(1280, 720, f"PMX Viewer - {model.name}")

    print("Loading model to GPU...")
    renderer.load_model(model, texture_dir)

    print("FPS Camera Controls:")
    print("  Left mouse drag: Rotate camera view")
    print("  W/A/S/D: Move forward/left/backward/right")
    print("  SPACE: Move up")
    print("  LEFT_SHIFT: Move down")
    print("  Mouse scroll: Adjust movement speed")
    print("  X key: Toggle world axis display")
    print("  G key: Toggle ground grid display")
    print("  R key: Reset camera to default position")
    print("Starting render loop...")
    renderer.render()
    renderer.close()


if __name__ == "__main__":
    main()