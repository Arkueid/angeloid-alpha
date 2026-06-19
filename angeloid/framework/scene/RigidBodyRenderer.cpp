#include "framework/scene/RigidBodyRenderer.h"

#include "core/anim/PhysicsWorld.h"
#include "framework/ShaderManager.h"
#include "framework/ShaderStandard.h"
#include "framework/gpu/IGpuDevice.h"
#include "framework/gpu/Types.h"

#include <algorithm>
#include <btBulletDynamicsCommon.h>
#include <cmath>

// Procedural shape generators for debug visualization of physics bodies.
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
            emitV(verts, a[0], a[1], a[2], c); emitV(verts, b[0], b[1], b[2], c); emitV(verts, p[0], p[1], p[2], c);
            emitV(verts, a[0], a[1], a[2], c); emitV(verts, p[0], p[1], p[2], c); emitV(verts, d[0], d[1], d[2], c);
        }
    }
}

static void addCapsule(std::vector<float>& verts, float r, float h, const Vec3& c) {
    int slices = 12, hStacks = 3;
    float hh = h * 0.5f;
    float topPts[4][13][3], botPts[4][13][3];

    topPts[0][0][0] = 0; topPts[0][0][1] = hh + r; topPts[0][0][2] = 0;
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
    for (int i = 0; i < slices; ++i) {
        float *a = topPts[0][0], *b = topPts[1][i + 1], *p = topPts[1][i];
        emitV(verts, a[0], a[1], a[2], c); emitV(verts, b[0], b[1], b[2], c); emitV(verts, p[0], p[1], p[2], c);
    }
    for (int s = 1; s < hStacks; ++s) {
        for (int i = 0; i < slices; ++i) {
            float *a = topPts[s][i], *b = topPts[s + 1][i], *p = topPts[s + 1][i + 1], *d = topPts[s][i + 1];
            emitV(verts, a[0], a[1], a[2], c); emitV(verts, b[0], b[1], b[2], c); emitV(verts, p[0], p[1], p[2], c);
            emitV(verts, a[0], a[1], a[2], c); emitV(verts, p[0], p[1], p[2], c); emitV(verts, d[0], d[1], d[2], c);
        }
    }

    botPts[0][0][0] = 0; botPts[0][0][1] = -hh - r; botPts[0][0][2] = 0;
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
    for (int i = 0; i < slices; ++i) {
        float *a = botPts[0][0], *b = botPts[1][i], *p = botPts[1][i + 1];
        emitV(verts, a[0], a[1], a[2], c); emitV(verts, b[0], b[1], b[2], c); emitV(verts, p[0], p[1], p[2], c);
    }
    for (int s = 1; s < hStacks; ++s) {
        for (int i = 0; i < slices; ++i) {
            float *a = botPts[s][i], *b = botPts[s + 1][i], *p = botPts[s + 1][i + 1], *d = botPts[s][i + 1];
            emitV(verts, a[0], a[1], a[2], c); emitV(verts, b[0], b[1], b[2], c); emitV(verts, p[0], p[1], p[2], c);
            emitV(verts, a[0], a[1], a[2], c); emitV(verts, p[0], p[1], p[2], c); emitV(verts, d[0], d[1], d[2], c);
        }
    }

    for (int i = 0; i < slices; ++i) {
        int j = (i + 1) % slices;
        float *t1 = topPts[hStacks][i], *t2 = topPts[hStacks][j];
        float *b1 = botPts[hStacks][i], *b2 = botPts[hStacks][j];
        emitV(verts, t1[0], t1[1], t1[2], c); emitV(verts, t2[0], t2[1], t2[2], c); emitV(verts, b2[0], b2[1], b2[2], c);
        emitV(verts, t1[0], t1[1], t1[2], c); emitV(verts, b2[0], b2[1], b2[2], c); emitV(verts, b1[0], b1[1], b1[2], c);
    }
}

// ── VAO builders ──

