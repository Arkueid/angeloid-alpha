import numpy as np
import glfw


class Camera:
    def __init__(self):
        self.x = 0.0
        self.y = 0.0
        self.z = 10.0
        self.rot_x = 0.0
        self.rot_y = 0
        self.speed = 5.0
        self.mouse_sensitivity = 0.1
        self.is_panning = False
        self.last_mouse_x = 0
        self.last_mouse_y = 0

    def reset(self):
        self.x = 0.0
        self.y = 0.0
        self.z = 10.0
        self.rot_x = 0.0
        self.rot_y = 0

    def update(self, window, delta_time):
        speed = self.speed * delta_time

        rot_y_rad = np.radians(self.rot_y)
        rot_x_rad = np.radians(self.rot_x)

        front_x = -np.sin(rot_y_rad) * np.cos(rot_x_rad)
        front_y = np.sin(rot_x_rad)
        front_z = -np.cos(rot_y_rad) * np.cos(rot_x_rad)

        right_x = np.cos(rot_y_rad)
        right_y = 0
        right_z = -np.sin(rot_y_rad)

        if glfw.get_key(window, glfw.KEY_W) == glfw.PRESS:
            self.x += front_x * speed
            self.y += front_y * speed
            self.z += front_z * speed
        if glfw.get_key(window, glfw.KEY_S) == glfw.PRESS:
            self.x -= front_x * speed
            self.y -= front_y * speed
            self.z -= front_z * speed
        if glfw.get_key(window, glfw.KEY_A) == glfw.PRESS:
            self.x -= right_x * speed
            self.z -= right_z * speed
        if glfw.get_key(window, glfw.KEY_D) == glfw.PRESS:
            self.x += right_x * speed
            self.z += right_z * speed
        if glfw.get_key(window, glfw.KEY_E) == glfw.PRESS:
            self.y += speed
        if glfw.get_key(window, glfw.KEY_Q) == glfw.PRESS:
            self.y -= speed

    def on_mouse_button(self, window, button, action, mods):
        if button == glfw.MOUSE_BUTTON_LEFT:
            if action == glfw.PRESS:
                self.is_panning = True
                self.last_mouse_x, self.last_mouse_y = glfw.get_cursor_pos(window)
            else:
                self.is_panning = False

    def on_cursor_pos(self, window, xpos, ypos):
        if self.is_panning:
            dx = xpos - self.last_mouse_x
            dy = ypos - self.last_mouse_y

            self.rot_y -= dx * self.mouse_sensitivity
            self.rot_x -= dy * self.mouse_sensitivity

            self.rot_x = max(-89, min(89, self.rot_x))

            self.last_mouse_x = xpos
            self.last_mouse_y = ypos

    def on_scroll(self, window, xoffset, yoffset):
        self.speed += yoffset * 0.5
        self.speed = max(1.0, min(20.0, self.speed))

    def create_view_matrix(self):
        rot_x = np.radians(self.rot_x)
        rot_y = np.radians(self.rot_y)

        cx, cy = np.cos(rot_x), np.cos(rot_y)
        sx, sy = np.sin(rot_x), np.sin(rot_y)

        rot_y_mat = np.array([
            [cy, 0, -sy, 0],
            [0, 1, 0, 0],
            [sy, 0, cy, 0],
            [0, 0, 0, 1]
        ], dtype='f4')

        rot_x_mat = np.array([
            [1, 0, 0, 0],
            [0, cx, sx, 0],
            [0, -sx, cx, 0],
            [0, 0, 0, 1]
        ], dtype='f4')

        trans = np.array([
            [1, 0, 0, -self.x],
            [0, 1, 0, -self.y],
            [0, 0, 1, -self.z],
            [0, 0, 0, 1]
        ], dtype='f4')

        return rot_x_mat @ rot_y_mat @ trans

    @staticmethod
    def create_projection_matrix(width, height, fov=45.0, near=0.1, far=500.0):
        aspect = width / height
        f = 1.0 / np.tan(np.radians(fov) / 2)
        proj = np.array([
            [f / aspect, 0, 0, 0],
            [0, f, 0, 0],
            [0, 0, -(far + near) / (far - near), -2.0 * far * near / (far - near)],
            [0, 0, -1, 0]
        ], dtype='f4')
        return proj
