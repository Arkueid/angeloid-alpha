#include "framework/opengl/debug/RigidBodyRenderer.h"

#include "core/anim/PhysicsWorld.h"
#include "framework/opengl/ShaderStandard.h"
#include "framework/opengl/gpu/Shader.h"

#include <algorithm>
#include <btBulletDynamicsCommon.h>
#include <cmath>

// Procedural shape generators for debug visualization of physics bodies.
// Each shape is tessellated into triangles with per-vertex normals:
//   Sphere:   smooth normals (radial from center)
//   Capsule:  smooth normals on hemispheres, smooth on cylinder
//   Box:      flat face normals
// Vertex layout: pos(3) + color(3) + normal(3) = 9 floats per vertex

static void emitTri(std::vector<float>& v, float x1, float y1, float z1, float x2, float y2,
                    float z2, float x3, float y3, float z3, float nx, float ny, float nz,
                    const Vec3& c) {
    v.insert(v.end(), {x1,  y1, z1, c.x, c.y, c.z, nx, ny,  nz,  x2,  y2, z2, c.x, c.y,
                       c.z, nx, ny, nz,  x3,  y3,  z3, c.x, c.y, c.z, nx, ny, nz});
}

static void addBox(std::vector<float>& verts, const Vec3& hs, const Vec3& c) {
    float v[8][3] = {
        {-hs.x, -hs.y, -hs.z}, {hs.x, -hs.y, -hs.z}, {hs.x, hs.y, -hs.z}, {-hs.x, hs.y, -hs.z},
        {-hs.x, -hs.y, hs.z},  {hs.x, -hs.y, hs.z},  {hs.x, hs.y, hs.z},  {-hs.x, hs.y, hs.z},
    };
    int f[6][4] = {{0, 1, 2, 3}, {5, 4, 7, 6}, {4, 0, 3, 7},
                   {1, 5, 6, 2}, {3, 2, 6, 7}, {4, 5, 1, 0}};
    float n[6][3] = {{0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, -1, 0}};
    for (int fi = 0; fi < 6; ++fi) {
        float nx = n[fi][0], ny = n[fi][1], nz = n[fi][2];
        auto& fi2 = f[fi];
        emitTri(verts, v[fi2[0]][0], v[fi2[0]][1], v[fi2[0]][2], v[fi2[1]][0], v[fi2[1]][1],
                v[fi2[1]][2], v[fi2[2]][0], v[fi2[2]][1], v[fi2[2]][2], nx, ny, nz, c);
        emitTri(verts, v[fi2[0]][0], v[fi2[0]][1], v[fi2[0]][2], v[fi2[2]][0], v[fi2[2]][1],
                v[fi2[2]][2], v[fi2[3]][0], v[fi2[3]][1], v[fi2[3]][2], nx, ny, nz, c);
    }
}

// Emit one vertex with normal = normalized position from center
static void emitV(std::vector<float>& v, float x, float y, float z, const Vec3& c) {
    float len = sqrtf(x * x + y * y + z * z);
    float nx = len > 0 ? x / len : 0, ny = len > 0 ? y / len : 0, nz = len > 0 ? z / len : 1;
    v.insert(v.end(), {x, y, z, c.x, c.y, c.z, nx, ny, nz});
}

static void addSphere(std::vector<float>& verts, float r, const Vec3& c) {
    int slices = 12, stacks = 6;
    float pts[8][13][3];
    for (int s = 0; s <= stacks; ++s) {
        float phi = s * 3.14159265f / stacks;
        float sr = r * sinf(phi), yy = r * cosf(phi);
        for (int i = 0; i <= slices; ++i) {
            float a = i * 6.2831853f / slices;
            pts[s][i][0] = sr * cosf(a);
            pts[s][i][1] = yy;
            pts[s][i][2] = sr * sinf(a);
        }
    }
    for (int s = 0; s < stacks; ++s) {
        for (int i = 0; i < slices; ++i) {
            float *a = pts[s][i], *b = pts[s][i + 1], *p = pts[s + 1][i + 1], *d = pts[s + 1][i];
            emitV(verts, a[0], a[1], a[2], c);
            emitV(verts, b[0], b[1], b[2], c);
            emitV(verts, p[0], p[1], p[2], c);
            emitV(verts, a[0], a[1], a[2], c);
            emitV(verts, p[0], p[1], p[2], c);
            emitV(verts, d[0], d[1], d[2], c);
        }
    }
}

