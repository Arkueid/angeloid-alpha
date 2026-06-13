#include "framework/opengl/scene/GroundPlane.h"

#include "framework/opengl/ShaderManager.h"
#include "framework/opengl/ShaderStandard.h"

GroundPlane::GroundPlane() {
    // 200×200 quad at Y=0
    float groundSize = 100.0f;
    float verts[] = {
        -groundSize, 0, -groundSize,
         groundSize, 0, -groundSize,
         groundSize, 0,  groundSize,
        -groundSize, 0, -groundSize,
         groundSize, 0,  groundSize,
        -groundSize, 0,  groundSize,
    };

    glGenVertexArrays(1, &mVao);
    glBindVertexArray(mVao);
    glGenBuffers(1, &mVbo);
    glBindBuffer(GL_ARRAY_BUFFER, mVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glBindVertexArray(0);
}

void GroundPlane::onMainPass(const std::array<float, 16>& proj,
                             const std::array<float, 16>& view,
                             const std::array<float, 16>& /*model*/,
                             const std::array<float, 16>& lightViewProj,
                             bool hasShadow) {
    auto* shader = ShaderManager::instance().ground();
    if (!shader) return;

    shader->use();
    shader->setMat4(U_PROJ_MAT, proj.data());
    shader->setMat4(U_VIEW_MAT, view.data());
    if (hasShadow) {
        shader->setInt("u_shadowMap", 5);
        shader->setMat4("u_lightViewProj", lightViewProj.data());
        shader->setInt("u_hasShadow", 1);
    } else {
        shader->setInt("u_hasShadow", 0);
    }
    glBindVertexArray(mVao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

GroundPlane::~GroundPlane() {
    if (mVao) glDeleteVertexArrays(1, &mVao);
    if (mVbo) glDeleteBuffers(1, &mVbo);
}
