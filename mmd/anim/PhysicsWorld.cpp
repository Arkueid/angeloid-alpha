#include "anim/PhysicsWorld.h"

#include "anim/BoneSkinning.h"
#include "util/Log.h"

#include <BulletDynamics/ConstraintSolver/btGeneric6DofSpringConstraint.h>
#include <algorithm>
#include <btBulletDynamicsCommon.h>
#include <cmath>
#include <string>

namespace {
// Gravity in MMD world space; modelScale division in build() normalizes for PMX-native physics
constexpr float kGravityY = -9.8f;
// Default Bullet solver iteration count (2× default, improves joint stability)
constexpr int kSolverIterations = 15;
// Substep count per stepSimulation call
constexpr int kSubsteps = 10;
constexpr float kFixedTimestep = 1.0f / 240.0f;
// Cap deltaTime to prevent explosion after frame spikes (e.g. window drag, breakpoint)
constexpr float kMaxTimestep = 1.0f / 30.0f;
constexpr float kSleepLinearThreshold = 0.08f;
constexpr float kSleepAngularThreshold = 0.02f;
constexpr float kDeactivationTime = 0.5f;
}

PhysicsWorld::PhysicsWorld() {
    mCollisionCfg = std::make_unique<btDefaultCollisionConfiguration>();
    mDispatcher = std::make_unique<btCollisionDispatcher>(mCollisionCfg.get());
    mBroadphase = std::make_unique<btDbvtBroadphase>();
    mSolver = std::make_unique<btSequentialImpulseConstraintSolver>();
    mWorld = std::make_unique<btDiscreteDynamicsWorld>(mDispatcher.get(), mBroadphase.get(),
                                                       mSolver.get(), mCollisionCfg.get());
    mWorld->setGravity(btVector3(0, kGravityY, 0));
    mWorld->getSolverInfo().m_numIterations = kSolverIterations;
    // ERP2 (Error Reduction Parameter) controls Baumgarte stabilization strength.
    // Bullet 3.x changed the default to 0.2; we restore 0.8 matching Bullet 2.75
    // — without this, joints drift visibly over time.
    mWorld->getSolverInfo().m_erp2 = 0.8f;
}

PhysicsWorld::~PhysicsWorld() {
    for (auto& c : mConstraints)
        if (c)
            mWorld->removeConstraint(c.get());
    for (auto& b : mBodies)
        if (b.body)
            mWorld->removeRigidBody(b.body);
}

