#include "render/opengl/ShaderManager.h"

#include <GL/glew.h>
#include <array>
#include <filesystem>

ShaderManager::ShaderManager(const std::filesystem::path& shaderDir)
    : mShaderDir(shaderDir.string()) {
    createMainShader();
    createAxisShader();
    createRigidbodyShader();
    createOutlineShader();
    createToonShader();
    createSkinnedShader();
    createMorphShader();
    createGradientTexture();
}

Gpu::ShaderProgram* ShaderManager::get(const std::string& name) {
    auto it = mPrograms.find(name);
    return it != mPrograms.end() ? it->second.get() : nullptr;
}

Gpu::ShaderProgram& ShaderManager::addProgram(const std::string& name, const std::string& vertFile,
                                              const std::string& fragFile) {
    auto vertSrc = Gpu::ShaderProgram::readFile(mShaderDir + "/" + vertFile);
    auto fragSrc = Gpu::ShaderProgram::readFile(mShaderDir + "/" + fragFile);
    auto prog = std::make_unique<Gpu::ShaderProgram>(vertSrc, fragSrc);
    auto& ref = *prog;
    mPrograms[name] = std::move(prog);
    return ref;
}

void ShaderManager::createMainShader() {
    auto& p = addProgram("main", "main.vert", "main.frag");
    p.use();
    p.setVec3("light_dir", 0.0f, 0.5f, -1.0f);
    p.setInt("has_texture", 0);
    p.setInt("tex", 0);
    p.setFloat("alpha", 1.0f);
    p.setVec3("material_color", 1.0f, 1.0f, 1.0f);
}

void ShaderManager::createAxisShader() {
    addProgram("axis", "axis.vert", "axis.frag");
}

void ShaderManager::createRigidbodyShader() {
    addProgram("rigidbody", "rigidbody.vert", "rigidbody.frag");
}

void ShaderManager::createOutlineShader() {
    auto& p = addProgram("outline", "outline.vert", "outline.frag");
    p.use();
    p.setVec4("outline_color", 0.0f, 0.0f, 0.0f, 1.0f);
    p.setFloat("outline_thickness", 0.001f);
    p.setInt("tex", 0);
    p.setFloat("alpha", 1.0f);

    auto& ps = addProgram("outline_skinned", "outline_skinned.vert", "outline.frag");
    ps.use();
    ps.setVec4("outline_color", 0.0f, 0.0f, 0.0f, 1.0f);
    ps.setFloat("outline_thickness", 0.001f);
    ps.setInt("tex", 0);
    ps.setInt("bone_texture", 1);
    ps.setInt("bone_texture_width", 64);
    ps.setFloat("alpha", 1.0f);
}

void ShaderManager::createToonShader() {
    auto& p = addProgram("toon", "toon.vert", "toon.frag");
    p.use();
    p.setVec3("light_dir", 0.0f, 0.5f, -1.0f);
    p.setFloat("shadow_thresh", 0.2f);
    p.setFloat("rim_power", 3.0f);
    p.setVec3("rim_color", 1.0f, 0.95f, 0.9f);
    p.setInt("has_texture", 0);
    p.setInt("tex", 0);
    p.setInt("gradient_map", 1);
    p.setFloat("alpha", 1.0f);
    p.setVec3("material_color", 1.0f, 1.0f, 1.0f);
    p.setInt("toon_tex", 4);
    p.setInt("has_toon", 0);
    p.setInt("sphere_tex", 3);
    p.setInt("sphere_mode", 0);
}

void ShaderManager::createSkinnedShader() {
    auto& p = addProgram("skinned", "skinned.vert", "toon.frag");
    p.use();
    p.setVec3("light_dir", 0.0f, 0.5f, -1.0f);
    p.setInt("has_texture", 0);
    p.setInt("tex", 0);
    p.setInt("bone_texture", 1);
    p.setInt("gradient_map", 2);
    p.setFloat("shadow_thresh", 0.2f);
    p.setFloat("rim_power", 3.0f);
    p.setVec3("rim_color", 1.0f, 0.95f, 0.9f);
    p.setFloat("alpha", 1.0f);
    p.setVec3("material_color", 1.0f, 1.0f, 1.0f);

    auto& pn = addProgram("skinned_notoon", "skinned.vert", "main.frag");
    pn.use();
    pn.setVec3("light_dir", 0.0f, 0.5f, -1.0f);
    pn.setInt("has_texture", 0);
    pn.setInt("tex", 0);
    pn.setInt("bone_texture", 1);
    pn.setFloat("alpha", 1.0f);
    pn.setVec3("material_color", 1.0f, 1.0f, 1.0f);
}

void ShaderManager::createMorphShader() {
    auto& p = addProgram("morph", "skinned_morph.vert", "toon.frag");
    p.use();
    p.setVec3("light_dir", 0.0f, 0.5f, -1.0f);
    p.setInt("has_texture", 0);
    p.setInt("tex", 0);
    p.setInt("gradient_map", 2);
    p.setFloat("shadow_thresh", 0.2f);
    p.setFloat("rim_power", 3.0f);
    p.setVec3("rim_color", 1.0f, 0.95f, 0.9f);
    p.setFloat("morph_weight", 0.0f);
    p.setInt("bone_texture_width", 64);
    p.setInt("bone_texture", 1);
    p.setFloat("alpha", 1.0f);
    p.setVec3("material_color", 1.0f, 1.0f, 1.0f);

    auto& pn = addProgram("morph_notoon", "skinned_morph.vert", "main.frag");
    pn.use();
    pn.setVec3("light_dir", 0.0f, 0.5f, -1.0f);
    pn.setInt("has_texture", 0);
    pn.setInt("tex", 0);
    pn.setFloat("morph_weight", 0.0f);
    pn.setInt("bone_texture_width", 64);
    pn.setInt("bone_texture", 1);
    pn.setFloat("alpha", 1.0f);
    pn.setVec3("material_color", 1.0f, 1.0f, 1.0f);

    auto& po = addProgram("morph_outline", "outline_skinned_morph.vert", "outline.frag");
    po.use();
    po.setVec4("outline_color", 0.0f, 0.0f, 0.0f, 1.0f);
    po.setFloat("outline_thickness", 0.001f);
    po.setInt("tex", 0);
    po.setFloat("morph_weight", 0.0f);
    po.setInt("bone_texture_width", 64);
    po.setInt("bone_texture", 1);
    po.setFloat("alpha", 1.0f);
}

void ShaderManager::createGradientTexture() {
    // 4-level gray gradient used by the toon fragment shader for cel-shading ramp.
    // The shader computes N·L dot product and uses it as a UV coordinate into this
    // 1D gradient texture, producing the discrete shadow bands characteristic of toon rendering.
    uint8_t gradient[] = {
        60, 60, 60, 120, 120, 120, 180, 180, 180, 220, 220, 220,
    };
    mGradientTexture = std::make_unique<Gpu::Texture>(4, 1, 3, gradient);
    mGradientTexture->setFilter(GL_LINEAR, GL_LINEAR);
    mGradientTexture->setWrap(false, false);
}

void ShaderManager::setOutlineThickness(float thickness) {
    auto* outline = get("outline");
    if (outline) {
        outline->use();
        outline->setFloat("outline_thickness", thickness);
    }
    auto* outlineSk = get("outline_skinned");
    if (outlineSk) {
        outlineSk->use();
        outlineSk->setFloat("outline_thickness", thickness);
    }
    auto* morphOl = get("morph_outline");
    if (morphOl) {
        morphOl->use();
        morphOl->setFloat("outline_thickness", thickness);
    }
}
