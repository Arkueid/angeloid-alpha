#include "render/PhysicsDebug.h"
#include "Gpu/Shader.h"

#include <algorithm>
#include <array>
#include <cmath>

static void addBox(std::vector<float>& verts, const Vec3& halfSize, const Vec3& color)
{
    float sx = halfSize.x, sy = halfSize.y, sz = halfSize.z;
    float v[][3] = {
        {-sx,-sy,-sz},{ sx,-sy,-sz},{ sx, sy,-sz},{-sx, sy,-sz},
        {-sx,-sy, sz},{ sx,-sy, sz},{ sx, sy, sz},{-sx, sy, sz},
    };
    int edges[][2] = {
        {0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7},
    };
    for (auto& e : edges) {
        verts.insert(verts.end(), {v[e[0]][0],v[e[0]][1],v[e[0]][2], color.x,color.y,color.z});
        verts.insert(verts.end(), {v[e[1]][0],v[e[1]][1],v[e[1]][2], color.x,color.y,color.z});
    }
}

static void addSphere(std::vector<float>& verts, float r, const Vec3& color)
{
    for (int a = 0; a < 3; ++a) {
        for (int i = 0; i < 8; ++i) {
            float t1 = (float)i * 3.14159f / 4.0f;
            float t2 = (float)(i + 1) * 3.14159f / 4.0f;
            if (a == 0) { // XY plane
                verts.insert(verts.end(), {r*cosf(t1),r*sinf(t1),0, color.x,color.y,color.z});
                verts.insert(verts.end(), {r*cosf(t2),r*sinf(t2),0, color.x,color.y,color.z});
            } else if (a == 1) { // XZ plane
                verts.insert(verts.end(), {r*cosf(t1),0,r*sinf(t1), color.x,color.y,color.z});
                verts.insert(verts.end(), {r*cosf(t2),0,r*sinf(t2), color.x,color.y,color.z});
            } else { // YZ plane
                verts.insert(verts.end(), {0,r*cosf(t1),r*sinf(t1), color.x,color.y,color.z});
                verts.insert(verts.end(), {0,r*cosf(t2),r*sinf(t2), color.x,color.y,color.z});
            }
        }
    }
}

static void addCapsule(std::vector<float>& verts, float r, float h, const Vec3& color)
{
    float hc = h * 0.5f;
    for (int i = 0; i < 8; ++i) {
        float a1 = i * 3.14159f * 2 / 8, a2 = (i+1) * 3.14159f * 2 / 8;
        // Top cap circle
        verts.insert(verts.end(), {r*cosf(a1), hc, r*sinf(a1), color.x,color.y,color.z});
        verts.insert(verts.end(), {r*cosf(a2), hc, r*sinf(a2), color.x,color.y,color.z});
        // Bottom cap circle
        verts.insert(verts.end(), {r*cosf(a1),-hc, r*sinf(a1), color.x,color.y,color.z});
        verts.insert(verts.end(), {r*cosf(a2),-hc, r*sinf(a2), color.x,color.y,color.z});
        // Vertical lines
        verts.insert(verts.end(), {r*cosf(a1), hc, r*sinf(a1), color.x,color.y,color.z});
        verts.insert(verts.end(), {r*cosf(a1),-hc, r*sinf(a1), color.x,color.y,color.z});
    }
}