// RB layout: pos(3f) + color(3f) + normal(3f) interleaved @ 9*sizeof(float), separate boneIdx(1i)
static std::unique_ptr<Gpu::IGpuVertexArray>
buildRbVao(const std::vector<float>& verts, const std::vector<int32_t>& boneIdx,
           std::vector<std::unique_ptr<Gpu::IGpuBuffer>>& keepAlive) {
    if (verts.empty()) return nullptr;

    auto* dev = Gpu::device();
    size_t stride = 9 * sizeof(float);

    auto vbo = dev->createVertexBuffer(verts.data(), verts.size() * sizeof(float), Gpu::BufferUsage::Static);
    auto bvbo = dev->createVertexBuffer(boneIdx.data(), boneIdx.size() * sizeof(int32_t), Gpu::BufferUsage::Static);

    std::vector<Gpu::VertexAttribute> attrs = {
        {0, 3, Gpu::DataType::Float,  (int)stride, 0},
        {1, 3, Gpu::DataType::Float,  (int)stride, 3 * (int)sizeof(float)},
        {2, 1, Gpu::DataType::Int32,  0,           0},
        {3, 3, Gpu::DataType::Float,  (int)stride, 6 * (int)sizeof(float)},
    };
    std::vector<Gpu::IGpuBuffer*> bufs = {vbo.get(), vbo.get(), bvbo.get(), vbo.get()};

    int vc = (int)verts.size() / 9;
    auto vao = dev->createVertexArray(attrs, bufs, nullptr, Gpu::IndexType::UInt32, vc, 0);

    keepAlive.push_back(std::move(vbo));
    keepAlive.push_back(std::move(bvbo));
    return vao;
}

// Joint layout: pos(3f) + color(3f) interleaved @ 6*sizeof(float), separate boneIdx(1i)
static std::unique_ptr<Gpu::IGpuVertexArray>
buildJtVao(const std::vector<float>& verts, const std::vector<int32_t>& boneIdx,
           std::vector<std::unique_ptr<Gpu::IGpuBuffer>>& keepAlive) {
    if (verts.empty()) return nullptr;

    auto* dev = Gpu::device();
    size_t stride = 6 * sizeof(float);

    auto vbo = dev->createVertexBuffer(verts.data(), verts.size() * sizeof(float), Gpu::BufferUsage::Static);
    auto bvbo = dev->createVertexBuffer(boneIdx.data(), boneIdx.size() * sizeof(int32_t), Gpu::BufferUsage::Static);

    std::vector<Gpu::VertexAttribute> attrs = {
        {0, 3, Gpu::DataType::Float, (int)stride, 0},
        {1, 3, Gpu::DataType::Float, (int)stride, 3 * (int)sizeof(float)},
        {2, 1, Gpu::DataType::Int32, 0,           0},
    };
    std::vector<Gpu::IGpuBuffer*> bufs = {vbo.get(), vbo.get(), bvbo.get()};

    int vc = (int)verts.size() / 6;
    auto vao = dev->createVertexArray(attrs, bufs, nullptr, Gpu::IndexType::UInt32, vc, 0);

    keepAlive.push_back(std::move(vbo));
    keepAlive.push_back(std::move(bvbo));
    return vao;
}