void PhysicsWorld::build(const PmxModel& model, float modelScale) {
    // Compute bounds
    float minX = 1e9f, minY = 1e9f, minZ = 1e9f, maxX = -1e9f, maxY = -1e9f, maxZ = -1e9f;
    for (const auto& v : model.vertices) {
        minX = std::min(minX, v.position.x);
        maxX = std::max(maxX, v.position.x);
        minY = std::min(minY, v.position.y);
        maxY = std::max(maxY, v.position.y);
        minZ = std::min(minZ, v.position.z);
        maxZ = std::max(maxZ, v.position.z);
    }
    mCenter = {(minX + maxX) * 0.5f, (minY + maxY) * 0.5f, (minZ + maxZ) * 0.5f};
    mMinY = minY;
    mModelScale = modelScale;
    mWorld->setGravity(btVector3(0, kGravityY / modelScale, 0));

    mBoneBindWorld = BoneSkinning::computeBindWorldMatrices(model);

    for (auto& c : mConstraints) {
        if (c)
            mWorld->removeConstraint(c.get());
    }
    mConstraints.clear();
    for (auto& b : mBodies) {
        if (b.body)
            mWorld->removeRigidBody(b.body);
    }
    mBodies.clear();
    mShapes.clear();

    int countMode[3] = {};
    for (const auto& rb : model.rigidbodies) {
        if (rb.mode >= 0 && rb.mode < 3)
            countMode[rb.mode]++;
    }
    MMD_INFO("PHYS", "%zu bodies (mode0/static=%d mode1/dyn=%d mode2/align=%d), %zu joints",
             model.rigidbodies.size(), countMode[0], countMode[1], countMode[2],
             model.joints.size());
    MMD_INFO("PHYS", "  modelScale=%.6f  gravity=(0, %.4f, 0)  center=(%.4f,%.4f,%.4f)  minY=%.4f",
             modelScale, kGravityY / modelScale, mCenter.x, mCenter.y, mCenter.z, mMinY);

    // --- Per-body dump (debug only) ---
    const char* kShapeNames[] = {"Sphere", "Box", "Capsule"};
    const char* kModeNames[] = {"STATIC", "DYNAMIC", "ALIGN"};
    for (const auto& rb : model.rigidbodies) {
        const char* shapeName =
            (rb.shape_type >= 0 && rb.shape_type < 3) ? kShapeNames[rb.shape_type] : "?";
        const char* modeName =
            (rb.mode >= 0 && rb.mode < 3) ? kModeNames[rb.mode] : "?";
        const char* boneName =
            (rb.bone_index >= 0 && rb.bone_index < (int)model.bones.size())
                ? model.bones[rb.bone_index].name.c_str()
                : "(none)";
        MMD_DEBUG("PHYS",
                  "  BODY[%d] \"%s\" mode=%s shape=%s size=(%.4f,%.4f,%.4f) "
                  "bone[%d]=\"%s\" mass=%.4f linDamp=%.3f angDamp=%.3f "
                  "colGroup=0x%04x noColGroup=0x%04x",
                  rb.index, rb.name.c_str(), modeName, shapeName, rb.shape_size.x, rb.shape_size.y,
                  rb.shape_size.z, rb.bone_index, boneName, rb.mass, rb.linear_damping,
                  rb.angular_damping, rb.collision_group, rb.no_collision_group);
    }

    for (const auto& rb : model.rigidbodies)
        addRigidBody(rb);

    int dynMassCount = 0;
    for (const auto& rb : model.rigidbodies) {
        if ((rb.mode == 1 || rb.mode == 2) && rb.mass > 0)
            dynMassCount++;
    }
    MMD_INFO("PHYS", "  Dynamic mass bodies: %d", dynMassCount);

    // --- Per-joint dump (debug only) ---
    for (const auto& jt : model.joints) {
        const char* nameA =
            (jt.rigidbody_index_a >= 0 && jt.rigidbody_index_a < (int)model.rigidbodies.size())
                ? model.rigidbodies[jt.rigidbody_index_a].name.c_str()
                : "?";
        const char* nameB =
            (jt.rigidbody_index_b >= 0 && jt.rigidbody_index_b < (int)model.rigidbodies.size())
                ? model.rigidbodies[jt.rigidbody_index_b].name.c_str()
                : "?";
        MMD_DEBUG("PHYS",
                  "  JOINT[%d] type=%d \"%s\"<->\"%s\" pos=(%.4f,%.4f,%.4f)",
                  jt.index, jt.joint_type, nameA, nameB, jt.position.x, jt.position.y,
                  jt.position.z);
    }

    for (const auto& jt : model.joints)
        addJoint(jt);

    // Mark mode-1 SPHERE bodies with >=3 joints as skipBoneFeedback.
    // These are star-topology constraint systems (chest/buttocks) where body
    // displacement is a solver compromise, not articulation. Grid cloth panels
    // (boxes) and chain bodies (<=2 joints) are unaffected.
    {
        std::vector<int> jointCount(mBodies.size(), 0);
        for (const auto& jt : model.joints) {
            if (jt.rigidbody_index_a >= 0 && jt.rigidbody_index_a < (int)mBodies.size())
                jointCount[jt.rigidbody_index_a]++;
            if (jt.rigidbody_index_b >= 0 && jt.rigidbody_index_b < (int)mBodies.size())
                jointCount[jt.rigidbody_index_b]++;
        }
        for (size_t i = 0; i < mBodies.size(); ++i) {
            if (mBodies[i].mode == 1 &&
                jointCount[i] >= 3 &&
                mBodies[i].body->getCollisionShape()->getShapeType() == SPHERE_SHAPE_PROXYTYPE) {
                mBodies[i].skipBoneFeedback = true;
            }
        }
    }

    // Cloth detection heuristic: a body is "cloth-like" if it connects to a joint that
    // (a) has rotation springs (restorative torque) AND (b) has meaningful rotation range.
    // Cloth-like bodies use physics rotation + bone position in getBoneTransforms(),
    // preventing cloth from snapping back to bone orientation each frame.
    for (const auto& jt : model.joints) {
        bool hasRotSpring = jt.spring_constant_rotation.x != 0 ||
                            jt.spring_constant_rotation.y != 0 ||
                            jt.spring_constant_rotation.z != 0;
        float rlRange = fabsf(jt.rotation_limit_max.x - jt.rotation_limit_min.x) +
                        fabsf(jt.rotation_limit_max.y - jt.rotation_limit_min.y) +
                        fabsf(jt.rotation_limit_max.z - jt.rotation_limit_min.z);
        if (!hasRotSpring || rlRange < 0.01f)
            continue;
        auto mark = [&](int idx) {
            if (idx >= 0 && idx < (int)mBodies.size() && mBodies[idx].mode == 1)
                mBodies[idx].clothLike = true;
        };
        mark(jt.rigidbody_index_a);
        mark(jt.rigidbody_index_b);
    }
    int clothCount = 0;
    std::string clothNames;
    for (const auto& b : mBodies) {
        if (b.clothLike) {
            clothCount++;
            clothNames += " [" + std::to_string(b.rigidBodyIndex) + "]" + b.name;
        }
    }
    MMD_INFO("PHYS", "  Cloth-like bodies:%s (%d total)", clothNames.c_str(), clothCount);
}