static void buildPass(const PmxModel& model, float cx, float my, float cz, float ms,
                       bool forBone, Gpu::Vao& rbVao, Gpu::Vao& jtVao)
{
    Vec3 colors[] = {{1,1,0},{1,0.5f,0},{0,1,1},{1,0,1}};

    // --- Rigid bodies ---
    std::vector<float> rbVerts;
    std::vector<int32_t> rbBoneIdx;

    for (size_t i = 0; i < model.rigidbodies.size(); ++i) {
        const auto& rb = model.rigidbodies[i];
        Vec3 c = colors[i % 4];
        Vec3 sz = {rb.shape_size.x * 0.5f, rb.shape_size.y * 0.5f, rb.shape_size.z * 0.5f};

        std::vector<float> v;
        if (rb.shape_type == RIGID_SHAPE_SPHERE)       { addSphere(v, sz.x, c); }
        else if (rb.shape_type == RIGID_SHAPE_BOX)     { addBox(v, sz, c); }
        else if (rb.shape_type == RIGID_SHAPE_CAPSULE) { addCapsule(v, sz.x, sz.y * 2, c); }
        else { addSphere(v, sz.x, c); }

        float rx = rb.shape_rotation.x, ry = rb.shape_rotation.y, rz = rb.shape_rotation.z;
        float crx = cosf(rx), srx = sinf(rx), cry = cosf(ry), sry = sinf(ry), crz = cosf(rz), srz = sinf(rz);
        float R[9] = {
            cry*crz, crz*srx*sry - crx*srz, crx*crz*sry + srx*srz,
            cry*srz, crx*crz + srx*sry*srz, -crz*srx + crx*sry*srz,
            -sry,    cry*srx,              crx*cry
        };

        for (size_t j = 0; j < v.size(); j += 6) {
            float lx = v[j], ly = v[j+1], lz = v[j+2];
            float wx = R[0]*lx + R[1]*ly + R[2]*lz + rb.shape_position.x;
            float wy = R[3]*lx + R[4]*ly + R[5]*lz + rb.shape_position.y;
            float wz = R[6]*lx + R[7]*ly + R[8]*lz + rb.shape_position.z;
            wx = (wx - cx) * ms; wy = (wy - my) * ms; wz = (wz - cz) * ms;
            v[j] = wx; v[j+1] = wy; v[j+2] = wz;
        }
        rbVerts.insert(rbVerts.end(), v.begin(), v.end());
        int vc = (int)v.size() / 6;
        for (int j = 0; j < vc; ++j) rbBoneIdx.push_back(forBone ? rb.bone_index : -1);
    }

    if (!rbVerts.empty()) {
        rbVao.bind();
        GLuint vbo; glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, rbVerts.size() * sizeof(float), rbVerts.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void*)(3*sizeof(float)));
        rbVao.vbos.push_back(vbo);
        GLuint bvbo; glGenBuffers(1, &bvbo);
        glBindBuffer(GL_ARRAY_BUFFER, bvbo);
        glBufferData(GL_ARRAY_BUFFER, rbBoneIdx.size()*sizeof(int32_t), rbBoneIdx.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(2);
        glVertexAttribIPointer(2, 1, GL_INT, 0, nullptr);
        rbVao.vbos.push_back(bvbo);
        rbVao.vertexCount = (int)rbVerts.size() / 6;
        Gpu::Vao::unbind();
    }

    // --- Joints ---
    std::vector<float> jtVerts;
    std::vector<int32_t> jtBoneIdx;

    for (size_t i = 0; i < model.joints.size(); ++i) {
        const auto& jt = model.joints[i];
        int boneIdx = jt.rigidbody_index_a >= 0 && jt.rigidbody_index_a < (int)model.rigidbodies.size()
            ? model.rigidbodies[jt.rigidbody_index_a].bone_index : -1;
        float x = jt.position.x, y = jt.position.y, z = jt.position.z;
        x = (x - cx) * ms; y = (y - my) * ms; z = (z - cz) * ms;
        Vec3 col = {0,1,0};
        float s = 0.05f;
        size_t base = jtVerts.size() / 6;
        jtVerts.insert(jtVerts.end(), {x-s,y,z, col.x,col.y,col.z, x+s,y,z, col.x,col.y,col.z});
        jtVerts.insert(jtVerts.end(), {x,y-s,z, col.x,col.y,col.z, x,y+s,z, col.x,col.y,col.z});
        jtVerts.insert(jtVerts.end(), {x,y,z-s, col.x,col.y,col.z, x,y,z+s, col.x,col.y,col.z});
        int vc = (int)(jtVerts.size()/6) - (int)base;
        for (int j = 0; j < vc; ++j) jtBoneIdx.push_back(forBone ? boneIdx : -1);
    }

    if (!jtVerts.empty()) {
        jtVao.bind();
        GLuint vbo; glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, jtVerts.size()*sizeof(float), jtVerts.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void*)(3*sizeof(float)));
        jtVao.vbos.push_back(vbo);
        GLuint bvbo; glGenBuffers(1, &bvbo);
        glBindBuffer(GL_ARRAY_BUFFER, bvbo);
        glBufferData(GL_ARRAY_BUFFER, jtBoneIdx.size()*sizeof(int32_t), jtBoneIdx.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(2);
        glVertexAttribIPointer(2, 1, GL_INT, 0, nullptr);
        jtVao.vbos.push_back(bvbo);
        jtVao.vertexCount = (int)jtVerts.size() / 6;
        Gpu::Vao::unbind();
    }
}

void PhysicsDebug::build(const PmxModel& model, float modelScale)
{
    Vec3 minPos = {1e9f,1e9f,1e9f}, maxPos = {-1e9f,-1e9f,-1e9f};
    for (const auto& v : model.vertices) {
        minPos.x = std::min(minPos.x, v.position.x);
        minPos.y = std::min(minPos.y, v.position.y);
        minPos.z = std::min(minPos.z, v.position.z);
        maxPos.x = std::max(maxPos.x, v.position.x);
        maxPos.y = std::max(maxPos.y, v.position.y);
        maxPos.z = std::max(maxPos.z, v.position.z);
    }
    float cx = (minPos.x + maxPos.x) * 0.5f;
    float my = minPos.y;
    float cz = (minPos.z + maxPos.z) * 0.5f;
    float ms = modelScale;

    buildPass(model, cx, my, cz, ms, false, mRbStatic, mJtStatic);
    buildPass(model, cx, my, cz, ms, true,  mRbAnimated, mJtAnimated);
}

void PhysicsDebug::render(Gpu::ShaderProgram& shader,
                           const std::array<float, 16>& projection,
                           const std::array<float, 16>& view,
                           const float* modelMatParam) const
{
    float defMat[] = {1,0,0,0, 0,1,0,0, 0,0,-1,0, 0,0,0,1};
    const float* mm = modelMatParam ? modelMatParam : defMat;

    shader.use();
    shader.setMat4("projection", projection.data());
    shader.setMat4("view", view.data());
    shader.setMat4("model", mm);

    const Gpu::Vao& rb = useBoneMatrices ? mRbAnimated : mRbStatic;
    const Gpu::Vao& jt = useBoneMatrices ? mJtAnimated : mJtStatic;

    if (showRigidBody && rb.vertexCount > 0) rb.render(GL_LINES);
    if (showJoint && jt.vertexCount > 0) jt.render(GL_LINES);
}
