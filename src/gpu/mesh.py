import ctypes
import numpy as np
from OpenGL.GL import *


class VBOWrapper:
    def __init__(self, vbo_id):
        self.vbo_id = vbo_id

    def write(self, data):
        glBindBuffer(GL_ARRAY_BUFFER, self.vbo_id)
        if isinstance(data, np.ndarray):
            glBufferData(GL_ARRAY_BUFFER, data.nbytes, data, GL_DYNAMIC_DRAW)
        else:
            glBufferData(GL_ARRAY_BUFFER, len(data), data, GL_DYNAMIC_DRAW)


class VAO:
    def __init__(self):
        self.vao_id = glGenVertexArrays(1)
        self.vbos = []
        self.ebo = None
        self.index_count = 0
        self.vertex_count = 0

    def bind(self):
        glBindVertexArray(self.vao_id)

    def unbind(self):
        glBindVertexArray(0)

    def add_vbo(self, data, location, size, dtype=GL_FLOAT, stride=0, offset=0):
        vbo = glGenBuffers(1)
        glBindBuffer(GL_ARRAY_BUFFER, vbo)
        if isinstance(data, np.ndarray):
            glBufferData(GL_ARRAY_BUFFER, data.nbytes, data, GL_STATIC_DRAW)
        else:
            glBufferData(GL_ARRAY_BUFFER, data, GL_STATIC_DRAW)
        glEnableVertexAttribArray(location)
        if dtype == GL_FLOAT:
            glVertexAttribPointer(location, size, dtype, GL_FALSE, stride, ctypes.c_void_p(offset))
        elif dtype == GL_INT:
            glVertexAttribIPointer(location, size, dtype, stride, ctypes.c_void_p(offset))
        self.vbos.append(vbo)
        return vbo

    def set_ebo(self, indices):
        ebo = glGenBuffers(1)
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo)
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.nbytes, indices, GL_STATIC_DRAW)
        self.ebo = ebo
        self.index_count = len(indices)

    def update_vbo(self, vbo_index, data):
        glBindBuffer(GL_ARRAY_BUFFER, self.vbos[vbo_index])
        glBufferData(GL_ARRAY_BUFFER, data.nbytes, data, GL_DYNAMIC_DRAW)

    def render(self, mode=GL_TRIANGLES, count=None, first=0):
        self.bind()
        if self.ebo:
            count = count if count is not None else self.index_count
            glDrawElements(mode, count, GL_UNSIGNED_INT, ctypes.c_void_p(first * 4))
        else:
            count = count if count is not None else self.vertex_count
            glDrawArrays(mode, first, count)
        self.unbind()

    def destroy(self):
        if self.vbos:
            glDeleteBuffers(len(self.vbos), self.vbos)
        if self.ebo:
            glDeleteBuffers(1, [self.ebo])
        glDeleteVertexArrays(1, [self.vao_id])


def create_vao_with_buffers(vertex_buffers, indices):
    vao = VAO()
    vao.bind()
    for location, data, size, dtype in vertex_buffers:
        vbo = glGenBuffers(1)
        glBindBuffer(GL_ARRAY_BUFFER, vbo)
        if isinstance(data, np.ndarray):
            glBufferData(GL_ARRAY_BUFFER, data.nbytes, data, GL_STATIC_DRAW)
        else:
            glBufferData(GL_ARRAY_BUFFER, len(data), data, GL_STATIC_DRAW)
        glEnableVertexAttribArray(location)
        if dtype == GL_INT:
            glVertexAttribIPointer(location, size, dtype, 0, ctypes.c_void_p(0))
        else:
            glVertexAttribPointer(location, size, dtype, GL_FALSE, 0, ctypes.c_void_p(0))
    ebo = glGenBuffers(1)
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo)
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.nbytes, indices, GL_STATIC_DRAW)
    vao.ebo = ebo
    vao.index_count = len(indices)
    return vao
