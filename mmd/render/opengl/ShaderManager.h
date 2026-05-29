#pragma once

#include "render/opengl/gpu/Shader.h"
#include "render/opengl/gpu/Texture.h"

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>

class ShaderManager {
   public:
    explicit ShaderManager(const std::filesystem::path& shaderDir);
    ~ShaderManager() = default;

    Gpu::ShaderProgram* get(const std::string& name);
    Gpu::Texture* gradientTexture()
    {
        return mGradientTexture.get();
    }

    void setOutlineThickness(float thickness);

   private:
    void createMainShader();
    void createAxisShader();
    void createRigidbodyShader();
    void createOutlineShader();
    void createToonShader();
    void createSkinnedShader();
    void createMorphShader();
    void createGradientTexture();

    Gpu::ShaderProgram& addProgram(const std::string& name, const std::string& vertFile,
                                   const std::string& fragFile);

    std::string mShaderDir;
    std::unordered_map<std::string, std::unique_ptr<Gpu::ShaderProgram>> mPrograms;
    std::unique_ptr<Gpu::Texture> mGradientTexture;
};