static void addCapsule(std::vector<float>& verts, float r, float h, const Vec3& c) {
    int slices = 12, hStacks = 3;
    float hh = h * 0.5f;
    float topPts[4][13][3], botPts[4][13][3];

    // Top hemisphere (stack 0 = north pole, stack 3 = equator)
    topPts[0][0][0] = 0;
    topPts[0][0][1] = hh + r;
    topPts[0][0][2] = 0;
    for (int s = 1; s <= hStacks; ++s) {
        float phi = s * 1.57079633f / hStacks;
        float sr = r * sinf(phi), yy = hh + r * cosf(phi);
        for (int i = 0; i <= slices; ++i) {
            float a = i * 6.2831853f / slices;
            topPts[s][i][0] = sr * cosf(a);
            topPts[s][i][1] = yy;
            topPts[s][i][2] = sr * sinf(a);
        }
    }
    // Top pole fan
    for (int i = 0; i < slices; ++i) {
        float *a = topPts[0][0], *b = topPts[1][i + 1], *p = topPts[1][i];
        emitV(verts, a[0], a[1], a[2], c);
        emitV(verts, b[0], b[1], b[2], c);
        emitV(verts, p[0], p[1], p[2], c);
    }
    // Top middle bands
    for (int s = 1; s < hStacks; ++s) {
        for (int i = 0; i < slices; ++i) {
            float *a = topPts[s][i], *b = topPts[s + 1][i], *p = topPts[s + 1][i + 1],
                  *d = topPts[s][i + 1];
            emitV(verts, a[0], a[1], a[2], c);
            emitV(verts, b[0], b[1], b[2], c);
            emitV(verts, p[0], p[1], p[2], c);
            emitV(verts, a[0], a[1], a[2], c);
            emitV(verts, p[0], p[1], p[2], c);
            emitV(verts, d[0], d[1], d[2], c);
        }
    }

    // Bottom hemisphere (stack 0 = south pole, stack 3 = equator)
    botPts[0][0][0] = 0;
    botPts[0][0][1] = -hh - r;
    botPts[0][0][2] = 0;
    for (int s = 1; s <= hStacks; ++s) {
        float phi = s * 1.57079633f / hStacks;
        float sr = r * sinf(phi), yy = -hh - r * cosf(phi);
        for (int i = 0; i <= slices; ++i) {
            float a = i * 6.2831853f / slices;
            botPts[s][i][0] = sr * cosf(a);
            botPts[s][i][1] = yy;
            botPts[s][i][2] = sr * sinf(a);
        }
    }
    // South pole fan
    for (int i = 0; i < slices; ++i) {
        float *a = botPts[0][0], *b = botPts[1][i], *p = botPts[1][i + 1];
        emitV(verts, a[0], a[1], a[2], c);
        emitV(verts, b[0], b[1], b[2], c);
        emitV(verts, p[0], p[1], p[2], c);
    }
    // Bottom middle bands
    for (int s = 1; s < hStacks; ++s) {
        for (int i = 0; i < slices; ++i) {
            float *a = botPts[s][i], *b = botPts[s + 1][i], *p = botPts[s + 1][i + 1],
                  *d = botPts[s][i + 1];
            emitV(verts, a[0], a[1], a[2], c);
            emitV(verts, b[0], b[1], b[2], c);
            emitV(verts, p[0], p[1], p[2], c);
            emitV(verts, a[0], a[1], a[2], c);
            emitV(verts, p[0], p[1], p[2], c);
            emitV(verts, d[0], d[1], d[2], c);
        }
    }

    // Cylinder body: connect top equator to bottom equator
    for (int i = 0; i < slices; ++i) {
        int j = (i + 1) % slices;
        float *t1 = topPts[hStacks][i], *t2 = topPts[hStacks][j];
        float *b1 = botPts[hStacks][i], *b2 = botPts[hStacks][j];
        emitV(verts, t1[0], t1[1], t1[2], c);
        emitV(verts, t2[0], t2[1], t2[2], c);
        emitV(verts, b2[0], b2[1], b2[2], c);
        emitV(verts, t1[0], t1[1], t1[2], c);
        emitV(verts, b2[0], b2[1], b2[2], c);
        emitV(verts, b1[0], b1[1], b1[2], c);
    }
}

// --- VAO setup ---
// Float VBO: pos(3)+color(3)+normal(3)=9 floats stride. Separate int VBO for bone_index

