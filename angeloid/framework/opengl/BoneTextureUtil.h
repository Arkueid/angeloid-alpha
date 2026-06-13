#pragma once

#include "core/anim/BoneSkinning.h"
#include "framework/opengl/gpu/Texture.h"

#include <GL/glew.h>
#include <memory>

// Creates a GPU texture from CPU-side bone matrix data.
// Extracted from BoneSkinning to keep the animation layer GPU-free.
inline std::unique_ptr<Gpu::Texture> createBoneTexture(const BoneTextureData& data) {
    auto tex =
        std::make_unique<Gpu::Texture>(data.width, data.height, 4, data.pixels.data(), GL_FLOAT);
    tex->setFilter(GL_NEAREST, GL_NEAREST);
    tex->setWrap(false, false);
    return tex;
}
