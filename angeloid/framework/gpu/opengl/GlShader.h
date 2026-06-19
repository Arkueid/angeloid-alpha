#pragma once

#include "framework/gpu/IGpuShader.h"

#include <glad/glad.h>
#include <string>
#include <unordered_map>

namespace Gpu {

class GlShader : public IGpuShader {
public:
    GlShader(const std::string& vertexSrc, const std::string& fragmentSrc);
    ~GlShader() override;

    GlShader(const GlShader&) = delete;
    GlShader& operator=(const GlShader&) = delete;
    GlShader(GlShader&&) noexcept;
    GlShader& operator=(GlShader&&) noexcept;

    GLuint id() const { return mProgramId; }

    void use() override;
    void setInt(const std::string& name, int value) override;
    void setFloat(const std::string& name, float value) override;
    void setVec2(const std::string& name, float x, float y) override;
    void setVec3(const std::string& name, float x, float y, float z) override;
    void setVec4(const std::string& name, float x, float y, float z, float w) override;
    void setMat4(const std::string& name, const float* data) override;

    void destroy();

    static GLuint compileProgram(const std::string& vertexSrc, const std::string& fragmentSrc);

private:
    GLint cacheLocation(const std::string& name) const;

    GLuint mProgramId = 0;
    mutable std::unordered_map<std::string, GLint> mUniformCache;
};

}  // namespace Gpu