static void setupVao(Gpu::Vao& vao, const std::vector<float>& verts,
                     const std::vector<int32_t>& boneIdx) {
    if (verts.empty())
        return;
    vao.bind();
    size_t stride = 9 * sizeof(float);
    GLuint vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
    vao.vbos.push_back(vbo);
    GLuint bvbo;
    glGenBuffers(1, &bvbo);
    glBindBuffer(GL_ARRAY_BUFFER, bvbo);
    glBufferData(GL_ARRAY_BUFFER, boneIdx.size() * sizeof(int32_t), boneIdx.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(2);
    glVertexAttribIPointer(2, 1, GL_INT, 0, nullptr);
    vao.vbos.push_back(bvbo);
    vao.vertexCount = (int)verts.size() / 9;
    Gpu::Vao::unbind();
}

static void buildPass(const PmxModel& model, float cx, float my, float cz, float ms, bool forBone,
                      Gpu::Vao& rbVao, Gpu::Vao& jtVao) {
    Vec3 colors[] = {{1, 1, 0}, {1, 0.5f, 0}, {0, 1, 1}, {1, 0, 1}};

    // --- Rigid bodies ---
    std::vector<float> rbVerts;
    std::vector<int32_t> rbBoneIdx;

    for (size_t i = 0; i < model.rigidbodies.size(); ++i) {
        const auto& rb = model.rigidbodies[i];
        // Color by mode: green=static(0), orange=dynamic(1), blue=bone-align(2)
        Vec3 c;
        switch (rb.mode) {
        case 0: c = {0.0f, 1.0f, 0.0f}; break;   // bright green: kinematic
        case 2: c = {0.2f, 0.6f, 1.0f}; break;   // bright blue: bone-align
        default: c = {1.0f, 0.5f, 0.0f}; break;  // bright orange: dynamic
        }
        float kx = rb.shape_size.x, ky = rb.shape_size.y, kz = rb.shape_size.z;

        std::vector<float> v;
        if (rb.shape_type == RIGID_SHAPE_SPHERE) {
            addSphere(v, kx * kSphereShapeScale, c);
        }
        else if (rb.shape_type == RIGID_SHAPE_BOX) {
            addBox(v, {kx * kBoxShapeScale, ky * kBoxShapeScale, kz * kBoxShapeScale}, c);
        }
        else if (rb.shape_type == RIGID_SHAPE_CAPSULE) {
            addCapsule(v, kx * kCapsuleShapeScale, ky * kCapsuleShapeScale, c);
        }
        else {
            addSphere(v, kx * kSphereShapeScale, c);
        }

        float rx = rb.shape_rotation.x, ry = rb.shape_rotation.y, rz = rb.shape_rotation.z;
        float crx = cosf(rx), srx = sinf(rx), cry = cosf(ry), sry = sinf(ry), crz = cosf(rz),
              srz = sinf(rz);
        // YXZ rotation order: Ry(ry) * Rx(rx) * Rz(rz) — matches MMD/saba
        float R[9] = {cry * crz + sry * srx * srz,
                      -cry * srz + sry * srx * crz,
                      sry * crx,
                      crx * srz,
                      crx * crz,
                      -srx,
                      -sry * crz + cry * srx * srz,
                      sry * srz + cry * srx * crz,
                      cry * crx};

        for (size_t j = 0; j < v.size(); j += 9) {
            // Transform position
            float lx = v[j], ly = v[j + 1], lz = v[j + 2];
            v[j] = R[0] * lx + R[1] * ly + R[2] * lz + rb.shape_position.x;
            v[j + 1] = R[3] * lx + R[4] * ly + R[5] * lz + rb.shape_position.y;
            v[j + 2] = R[6] * lx + R[7] * ly + R[8] * lz + rb.shape_position.z;
            // PMX world-space vertex — modelMat on GPU handles display transform
            // Transform normal (rotation only)
            float nx = v[j + 6], ny = v[j + 7], nz = v[j + 8];
            v[j + 6] = R[0] * nx + R[1] * ny + R[2] * nz;
            v[j + 7] = R[3] * nx + R[4] * ny + R[5] * nz;
            v[j + 8] = R[6] * nx + R[7] * ny + R[8] * nz;
        }
        rbVerts.insert(rbVerts.end(), v.begin(), v.end());
        int vc = (int)v.size() / 9;
        for (int j = 0; j < vc; ++j)
            rbBoneIdx.push_back(forBone ? rb.bone_index : -1);
    }

    setupVao(rbVao, rbVerts, rbBoneIdx);

    // --- Joints (keep as lines, 6 floats per vertex) ---
    std::vector<float> jtVerts;
    std::vector<int32_t> jtBoneIdx;

    for (size_t i = 0; i < model.joints.size(); ++i) {
        const auto& jt = model.joints[i];
        int boneIdx =
            jt.rigidbody_index_a >= 0 && jt.rigidbody_index_a < (int)model.rigidbodies.size()
                ? model.rigidbodies[jt.rigidbody_index_a].bone_index
                : -1;
        float x = jt.position.x, y = jt.position.y, z = jt.position.z;
        // PMX world space — modelMat handles display transform
        Vec3 col = {0, 1, 0};
        float s = 0.05f;
        size_t base = jtVerts.size() / 6;
        jtVerts.insert(jtVerts.end(),
                       {x - s, y, z, col.x, col.y, col.z, x + s, y, z, col.x, col.y, col.z});
        jtVerts.insert(jtVerts.end(),
                       {x, y - s, z, col.x, col.y, col.z, x, y + s, z, col.x, col.y, col.z});
        jtVerts.insert(jtVerts.end(),
                       {x, y, z - s, col.x, col.y, col.z, x, y, z + s, col.x, col.y, col.z});
        int vc = (int)(jtVerts.size() / 6) - (int)base;
        for (int j = 0; j < vc; ++j)
            jtBoneIdx.push_back(forBone ? boneIdx : -1);
    }

    if (!jtVerts.empty()) {
        jtVao.bind();
        GLuint vbo;
        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, jtVerts.size() * sizeof(float), jtVerts.data(),
                     GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                              (void*)(3 * sizeof(float)));
        jtVao.vbos.push_back(vbo);
        GLuint bvbo;
        glGenBuffers(1, &bvbo);
        glBindBuffer(GL_ARRAY_BUFFER, bvbo);
        glBufferData(GL_ARRAY_BUFFER, jtBoneIdx.size() * sizeof(int32_t), jtBoneIdx.data(),
                     GL_STATIC_DRAW);
        glEnableVertexAttribArray(2);
        glVertexAttribIPointer(2, 1, GL_INT, 0, nullptr);
        jtVao.vbos.push_back(bvbo);
        jtVao.vertexCount = (int)jtVerts.size() / 6;
        Gpu::Vao::unbind();
    }
}