static void buildPass(const PmxModel& model, float cx, float my, float cz, float ms, bool forBone,
                      std::unique_ptr<Gpu::IGpuVertexArray>& rbVao,
                      std::unique_ptr<Gpu::IGpuVertexArray>& jtVao,
                      std::vector<std::unique_ptr<Gpu::IGpuBuffer>>& keepAlive) {
    (void)cx; (void)my; (void)cz; (void)ms;

    // Rigid bodies
    std::vector<float> rbVerts;
    std::vector<int32_t> rbBoneIdx;

    for (size_t i = 0; i < model.rigidbodies.size(); ++i) {
        const auto& rb = model.rigidbodies[i];
        Vec3 c;
        switch (rb.mode) {
        case 0: c = {0.0f, 1.0f, 0.0f}; break;
        case 2: c = {0.2f, 0.6f, 1.0f}; break;
        default: c = {1.0f, 0.5f, 0.0f}; break;
        }
        float kx = rb.shape_size.x, ky = rb.shape_size.y, kz = rb.shape_size.z;

        std::vector<float> v;
        if (rb.shape_type == RIGID_SHAPE_SPHERE) {
            addSphere(v, kx * kSphereShapeScale, c);
        } else if (rb.shape_type == RIGID_SHAPE_BOX) {
            addBox(v, {kx * kBoxShapeScale, ky * kBoxShapeScale, kz * kBoxShapeScale}, c);
        } else if (rb.shape_type == RIGID_SHAPE_CAPSULE) {
            addCapsule(v, kx * kCapsuleShapeScale, ky * kCapsuleShapeScale, c);
        } else {
            addSphere(v, kx * kSphereShapeScale, c);
        }

        float rx = rb.shape_rotation.x, ry = rb.shape_rotation.y, rz = rb.shape_rotation.z;
        float crx = cosf(rx), srx = sinf(rx), cry = cosf(ry), sry = sinf(ry), crz = cosf(rz), srz = sinf(rz);
        float R[9] = {cry * crz + sry * srx * srz, -cry * srz + sry * srx * crz, sry * crx,
                      crx * srz, crx * crz, -srx,
                      -sry * crz + cry * srx * srz, sry * srz + cry * srx * crz, cry * crx};

        for (size_t j = 0; j < v.size(); j += 9) {
            float lx = v[j], ly = v[j + 1], lz = v[j + 2];
            v[j] = R[0] * lx + R[1] * ly + R[2] * lz + rb.shape_position.x;
            v[j + 1] = R[3] * lx + R[4] * ly + R[5] * lz + rb.shape_position.y;
            v[j + 2] = R[6] * lx + R[7] * ly + R[8] * lz + rb.shape_position.z;
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

    rbVao = buildRbVao(rbVerts, rbBoneIdx, keepAlive);

    // Joints
    std::vector<float> jtVerts;
    std::vector<int32_t> jtBoneIdx;

    for (size_t i = 0; i < model.joints.size(); ++i) {
        const auto& jt = model.joints[i];
        int boneIdx = jt.rigidbody_index_a >= 0 && jt.rigidbody_index_a < (int)model.rigidbodies.size()
                          ? model.rigidbodies[jt.rigidbody_index_a].bone_index : -1;
        float x = jt.position.x, y = jt.position.y, z = jt.position.z;
        Vec3 col = {0, 1, 0};
        float s = 0.05f;
        size_t base = jtVerts.size() / 6;
        jtVerts.insert(jtVerts.end(), {x - s, y, z, col.x, col.y, col.z, x + s, y, z, col.x, col.y, col.z});
        jtVerts.insert(jtVerts.end(), {x, y - s, z, col.x, col.y, col.z, x, y + s, z, col.x, col.y, col.z});
        jtVerts.insert(jtVerts.end(), {x, y, z - s, col.x, col.y, col.z, x, y, z + s, col.x, col.y, col.z});
        int vc = (int)(jtVerts.size() / 6) - (int)base;
        for (int j = 0; j < vc; ++j)
            jtBoneIdx.push_back(forBone ? boneIdx : -1);
    }

    jtVao = buildJtVao(jtVerts, jtBoneIdx, keepAlive);
}

void RigidBodyRenderer::build(const PmxModel& model, float modelScale,
                               const float* modelMat,
                               const PhysicsWorld* physicsWorld) {
    mModelScale = modelScale;
    mModelMat = modelMat;
    mPhysicsWorld = physicsWorld;
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

    buildPass(model, mCx, mMy, mCz, modelScale, false, mRbStatic, mJtStatic, mBuffers);
    buildPass(model, mCx, mMy, mCz, modelScale, true, mRbAnimated, mJtAnimated, mBuffers);
}

void RigidBodyRenderer::updateFromPhysics() {
    if (!mPhysicsWorld) return;
    const auto& bodies = mPhysicsWorld->bodies();
    int nBodies = (int)bodies.size();

    std::vector<float> bodyMats(nBodies * 16, 0);
    for (int i = 0; i < nBodies; ++i) {
        if (!bodies[i].body) continue;
        btTransform t = bodies[i].body->getCenterOfMassTransform();
        btQuaternion r = t.getRotation();
        btVector3 p = t.getOrigin();
        float* m = bodyMats.data() + i * 16;
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
        m[3] = 0; m[7] = 0; m[11] = 0; m[15] = 1;
    }

    int texW = 64;
    int texelsPerBody = 4;
    int totalTexels = nBodies * texelsPerBody;
    int texH = (totalTexels + texW - 1) / texW;
    if (texH < 1) texH = 1;

    bool rebuildTex = (mBodyTex == nullptr || mBodyTex->width() != texW || mBodyTex->height() != texH);
    if (rebuildTex) {
        mBodyTex = Gpu::device()->createTexture(texW, texH, Gpu::TextureFormat::RGBA32F, bodyMats.data());
        mBodyTex->setFilter(Gpu::TextureFilter::Nearest, Gpu::TextureFilter::Nearest);
        mBodyTex->setWrap(Gpu::TextureWrap::Clamp, Gpu::TextureWrap::Clamp);
        mBodyTexWidth = texW;
        mBodyCount = nBodies;
    } else {
        mBodyTex->write(bodyMats.data());
    }

    if (nBodies != mBodyCount || mRbPhysics == nullptr) {
        mRbPhysics.reset();
        mJtPhysics.reset();

        std::vector<float> rbVerts;
        std::vector<int32_t> rbBoneIdx;

        for (int i = 0; i < nBodies; ++i) {
            const auto& bb = bodies[i];
            if (!bb.body) continue;
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
            } else if (shapeType == BOX_SHAPE_PROXYTYPE) {
                btVector3 half = ((btBoxShape*)shape)->getHalfExtentsWithMargin();
                addBox(v, {half.x(), half.y(), half.z()}, c);
            } else if (shapeType == CAPSULE_SHAPE_PROXYTYPE) {
                auto* cap = (btCapsuleShape*)shape;
                addCapsule(v, cap->getRadius(), cap->getHalfHeight() * 2, c);
            } else {
                addSphere(v, 0.05f, c);
            }

            rbVerts.insert(rbVerts.end(), v.begin(), v.end());
            int vc = (int)v.size() / 9;
            for (int j = 0; j < vc; ++j)
                rbBoneIdx.push_back(i);
        }

        if (!rbVerts.empty()) {
            mRbPhysics = buildRbVao(rbVerts, rbBoneIdx, mBuffers);
            mBodyCount = nBodies;
        }
    }
}

void RigidBodyRenderer::onDebugPass(const DebugPassParams& dp) {
    updateFromPhysics();

    auto* s = ShaderManager::instance().rigidBody();
    if (!s) return;

    auto* dev = Gpu::device();

    s->use();
    s->setMat4(U_PROJ_MAT, dp.proj.data());
    s->setMat4(U_VIEW_MAT, dp.view.data());
    s->setMat4(U_MODEL_MAT, mModelMat);

    bool hasPhysics = (mRbPhysics != nullptr);
    Gpu::IGpuVertexArray& rb = hasPhysics ? *mRbPhysics : (useBoneMatrices ? *mRbAnimated : *mRbStatic);
    Gpu::IGpuVertexArray* jtPtr = hasPhysics ? mJtPhysics.get() : (useBoneMatrices ? mJtAnimated.get() : mJtStatic.get());

    if (showRigidBody) {
        if (hasPhysics && mBodyTex) {
            s->setInt(U_BONE_TEX_WIDTH, mBodyTexWidth);
            mBodyTex->bind(TEX_UNIT_BONE);
            s->setInt(U_BONE_TEX, TEX_UNIT_BONE);
        }
        dev->setPolygonMode(Gpu::PolygonMode::Line);
        rb.draw(Gpu::PrimitiveType::Triangles);
        dev->setPolygonMode(Gpu::PolygonMode::Fill);
    }
    if (showJoint && jtPtr)
        jtPtr->draw(Gpu::PrimitiveType::Lines);
}
