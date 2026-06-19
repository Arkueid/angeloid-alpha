#include "framework/gpu/opengl/GlShader.h"

#include "core/util/Log.h"

#include <cstdio>

namespace Gpu {

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

GLuint GlShader::compileProgram(const std::string& vertexSrc, const std::string& fragmentSrc) {
    GLuint vs = compileShader(GL_VERTEX_SHADER, vertexSrc);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragmentSrc);
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
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

// --- GlShader ---

GlShader::GlShader(const std::string& vertexSrc, const std::string& fragmentSrc)
    : mProgramId(compileProgram(vertexSrc, fragmentSrc)) {}

GlShader::~GlShader() {
    destroy();
}

GlShader::GlShader(GlShader&& other) noexcept : mProgramId(other.mProgramId) {
    other.mProgramId = 0;
}

GlShader& GlShader::operator=(GlShader&& other) noexcept {
    if (this != &other) {
        destroy();
        mProgramId = other.mProgramId;
        other.mProgramId = 0;
    }
    return *this;
}

void GlShader::use() {
    glUseProgram(mProgramId);
}

GLint GlShader::cacheLocation(const std::string& name) const {
    auto it = mUniformCache.find(name);
    if (it != mUniformCache.end()) return it->second;
    GLint loc = glGetUniformLocation(mProgramId, name.c_str());
    mUniformCache[name] = loc;
    return loc;
}

void GlShader::setInt(const std::string& name, int value) {
    GLint loc = cacheLocation(name);
    if (loc != -1) glUniform1i(loc, value);
}

void GlShader::setFloat(const std::string& name, float value) {
    GLint loc = cacheLocation(name);
    if (loc != -1) glUniform1f(loc, value);
}

void GlShader::setVec2(const std::string& name, float x, float y) {
    GLint loc = cacheLocation(name);
    if (loc != -1) glUniform2f(loc, x, y);
}

void GlShader::setVec3(const std::string& name, float x, float y, float z) {
    GLint loc = cacheLocation(name);
    if (loc != -1) glUniform3f(loc, x, y, z);
}

void GlShader::setVec4(const std::string& name, float x, float y, float z, float w) {
    GLint loc = cacheLocation(name);
    if (loc != -1) glUniform4f(loc, x, y, z, w);
}

void GlShader::setMat4(const std::string& name, const float* data) {
    GLint loc = cacheLocation(name);
    if (loc != -1) glUniformMatrix4fv(loc, 1, GL_FALSE, data);
}

void GlShader::destroy() {
    if (mProgramId) {
        glDeleteProgram(mProgramId);
        mProgramId = 0;
    }
}

}  // namespace Gpu