void RigidBodyRenderer::build(const PmxModel& model, float modelScale) {
    Vec3 minPos = {1e9f, 1e9f, 1e9f}, maxPos = {-1e9f, -1e9f, -1e9f};
    for (const auto& v : model.vertices) {
        minPos.x = std::min(minPos.x, v.position.x);
        minPos.y = std::min(minPos.y, v.position.y);
        minPos.z = std::min(minPos.z, v.position.z);
        maxPos.x = std::max(maxPos.x, v.position.x);
        maxPos.y = std::max(maxPos.y, v.position.y);
        maxPos.z = std::max(maxPos.z, v.position.z);
    }
    mCx = (minPos.x + maxPos.x) * 0.5f;
    mMy = minPos.y;
    mCz = (minPos.z + maxPos.z) * 0.5f;
    mModelScale = modelScale;

    buildPass(model, mCx, mMy, mCz, modelScale, false, mRbStatic, mJtStatic);
    buildPass(model, mCx, mMy, mCz, modelScale, true, mRbAnimated, mJtAnimated);
}

// Update debug visualization from live physics state.
// Strategy: upload each body's world matrix to a floating-point texture,
// then use instanced rendering in the vertex shader (sampling the texture
// by gl_InstanceID) to transform each shape from local space to world space.
// The VAO is rebuilt only when the body count changes.
void RigidBodyRenderer::updateFromPhysics(const PhysicsWorld& world) {
    const auto& bodies = world.bodies();
    int nBodies = (int)bodies.size();

    // Pack body world matrices into a flat array for texture upload.
    // Each body gets a column-major 4×4 matrix (16 floats).
    std::vector<float> bodyMats(nBodies * 16, 0);
    for (int i = 0; i < nBodies; ++i) {
        if (!bodies[i].body)
            continue;
        btTransform t = bodies[i].body->getCenterOfMassTransform();
        btQuaternion r = t.getRotation();
        btVector3 p = t.getOrigin();
        float* m = bodyMats.data() + i * 16;
        // Column-major 4x4: body rotation + PMX-world translation (body pos is center-relative, add center back)
        m[0] = 1 - 2 * (r.y() * r.y() + r.z() * r.z());
        m[4] = 2 * (r.x() * r.y() - r.z() * r.w());
        m[8] = 2 * (r.x() * r.z() + r.y() * r.w());
        m[12] = p.x() + mCx;
        m[1] = 2 * (r.x() * r.y() + r.z() * r.w());
        m[5] = 1 - 2 * (r.x() * r.x() + r.z() * r.z());
        m[9] = 2 * (r.y() * r.z() - r.x() * r.w());
        m[13] = p.y() + mMy;
        m[2] = 2 * (r.x() * r.z() - r.y() * r.w());
        m[6] = 2 * (r.y() * r.z() + r.x() * r.w());
        m[10] = 1 - 2 * (r.x() * r.x() + r.y() * r.y());
        m[14] = p.z() + mCz;
        m[3] = 0;
        m[7] = 0;
        m[11] = 0;
        m[15] = 1;
    }

    // Upload body matrices to texture
    int texW = 64;
    int texelsPerBody = 4;
    int totalTexels = nBodies * texelsPerBody;
    int texH = (totalTexels + texW - 1) / texW;
    if (texH < 1)
        texH = 1;

    bool rebuildTex = (mBodyTex == nullptr || mBodyTex->width != texW || mBodyTex->height != texH);
    if (rebuildTex) {
        mBodyTex = std::make_unique<Gpu::Texture>(texW, texH, 4, bodyMats.data(), GL_FLOAT);
        mBodyTex->setFilter(GL_NEAREST, GL_NEAREST);
        mBodyTex->setWrap(false, false);
        mBodyTexWidth = texW;
        mBodyCount = nBodies;
    }
    else {
        mBodyTex->write(bodyMats.data());
    }

    // Build local-space VAO once (rebuilt only when body count changes)
    if (nBodies != mBodyCount || mRbPhysics.vertexCount == 0) {
        mRbPhysics.destroy();
        mRbPhysics = Gpu::Vao();

        std::vector<float> rbVerts;
        std::vector<int32_t> rbBoneIdx;

        for (int i = 0; i < nBodies; ++i) {
            const auto& bb = bodies[i];
            if (!bb.body)
                continue;
            // Color by mode: green=static(0), orange=dynamic(1), blue=bone-align(2)
            Vec3 c;
            switch (bb.mode) {
            case 0: c = {0.0f, 1.0f, 0.0f}; break;
            case 2: c = {0.2f, 0.6f, 1.0f}; break;
            default: c = {1.0f, 0.5f, 0.0f}; break;
            }

            btCollisionShape* shape = bb.body->getCollisionShape();
            int shapeType = shape->getShapeType();
            std::vector<float> v;

            if (shapeType == SPHERE_SHAPE_PROXYTYPE) {
                float r = ((btSphereShape*)shape)->getRadius();
                addSphere(v, r, c);
            }
            else if (shapeType == BOX_SHAPE_PROXYTYPE) {
                btVector3 half = ((btBoxShape*)shape)->getHalfExtentsWithMargin();
                addBox(v, {half.x(), half.y(), half.z()}, c);
            }
            else if (shapeType == CAPSULE_SHAPE_PROXYTYPE) {
                auto* cap = (btCapsuleShape*)shape;
                addCapsule(v, cap->getRadius(), cap->getHalfHeight() * 2, c);
            }
            else {
                addSphere(v, 0.05f, c);
            }

            // Vertices are in LOCAL shape space — world transform applied via body matrix in shader
            rbVerts.insert(rbVerts.end(), v.begin(), v.end());
            int vc = (int)v.size() / 9;
            for (int j = 0; j < vc; ++j)
                rbBoneIdx.push_back(i);
        }

        if (!rbVerts.empty())
            setupVao(mRbPhysics, rbVerts, rbBoneIdx);
        mBodyCount = nBodies;
    }
}

