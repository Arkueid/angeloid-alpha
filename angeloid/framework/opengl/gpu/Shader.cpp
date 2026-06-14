#include "framework/opengl/gpu/Shader.h"

#include "core/util/Log.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

namespace Gpu {

// --- Utility ---

std::string ShaderProgram::readFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        MMD_ERROR("SHADER", "Failed to open shader: %s", path.string().c_str());
        return {};
    }
    std::stringstream buf;
    buf << file.rdbuf();
    return buf.str();
}

static GLuint compileShader(GLenum type, const std::string& src) {
    GLuint shader = glCreateShader(type);
    const char* srcPtr = src.c_str();
    glShaderSource(shader, 1, &srcPtr, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[1024];
        glGetShaderInfoLog(shader, sizeof(infoLog), nullptr, infoLog);
        std::string typeName = (type == GL_VERTEX_SHADER) ? "vertex" : "fragment";
        MMD_ERROR("SHADER", "Shader compile error (%s):\n%s", typeName.c_str(), infoLog);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint ShaderProgram::compileProgram(const std::string& vertexSrc, const std::string& fragmentSrc) {
    GLuint vs = compileShader(GL_VERTEX_SHADER, vertexSrc);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragmentSrc);
    if (!vs || !fs) {
        if (vs)
            glDeleteShader(vs);
        if (fs)
            glDeleteShader(fs);
        return 0;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[1024];
        glGetProgramInfoLog(program, sizeof(infoLog), nullptr, infoLog);
        MMD_ERROR("SHADER", "Shader link error:\n%s", infoLog);
        glDeleteProgram(program);
        program = 0;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

// --- ShaderProgram ---

ShaderProgram::ShaderProgram(const std::string& vertexSrc, const std::string& fragmentSrc)
    : mProgramId(compileProgram(vertexSrc, fragmentSrc)) {
}

ShaderProgram::~ShaderProgram() {
    destroy();
}

ShaderProgram::ShaderProgram(ShaderProgram&& other) noexcept : mProgramId(other.mProgramId) {
    other.mProgramId = 0;
}

ShaderProgram& ShaderProgram::operator=(ShaderProgram&& other) noexcept {
    if (this != &other) {
        destroy();
        mProgramId = other.mProgramId;
        other.mProgramId = 0;
    }
    return *this;
}

void ShaderProgram::use() const {
    glUseProgram(mProgramId);
}

GLint ShaderProgram::cacheLocation(const std::string& name) const {
    auto it = mUniformCache.find(name);
    if (it != mUniformCache.end())
        return it->second;
    GLint loc = glGetUniformLocation(mProgramId, name.c_str());
    mUniformCache[name] = loc;
    return loc;
}

void ShaderProgram::setInt(const std::string& name, int value) const {
    GLint loc = cacheLocation(name);
    if (loc != -1)
        glUniform1i(loc, value);
}

void ShaderProgram::setFloat(const std::string& name, float value) const {
    GLint loc = cacheLocation(name);
    if (loc != -1)
        glUniform1f(loc, value);
}

void ShaderProgram::setVec2(const std::string& name, float x, float y) const {
    GLint loc = cacheLocation(name);
    if (loc != -1)
        glUniform2f(loc, x, y);
}

void ShaderProgram::setVec3(const std::string& name, float x, float y, float z) const {
    GLint loc = cacheLocation(name);
    if (loc != -1)
        glUniform3f(loc, x, y, z);
}

void ShaderProgram::setVec4(const std::string& name, float x, float y, float z, float w) const {
    GLint loc = cacheLocation(name);
    if (loc != -1)
        glUniform4f(loc, x, y, z, w);
}

void ShaderProgram::setMat4(const std::string& name, const float* data) const {
    GLint loc = cacheLocation(name);
    if (loc != -1)
        glUniformMatrix4fv(loc, 1, GL_FALSE, data);
}

void ShaderProgram::destroy() {
    if (mProgramId) {
        glDeleteProgram(mProgramId);
        mProgramId = 0;
    }
}

}  // namespace Gpu