void PhysicsWorld::resetPhysics(const std::vector<std::array<float, 16>>& poseWorld) {
    // Phase 1: align to init positions, make kinematic
    for (auto& bb : mBodies) {
        if (bb.mode == 0 || bb.boneIndex < 0 || bb.boneIndex >= (int)poseWorld.size())
            continue;
        if (bb.boneIndex >= (int)mBoneBindWorld.size())
            continue;

        bb.body->setCollisionFlags(bb.body->getCollisionFlags() |
                                   btCollisionObject::CF_KINEMATIC_OBJECT);
        btTransform cur;
        cur.setIdentity();
        cur.setOrigin(btVector3(bb.initPosX, bb.initPosY, bb.initPosZ));
        cur.setRotation(btQuaternion(bb.initRotX, bb.initRotY, bb.initRotZ, bb.initRotW));
        bb.body->getMotionState()->setWorldTransform(cur);
        bb.body->setCenterOfMassTransform(cur);
    }

    // Phase 2: run one step to resolve initial overlaps
    mWorld->stepSimulation(1.0f / 60.0f, 2, 1.0f / 120.0f);

    // Phase 3: clear forces, switch back to dynamic
    for (auto& bb : mBodies) {
        if (bb.mode == 0)
            continue;
        bb.body->setCollisionFlags(bb.body->getCollisionFlags() &
                                   ~btCollisionObject::CF_KINEMATIC_OBJECT);
        bb.body->clearForces();
        bb.body->setLinearVelocity(btVector3(0, 0, 0));
        bb.body->setAngularVelocity(btVector3(0, 0, 0));
        bb.body->getMotionState()->setWorldTransform(bb.body->getCenterOfMassTransform());
    }

    MMD_INFO("PHYS", "reset complete, bodies aligned");
}

void PhysicsWorld::debugDump() const {
    MMD_DEBUG("PHYS", "=== Physics dump (scale=%.4f center=%.2f,%.2f,%.2f) ===", mModelScale,
              mCenter.x, mCenter.y, mCenter.z);
    int moved = 0, active = 0;
    for (size_t i = 0; i < mBodies.size(); ++i) {
        const auto& bb = mBodies[i];
        if (!bb.body)
            continue;
        btTransform t = bb.body->getCenterOfMassTransform();
        btVector3 p = t.getOrigin();
        btVector3 init(bb.initPosX, bb.initPosY, bb.initPosZ);
        float disp = (p - init).length();
        if (bb.body->isActive())
            active++;
        if (disp > 0.02f) {
            moved++;
            float mx = p.x() + mCenter.x, my = p.y() + mMinY, mz = p.z() + mCenter.z;
            float ix = init.x() + mCenter.x, iy = init.y() + mMinY, iz = init.z() + mCenter.z;
            const char* modeStr = bb.mode == 0 ? "STATIC" : (bb.mode == 1 ? "dyn" : "ALIGN");
            float mass = bb.body->getInvMass() > 0 ? 1.0f / bb.body->getInvMass() : 0;
            MMD_DEBUG("PHYS",
                      "  [%zu] %s %s bone=%d dMMD=%.4f active=%d mass=%.3f now=(%.2f,%.2f,%.2f) "
                      "init=(%.2f,%.2f,%.2f)",
                      i, modeStr, bb.name.c_str(), bb.boneIndex, disp, bb.body->isActive() ? 1 : 0,
                      mass, mx, my, mz, ix, iy, iz);
        }
    }
    MMD_DEBUG("PHYS", "  Bodies displaced>0.02mmd: %d  active: %d  total: %zu", moved, active,
              mBodies.size());
    MMD_DEBUG("PHYS", "=== End dump ===");
}