void RigidBodyRenderer::render(Gpu::ShaderProgram& shader, const std::array<float, 16>& projection,
                               const std::array<float, 16>& view,
                               const float* modelMatParam) const {
    float defMat[] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 1};
    const float* mm = modelMatParam ? modelMatParam : defMat;

    shader.use();
    shader.setMat4(U_PROJ_MAT, projection.data());
    shader.setMat4(U_VIEW_MAT, view.data());
    shader.setMat4(U_MODEL_MAT, mm);

    bool hasPhysics = (mRbPhysics.vertexCount > 0);
    const Gpu::Vao& rb = hasPhysics ? mRbPhysics : (useBoneMatrices ? mRbAnimated : mRbStatic);
    const Gpu::Vao& jt = hasPhysics ? mJtPhysics : (useBoneMatrices ? mJtAnimated : mJtStatic);

    if (showRigidBody && rb.vertexCount > 0) {
        if (hasPhysics && mBodyTex) {
            shader.setInt(U_BONE_TEX_WIDTH, mBodyTexWidth);
            mBodyTex->bind(TEX_UNIT_BONE);
            shader.setInt(U_BONE_TEX, TEX_UNIT_BONE);
        }
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        rb.render(GL_TRIANGLES);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
    if (showJoint && jt.vertexCount > 0)
        jt.render(GL_LINES);
}
