#pragma once

#include <glad/glad.h>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace Gpu {

class ShaderProgram {
public:
    ShaderProgram() = default;
    ShaderProgram(const std::string& vertexSrc, const std::string& fragmentSrc);
    ~ShaderProgram();
    ShaderProgram(const ShaderProgram&) = delete;
    ShaderProgram& operator=(const ShaderProgram&) = delete;
    ShaderProgram(ShaderProgram&&) noexcept;
    ShaderProgram& operator=(ShaderProgram&&) noexcept;

    bool isValid() const {
        return mProgramId != 0;
    }
    GLuint id() const {
        return mProgramId;
    }

    void use() const;

    void setInt(const std::string& name, int value) const;
    void setFloat(const std::string& name, float value) const;
    void setVec2(const std::string& name, float x, float y) const;
    void setVec3(const std::string& name, float x, float y, float z) const;
    void setVec4(const std::string& name, float x, float y, float z, float w) const;
    void setMat4(const std::string& name, const float* data) const;

    void destroy();

    static std::string readFile(const std::filesystem::path& path);
    static GLuint compileProgram(const std::string& vertexSrc, const std::string& fragmentSrc);

private:
    GLint cacheLocation(const std::string& name) const;
    GLuint mProgramId = 0;
    mutable std::unordered_map<std::string, GLint> mUniformCache;
};

}  // namespace Gpu