void PhysicsWorld::addRigidBody(const PmxRigidBody& rb) {
    // PMX-native positions: offset from model center so physics runs near origin.
    // This avoids floating-point precision loss with large absolute coordinates.
    float px = rb.shape_position.x - mCenter.x;
    float py = rb.shape_position.y - mMinY;
    float pz = rb.shape_position.z - mCenter.z;

    // Shapes at PMX-native scale (no modelScale — physics runs in PMX space)
    btCollisionShape* shape = nullptr;
    if (rb.shape_type == RIGID_SHAPE_SPHERE)
        shape = new btSphereShape(rb.shape_size.x * kSphereShapeScale);
    else if (rb.shape_type == RIGID_SHAPE_BOX)
        shape = new btBoxShape(btVector3(rb.shape_size.x * kBoxShapeScale,
                                         rb.shape_size.y * kBoxShapeScale,
                                         rb.shape_size.z * kBoxShapeScale));
    else if (rb.shape_type == RIGID_SHAPE_CAPSULE) {
        float capR = rb.shape_size.x * kCapsuleShapeScale;
        float capH = rb.shape_size.y * kCapsuleShapeScale;
        float minR = 0.01f;
        if (capR < minR) {
            capR = minR;
        }
        shape = new btCapsuleShape(capR, capH);
    }
    else
        shape = new btSphereShape(rb.shape_size.x * kSphereShapeScale);
    mShapes.emplace_back(shape);

    // MMD rotation order: Y * X * Z (matches saba: ry * rx * rz)
    btQuaternion qx, qy, qz;
    qx.setRotation(btVector3(1, 0, 0), rb.shape_rotation.x);
    qy.setRotation(btVector3(0, 1, 0), rb.shape_rotation.y);
    qz.setRotation(btVector3(0, 0, 1), rb.shape_rotation.z);
    btQuaternion rot = qy * qx * qz;
    btTransform t;
    t.setIdentity();
    t.setOrigin(btVector3(px, py, pz));
    t.setRotation(rot);

    btScalar mass = (rb.mode == 0) ? 0 : rb.mass;
    btVector3 inertia(0, 0, 0);
    if (mass > 0)
        shape->calculateLocalInertia(mass, inertia);

    auto* ms = new btDefaultMotionState(t);
    btRigidBody::btRigidBodyConstructionInfo ci(mass, ms, shape, inertia);
    ci.m_restitution = rb.restitution;
    ci.m_friction = rb.friction;
    ci.m_linearDamping = rb.linear_damping;
    ci.m_angularDamping = rb.angular_damping;

    auto* body = new btRigidBody(ci);
    body->setUserIndex((int)mBodies.size());
    if (mass <= 0) {
        body->setCollisionFlags(body->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
    }
    body->setSleepingThresholds(kSleepLinearThreshold, kSleepAngularThreshold);
    body->setDeactivationTime(kDeactivationTime);

    // CCD for dynamic bodies to prevent tunneling
    if (mass > 0) {
        float ccdRadius = rb.shape_size.x * kSphereShapeScale;
        if (rb.shape_type == RIGID_SHAPE_CAPSULE)
            ccdRadius = rb.shape_size.x * kCapsuleShapeScale;
        if (rb.shape_type == RIGID_SHAPE_BOX)
            ccdRadius = std::min({rb.shape_size.x, rb.shape_size.y, rb.shape_size.z}) *
                        kBoxShapeScale * 0.5f;
        body->setCcdMotionThreshold(ccdRadius * 0.5f);
        body->setCcdSweptSphereRadius(ccdRadius);
    }

    mWorld->addRigidBody(body, 1 << rb.collision_group, rb.no_collision_group);
    btQuaternion initRot = t.getRotation();
    btVector3 initPos = t.getOrigin();

    // Precompute invBodyInit and boneBindMat for matrix-multiply bone feedback
    btTransform invBodyInit = t.inverse();
    btTransform boneBindMat;
    boneBindMat.setIdentity();
    if (rb.bone_index >= 0 && rb.bone_index < (int)mBoneBindWorld.size()) {
        const auto& bw = mBoneBindWorld[rb.bone_index];
        float bpx = bw[12] - mCenter.x;
        float bpy = bw[13] - mMinY;
        float bpz = bw[14] - mCenter.z;
        btMatrix3x3 bwBasis(bw[0], bw[4], bw[8], bw[1], bw[5], bw[9], bw[2], bw[6], bw[10]);
        btQuaternion bwRot;
        bwBasis.getRotation(bwRot);
        boneBindMat.setOrigin(btVector3(bpx, bpy, bpz));
        boneBindMat.setRotation(bwRot);
    }

    mBodies.push_back({body, rb.bone_index, rb.index, rb.mode,
                       initPos.x(), initPos.y(), initPos.z(),
                       initRot.x(), initRot.y(), initRot.z(), initRot.w(),
                       std::move(invBodyInit), std::move(boneBindMat),
                       false, false, rb.name});
}

void PhysicsWorld::addJoint(const PmxJoint& jt) {
    if (jt.rigidbody_index_a < 0 || jt.rigidbody_index_b < 0)
        return;
    if (jt.rigidbody_index_a >= (int)mBodies.size() || jt.rigidbody_index_b >= (int)mBodies.size())
        return;
    auto *a = mBodies[jt.rigidbody_index_a].body, *b = mBodies[jt.rigidbody_index_b].body;
    if (!a || !b)
        return;

    btVector3 pos(jt.position.x - mCenter.x, jt.position.y - mMinY, jt.position.z - mCenter.z);

    btQuaternion jqx, jqy, jqz;
    jqx.setRotation(btVector3(1, 0, 0), jt.rotation.x);
    jqy.setRotation(btVector3(0, 1, 0), jt.rotation.y);
    jqz.setRotation(btVector3(0, 0, 1), jt.rotation.z);
    btQuaternion jtRot = jqy * jqx * jqz;
    btMatrix3x3 jtBasis;
    jtBasis.setRotation(jtRot);

    btTransform jointTransform;
    jointTransform.setIdentity();
    jointTransform.setOrigin(pos);
    jointTransform.setBasis(jtBasis);

    btTransform invA = a->getCenterOfMassTransform().inverse();
    btTransform invB = b->getCenterOfMassTransform().inverse();
    invA = invA * jointTransform;
    invB = invB * jointTransform;

    btTypedConstraint* c = nullptr;

    switch (jt.joint_type) {
    case 0:     // 6DOF Spring — btGeneric6DofSpring2Constraint
    case 1:     // 6DOF — same constraint, PMX springs disabled per spec
        break;  // Handled below with the generic 6DOF path

    case 2: {  // P2P — btPoint2PointConstraint, limits/springs ignored
        c = new btPoint2PointConstraint(*a, *b, invA.getOrigin(), invB.getOrigin());
        break;
    }
    case 3: {  // ConeTwist — btConeTwistConstraint
        auto* ct = new btConeTwistConstraint(*a, *b, invA, invB);

        // Rotation limits map to swing/twist spans
        ct->setLimit(jt.rotation_limit_min.z,           // swingSpan1
                     jt.rotation_limit_min.y,           // swingSpan2
                     jt.rotation_limit_min.x,           // twistSpan
                     jt.spring_constant_translation.x,  // softness
                     jt.spring_constant_translation.y,  // biasFactor
                     jt.spring_constant_translation.z   // relaxationFactor
        );

        // Damping / fixThresh from translation limit fields
        float damping = jt.translation_limit_min.x;
        float fixThresh = jt.translation_limit_max.x;
        if (damping == 0 && fixThresh == 0) {
            damping = 0.1f;
            fixThresh = 0.1f;
        }
        ct->setDamping(damping);
        ct->setFixThresh(fixThresh);

        // Motor: enabled when translation_limit_min.z != 0
        if (jt.translation_limit_min.z != 0) {
            ct->enableMotor(true);
            ct->setMaxMotorImpulse(jt.translation_limit_max.z);
            btQuaternion motorTarget;
            motorTarget.setEulerZYX(jt.spring_constant_rotation.z, jt.spring_constant_rotation.y,
                                    jt.spring_constant_rotation.x);
            ct->setMotorTargetInConstraintSpace(motorTarget);
        }
        c = ct;
        break;
    }
    case 4: {  // Slider — btSliderConstraint (X-axis linear, X-axis angular)
        auto* sl = new btSliderConstraint(*a, *b, invA, invB, true);
        sl->setLowerLinLimit(jt.translation_limit_min.x);
        sl->setUpperLinLimit(jt.translation_limit_max.x);
        sl->setLowerAngLimit(jt.rotation_limit_min.x);
        sl->setUpperAngLimit(jt.rotation_limit_max.x);

        if (jt.spring_constant_translation.x != 0) {
            sl->setPoweredLinMotor(true);
            sl->setTargetLinMotorVelocity(jt.spring_constant_translation.y);
            sl->setMaxLinMotorForce(jt.spring_constant_translation.z);
        }
        if (jt.spring_constant_rotation.x != 0) {
            sl->setPoweredAngMotor(true);
            sl->setTargetAngMotorVelocity(jt.spring_constant_rotation.y);
            sl->setMaxAngMotorForce(jt.spring_constant_rotation.z);
        }
        c = sl;
        break;
    }
    case 5: {  // Hinge — btHingeConstraint (Z-axis rotation)
        auto* h = new btHingeConstraint(*a, *b, invA, invB);
        float softness = jt.spring_constant_translation.x;
        float biasFactor = jt.spring_constant_translation.y;
        float relaxFactor = jt.spring_constant_translation.z;
        if (softness == 0 && biasFactor == 0 && relaxFactor == 0) {
            softness = 0.9f;
            biasFactor = 0.3f;
            relaxFactor = 1.0f;
        }
        h->setLimit(jt.rotation_limit_min.x, jt.rotation_limit_max.x, softness, biasFactor,
                    relaxFactor);

        if (jt.spring_constant_rotation.x != 0) {
            h->enableAngularMotor(true, jt.spring_constant_rotation.y,
                                  jt.spring_constant_rotation.z);
        }
        c = h;
        break;
    }
    default: {  // Unknown — fall back to 6DOF spring
        // Falls through to same logic as case 0 below
        break;
    }
    }

    // Types 0, 1, and default use btGeneric6DofSpringConstraint
    if (!c &&
        (jt.joint_type == 0 || jt.joint_type == 1 || jt.joint_type < 0 || jt.joint_type > 5)) {
        auto* sc = new btGeneric6DofSpringConstraint(*a, *b, invA, invB, true);

        sc->setLinearLowerLimit(btVector3(jt.translation_limit_min.x, jt.translation_limit_min.y,
                                          jt.translation_limit_min.z));
        sc->setLinearUpperLimit(btVector3(jt.translation_limit_max.x, jt.translation_limit_max.y,
                                          jt.translation_limit_max.z));
        sc->setAngularLowerLimit(
            btVector3(jt.rotation_limit_min.x, jt.rotation_limit_min.y, jt.rotation_limit_min.z));
        sc->setAngularUpperLimit(
            btVector3(jt.rotation_limit_max.x, jt.rotation_limit_max.y, jt.rotation_limit_max.z));

        // PMX springs (type 0 and default only; type 1 skips per spec)
        if (jt.joint_type != 1) {
            const float* st = &jt.spring_constant_translation.x;
            const float* sr = &jt.spring_constant_rotation.x;
            if (st[0] != 0) {
                sc->enableSpring(0, true);
                sc->setStiffness(0, st[0]);
            }
            if (st[1] != 0) {
                sc->enableSpring(1, true);
                sc->setStiffness(1, st[1]);
            }
            if (st[2] != 0) {
                sc->enableSpring(2, true);
                sc->setStiffness(2, st[2]);
            }
            if (sr[0] != 0) {
                sc->enableSpring(3, true);
                sc->setStiffness(3, sr[0]);
            }
            if (sr[1] != 0) {
                sc->enableSpring(4, true);
                sc->setStiffness(4, sr[1]);
            }
            if (sr[2] != 0) {
                sc->enableSpring(5, true);
                sc->setStiffness(5, sr[2]);
            }
        }

        // Tiered spring fallback: for translation DOFs that are tightly constrained
        // (small range) but have no explicit PMX spring, inject a strong spring to
        // prevent drift. Type 0 joints with existing springs skip this.
        float lo[3] = {jt.translation_limit_min.x, jt.translation_limit_min.y,
                       jt.translation_limit_min.z};
        float hi[3] = {jt.translation_limit_max.x, jt.translation_limit_max.y,
                       jt.translation_limit_max.z};
        const float* st = &jt.spring_constant_translation.x;
        for (int i = 0; i < 3; ++i) {
            float range = fabsf(hi[i] - lo[i]);
            if (jt.joint_type == 0 && st[i] != 0)
                continue;
            float k = 0;
            if (range < 0.001f)
                k = 10000.0f;
            else if (range < 0.2f)
                k = 2000.0f;
            else if (range < 0.5f)
                k = 500.0f;
            if (k > 0) {
                sc->enableSpring(i, true);
                sc->setStiffness(i, k);
                sc->setDamping(i, 0.02f);
            }
        }

        // sc->setEquilibriumPoint();  // saba does not call this
        c = sc;
    }

    if (!c)
        return;
    mWorld->addConstraint(c, true);
    mConstraints.emplace_back(c);
}

// Compute the target transform a rigid body should follow based on its linked
// bone's current animation pose. Uses matrix multiplication matching saba:
//   bodyTarget = animBone * inv(bindBone) * bodyInit
void PhysicsWorld::computeBoneTarget(const BulletBody& bb,
                                     const std::vector<std::array<float, 16>>& poseWorld,
                                     btVector3& outPos, btQuaternion& outRot) const {
    const auto& pw = poseWorld[bb.boneIndex];
    const auto& bw = mBoneBindWorld[bb.boneIndex];

    // Build current animation bone transform (centered)
    btTransform animBone;
    animBone.setOrigin(btVector3(pw[12] - mCenter.x, pw[13] - mMinY, pw[14] - mCenter.z));
    btMatrix3x3 pwBasis(pw[0],pw[4],pw[8], pw[1],pw[5],pw[9], pw[2],pw[6],pw[10]);
    btQuaternion pwRot; pwBasis.getRotation(pwRot);
    animBone.setRotation(pwRot);

    // Build bind bone transform (centered)
    btTransform bindBone;
    bindBone.setOrigin(btVector3(bw[12] - mCenter.x, bw[13] - mMinY, bw[14] - mCenter.z));
    btMatrix3x3 bwBasis(bw[0],bw[4],bw[8], bw[1],bw[5],bw[9], bw[2],bw[6],bw[10]);
    btQuaternion bwRot; bwBasis.getRotation(bwRot);
    bindBone.setRotation(bwRot);

    // bodyTarget = animBone * inv(bindBone) * bodyInit
    btTransform bodyInit;
    bodyInit.setOrigin(btVector3(bb.initPosX, bb.initPosY, bb.initPosZ));
    bodyInit.setRotation(btQuaternion(bb.initRotX, bb.initRotY, bb.initRotZ, bb.initRotW));
    btTransform bodyTarget = animBone * bindBone.inverse() * bodyInit;

    outPos = bodyTarget.getOrigin();
    outRot = bodyTarget.getRotation();
}

void PhysicsWorld::updateMode0Bodies(const std::vector<std::array<float, 16>>& poseWorld) {
    for (auto& bb : mBodies) {
        if (bb.mode != 0 || bb.boneIndex < 0 || bb.boneIndex >= (int)poseWorld.size())
            continue;
        if (bb.boneIndex >= (int)mBoneBindWorld.size())
            continue;

        btVector3 newPos;
        btQuaternion newRot;
        computeBoneTarget(bb, poseWorld, newPos, newRot);

        btTransform cur;
        cur.setIdentity();
        cur.setOrigin(newPos);
        cur.setRotation(newRot);
        bb.body->getMotionState()->setWorldTransform(cur);
        bb.body->setCenterOfMassTransform(cur);
    }
}

void PhysicsWorld::step(float deltaTime, const std::vector<std::array<float, 16>>& poseWorld) {
    if (!enabled)
        return;
    float dt = std::min(deltaTime, kMaxTimestep);
    mWorld->stepSimulation(dt, kSubsteps, kFixedTimestep);
}

// Write physics simulation results back to bone world matrices.
// For each active dynamic body, compute how far it moved from its initial pose,
// then apply that same displacement to the linked bone's bind-world transform.
// This is the feedback path: physics → bone animation.
void PhysicsWorld::getBoneTransforms(std::vector<std::array<float, 16>>& out) const {
    for (const auto& bb : mBodies) {
        if (bb.boneIndex < 0 || bb.boneIndex >= (int)out.size())
            continue;
        if (!bb.body->isActive())
            continue;

        // Star-topology spheres (chest/buttocks): skip all bone feedback.
        // Their displacement is a constraint-solver compromise, not articulation.
        if (bb.skipBoneFeedback)
            continue;

        btTransform bodyCurr = bb.body->getCenterOfMassTransform();
        // boneMat = bodyCurr * invBodyInit * boneBind (matrix multiply, matches saba)
        btTransform boneMat = bodyCurr * bb.invBodyInit * bb.boneBindMat;
        btQuaternion boneNewRot = boneMat.getRotation();
        btVector3 boneNewPos = boneMat.getOrigin();

        float tx, ty, tz;
        if (bb.mode == 2) {
            // Keep animation position, only use physics rotation
            tx = out[bb.boneIndex][12];
            ty = out[bb.boneIndex][13];
            tz = out[bb.boneIndex][14];
        } else {
            tx = boneNewPos.x() + mCenter.x;
            ty = boneNewPos.y() + mMinY;
            tz = boneNewPos.z() + mCenter.z;
        }

        // Write as column-major 4x4 matrix
        auto& m = out[bb.boneIndex];
        const btQuaternion& r = boneNewRot;
        m[0] = 1 - 2 * (r.y() * r.y() + r.z() * r.z());
        m[4] = 2 * (r.x() * r.y() - r.z() * r.w());
        m[8] = 2 * (r.x() * r.z() + r.y() * r.w());
        m[12] = tx;
        m[1] = 2 * (r.x() * r.y() + r.z() * r.w());
        m[5] = 1 - 2 * (r.x() * r.x() + r.z() * r.z());
        m[9] = 2 * (r.y() * r.z() - r.x() * r.w());
        m[13] = ty;
        m[2] = 2 * (r.x() * r.z() - r.y() * r.w());
        m[6] = 2 * (r.y() * r.z() + r.x() * r.w());
        m[10] = 1 - 2 * (r.x() * r.x() + r.y() * r.y());
        m[14] = tz;
        m[3] = 0;
        m[7] = 0;
        m[11] = 0;
        m[15] = 1;
    }
}

// Comprehensive physics analysis dump for debugging skirt/cloth sagging.
// Called periodically (every N frames). Logs:
//   - Bodies with significant Y displacement (sorted by droop amount)
//   - Active vs inactive body counts
//   - Joint spring stiffness summary for bodies with large Y displacement
void PhysicsWorld::debugFullDump(int frameNum) const {
    // Threshold: Y displacement below this (in PMX model space) is "significant droop"
    // 0.03 PMX units ~ 3mm for a typical model — visually noticeable
    constexpr float kDroopWarnThreshold = -0.03f;
    constexpr float kDroopSevereThreshold = -0.1f;

    struct BodyInfo {
        int idx;
        const BulletBody* bb;
        float yNow;       // current Y in model space
        float yInit;      // init Y in model space
        float deltaY;     // current - init (negative = droop)
        float disp3D;     // 3D displacement magnitude
        bool active;
    };
    std::vector<BodyInfo> infos;
    int activeCount = 0, dynamicCount = 0;

    for (int i = 0; i < (int)mBodies.size(); ++i) {
        const auto& bb = mBodies[i];
        if (!bb.body)
            continue;
        if (bb.mode == 0)
            continue; // kinematic bodies don't sag
        dynamicCount++;

        btTransform t = bb.body->getCenterOfMassTransform();
        btVector3 p = t.getOrigin();
        float yNow = p.y() + mMinY;
        float yInit = bb.initPosY + mMinY;
        float deltaY = yNow - yInit;
        btVector3 init(bb.initPosX, bb.initPosY, bb.initPosZ);
        float disp3D = (p - init).length();
        bool active = bb.body->isActive();
        if (active)
            activeCount++;

        infos.push_back({i, &bb, yNow, yInit, deltaY, disp3D, active});
    }

    // Sort by |Y displacement| descending (largest droop first)
    std::sort(infos.begin(), infos.end(),
              [](const BodyInfo& a, const BodyInfo& b) { return a.deltaY < b.deltaY; });

    int warnCount = 0, severeCount = 0;
    for (const auto& info : infos) {
        if (info.deltaY < kDroopWarnThreshold)
            warnCount++;
        if (info.deltaY < kDroopSevereThreshold)
            severeCount++;
    }

    MMD_DEBUG("PHYS", "=== Frame %d Physics Analysis ===", frameNum);
    MMD_DEBUG("PHYS", "  Bodies: dynamic=%d active=%d sleeping=%d | droop>0.03=%d severe(>0.1)=%d",
              dynamicCount, activeCount, dynamicCount - activeCount, warnCount, severeCount);

    // Print top 25 displaced bodies (most negative deltaY = biggest droop)
    int showCount = std::min(25, (int)infos.size());
    MMD_DEBUG("PHYS", "  --- Top %d bodies by Y displacement ---", showCount);
    for (int i = 0; i < showCount; ++i) {
        const auto& info = infos[i];
        const char* modeStr =
            info.bb->mode == 2 ? "ALIGN" : (info.bb->mode == 1 ? "dyn" : "?");
        const char* flag = "";
        if (info.deltaY < kDroopSevereThreshold)
            flag = " **SEVERE**";
        else if (info.deltaY < kDroopWarnThreshold)
            flag = " *droop*";
        MMD_DEBUG("PHYS",
                  "  [%2d] BODY[%d] \"%s\" mode=%s cloth=%d skipFB=%d mass=%.3f "
                  "act=%d | Y=%.4f initY=%.4f dY=%+.4f disp3D=%.4f%s",
                  i, info.bb->rigidBodyIndex, info.bb->name.c_str(), modeStr, info.bb->clothLike,
                  info.bb->skipBoneFeedback, info.bb->body ? 1.0f / info.bb->body->getInvMass() : 0,
                  info.active, info.yNow, info.yInit, info.deltaY, info.disp3D, flag);
    }

    // Also show bodies that moved UP significantly (positive Y displacement)
    int upCount = 0;
    for (int i = (int)infos.size() - 1; i >= 0 && upCount < 5; --i) {
        if (infos[i].deltaY > 0.03f)
            upCount++;
    }
    if (upCount > 0) {
        MMD_DEBUG("PHYS", "  --- Bodies pushed UP (>0.03) ---");
        int shown = 0;
        for (int i = (int)infos.size() - 1; i >= 0 && shown < 10; --i) {
            if (infos[i].deltaY <= 0.03f)
                continue;
            const auto& info = infos[i];
            MMD_DEBUG("PHYS",
                      "  [%2d] BODY[%d] \"%s\" mode=%d act=%d dY=%+.4f",
                      info.idx, info.bb->rigidBodyIndex, info.bb->name.c_str(),
                      info.bb->mode, info.active, info.deltaY);
            shown++;
        }
    }
    MMD_DEBUG("PHYS", "=== End Frame %d ===", frameNum);
}

void PhysicsWorld::debugTrackCloth() const {
    static int fc = 0;
    if (fc % 60 == 0) {
        for (const auto& bb : mBodies) {
            if (!bb.clothLike || !bb.body)
                continue;
            btTransform t = bb.body->getCenterOfMassTransform();
            btVector3 p = t.getOrigin();
            btQuaternion r = t.getRotation();
            float Y = p.y() + mMinY;
            float initY = bb.initPosY + mMinY;
            float pitch = atan2f(2 * (r.w() * r.x() + r.y() * r.z()),
                                 1 - 2 * (r.x() * r.x() + r.y() * r.y()));
            MMD_DEBUG("ANIM", "F%4d [%d]%s Y=%.4f(init=%.4f dY=%+.4f) pitch=%.2f", fc,
                      bb.rigidBodyIndex, bb.name.c_str(), Y, initY, Y - initY, pitch);
        }
    }
    fc++;
}
