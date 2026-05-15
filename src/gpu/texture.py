from OpenGL.GL import *


class Texture:
    def __init__(self, width, height, components, data=None, dtype=GL_UNSIGNED_BYTE):
        self.texture_id = glGenTextures(1)
        self.width = width
        self.height = height
        self.components = components

        glBindTexture(GL_TEXTURE_2D, self.texture_id)

        fmt_map = {1: GL_RED, 2: GL_RG, 3: GL_RGB, 4: GL_RGBA}
        internal_fmt_map = {1: GL_R8, 2: GL_RG8, 3: GL_RGB8, 4: GL_RGBA8}

        if dtype == GL_FLOAT:
            internal_fmt_map = {1: GL_R32F, 2: GL_RG32F, 3: GL_RGB32F, 4: GL_RGBA32F}

        internal_fmt = internal_fmt_map.get(components, GL_RGBA8)
        fmt = fmt_map.get(components, GL_RGBA)

        glTexImage2D(GL_TEXTURE_2D, 0, internal_fmt, width, height, 0, fmt, dtype, data)

    def bind(self, unit=0):
        glActiveTexture(GL_TEXTURE0 + unit)
        glBindTexture(GL_TEXTURE_2D, self.texture_id)

    def set_filter(self, min_filter, mag_filter):
        glBindTexture(GL_TEXTURE_2D, self.texture_id)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter)

    def set_repeat(self, repeat_x, repeat_y):
        glBindTexture(GL_TEXTURE_2D, self.texture_id)
        wrap_s = GL_REPEAT if repeat_x else GL_CLAMP_TO_EDGE
        wrap_t = GL_REPEAT if repeat_y else GL_CLAMP_TO_EDGE
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap_s)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap_t)

    def set_mirror_repeat(self, mirror_x, mirror_y):
        glBindTexture(GL_TEXTURE_2D, self.texture_id)
        wrap_s = GL_MIRRORED_REPEAT if mirror_x else GL_CLAMP_TO_EDGE
        wrap_t = GL_MIRRORED_REPEAT if mirror_y else GL_CLAMP_TO_EDGE
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap_s)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap_t)

    def write(self, data, x=0, y=0):
        glBindTexture(GL_TEXTURE_2D, self.texture_id)
        fmt_map = {1: GL_RED, 2: GL_RG, 3: GL_RGB, 4: GL_RGBA}
        fmt = fmt_map.get(self.components, GL_RGBA)
        glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, self.width, self.height, fmt, GL_FLOAT, data)

    def destroy(self):
        glDeleteTextures(1, [self.texture_id])
