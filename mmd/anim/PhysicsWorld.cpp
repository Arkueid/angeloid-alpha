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
// Substep count per stepSimulation call — trades performance for constraint accuracy
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
    // Bullet 3.x changed the default to 0.2; we restore 0.8 matching saba and Bullet 2.75
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

    for (const auto& rb : model.rigidbodies)
        addRigidBody(rb);

    int dynMassCount = 0;
    for (const auto& rb : model.rigidbodies) {
        if ((rb.mode == 1 || rb.mode == 2) && rb.mass > 0)
            dynMassCount++;
    }
    MMD_INFO("PHYS", "  Dynamic mass bodies: %d", dynMassCount);

    for (const auto& jt : model.joints)
        addJoint(jt);

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
    // Set all dynamic bodies to kinematic, align them to bone targets,
    // run one step, then switch back to dynamic. Prevents initial overlap explosion.
    for (auto& bb : mBodies) {
        if (bb.mode == 0 || bb.boneIndex < 0 || bb.boneIndex >= (int)poseWorld.size())
            continue;
        if (bb.boneIndex >= (int)mBoneBindWorld.size())
            continue;

        // Make kinematic and align to bone
        bb.body->setCollisionFlags(bb.body->getCollisionFlags() |
                                   btCollisionObject::CF_KINEMATIC_OBJECT);
        btVector3 tgtPos;
        btQuaternion tgtRot;
        computeBoneTarget(bb, poseWorld, tgtPos, tgtRot);
        btTransform cur;
        cur.setIdentity();
        cur.setOrigin(tgtPos);
        cur.setRotation(tgtRot);
        bb.body->getMotionState()->setWorldTransform(cur);
        bb.body->setCenterOfMassTransform(cur);
    }

    mWorld->stepSimulation(1.0f / 60.0f, 2, 1.0f / 120.0f);

    // Clear forces and switch back to dynamic
    for (auto& bb : mBodies) {
        if (bb.mode == 0)
            continue;
        bb.body->setCollisionFlags(bb.body->getCollisionFlags() &
                                   ~btCollisionObject::CF_KINEMATIC_OBJECT);
        bb.body->clearForces();
        bb.body->setLinearVelocity(btVector3(0, 0, 0));
        bb.body->setAngularVelocity(btVector3(0, 0, 0));
        // Sync motion state to current body transform
        bb.body->getMotionState()->setWorldTransform(bb.body->getCenterOfMassTransform());
    }

    MMD_INFO("PHYS", "reset complete, bodies aligned");
}

void PhysicsWorld::debugDump() const {
    MMD_INFO("PHYS", "=== Physics dump (scale=%.4f center=%.2f,%.2f,%.2f) ===", mModelScale,
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
            MMD_INFO("PHYS",
                     "  [%zu] %s %s bone=%d dMMD=%.4f active=%d mass=%.3f now=(%.2f,%.2f,%.2f) "
                     "init=(%.2f,%.2f,%.2f)",
                     i, modeStr, bb.name.c_str(), bb.boneIndex, disp, bb.body->isActive() ? 1 : 0,
                     mass, mx, my, mz, ix, iy, iz);
        }
    }
    MMD_INFO("PHYS", "  Bodies displaced>0.02mmd: %d  active: %d  total: %zu", moved, active,
             mBodies.size());
    MMD_INFO("PHYS", "=== End dump ===");
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

    // Bone bind world position in PMX-native space (offset from center)
    float bpx = 0, bpy = 0, bpz = 0, brx = 0, bry = 0, brz = 0, brw = 1;
    if (rb.bone_index >= 0 && rb.bone_index < (int)mBoneBindWorld.size()) {
        const auto& bw = mBoneBindWorld[rb.bone_index];
        bpx = bw[12] - mCenter.x;
        bpy = bw[13] - mMinY;
        bpz = bw[14] - mCenter.z;
        btMatrix3x3 bwBasis(bw[0], bw[4], bw[8], bw[1], bw[5], bw[9], bw[2], bw[6], bw[10]);
        btQuaternion bwRot;
        bwBasis.getRotation(bwRot);
        brx = bwRot.x();
        bry = bwRot.y();
        brz = bwRot.z();
        brw = bwRot.w();
    }

    mBodies.push_back({body,        rb.bone_index, rb.index,    rb.mode,     initPos.x(),
                       initPos.y(), initPos.z(),   initRot.x(), initRot.y(), initRot.z(),
                       initRot.w(), bpx,           bpy,         bpz,         brx,
                       bry,         brz,           brw,         false,       rb.name});
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
        // prevent drift. Type 0 joints with existing springs skip this — the PMX spring
        // already provides the restoring force. Three tiers:
        //   near-locked: range < 0.001 m → k=10000 (essentially rigid)
        //   tight:       range < 0.2   m → k=2000
        //   narrow:      range < 0.5   m → k=500
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

// Compute the target position/rotation a rigid body should follow based on its linked
// bone's current animation pose. Uses a "delta tracking" approach:
//   1. Record the body's initial offset from the bone's bind-pose position.
//   2. Compute the bone's rotation delta from bind → current pose.
//   3. Apply that same delta rotation to the body's offset.
//   4. Add the result to the bone's current pose position.
// This way, when a bone rotates, its attached rigid body orbits around it correctly.
void PhysicsWorld::computeBoneTarget(const BulletBody& bb,
                                     const std::vector<std::array<float, 16>>& poseWorld,
                                     btVector3& outPos, btQuaternion& outRot) const {
    // Pose world = current frame bone transform
    const auto& pw = poseWorld[bb.boneIndex];
    // Bind world = initial bone transform from PMX (pre-animation)
    const auto& bw = mBoneBindWorld[bb.boneIndex];

    // Body initial position in model space (un-center for absolute coordinates)
    float bodyModelX = bb.initPosX + mCenter.x;
    float bodyModelY = bb.initPosY + mMinY;
    float bodyModelZ = bb.initPosZ + mCenter.z;

    // Bone bind-pose position and rotation
    float bbx = bw[12], bby = bw[13], bbz = bw[14];
    btMatrix3x3 bwBasis(bw[0], bw[4], bw[8], bw[1], bw[5], bw[9], bw[2], bw[6], bw[10]);
    btQuaternion bwRot;
    bwBasis.getRotation(bwRot);

    // Bone current-pose position and rotation
    float bax = pw[12], bay = pw[13], baz = pw[14];
    btMatrix3x3 pwBasis(pw[0], pw[4], pw[8], pw[1], pw[5], pw[9], pw[2], pw[6], pw[10]);
    btQuaternion pwRot;
    pwBasis.getRotation(pwRot);

    // Body's offset from bone in bind pose, then rotated by bone's animation delta
    btVector3 offset(bodyModelX - bbx, bodyModelY - bby, bodyModelZ - bbz);
    btQuaternion deltaRot = pwRot * bwRot.inverse();
    btVector3 rotatedOffset = btMatrix3x3(deltaRot) * offset;

    // Target = bone current position + rotated body offset
    float tgtX = bax + rotatedOffset.x();
    float tgtY = bay + rotatedOffset.y();
    float tgtZ = baz + rotatedOffset.z();
    btQuaternion bodyInitRot(bb.initRotX, bb.initRotY, bb.initRotZ, bb.initRotW);

    // Output in PMX-native space (center-relative, matching Bullet body positions)
    outPos = btVector3(tgtX - mCenter.x, tgtY - mMinY, tgtZ - mCenter.z);
    // Target rotation = bone's animation delta composed with body's initial rotation
    outRot = deltaRot * bodyInitRot;
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

    // Mode 2 (bone-align): apply corrective forces pulling the body toward the
    // bone-animated target transform, acting as a soft constraint rather than
    // hard teleportation. This lets physics (collisions, other joints) still
    // influence the body while keeping it roughly aligned with the bone.
    //
    // dtScale normalizes forces for frame-rate independence: at 60fps it's 1.0,
    // at 30fps it's 0.5 (half the force per step, twice as many steps).
    float dtScale = deltaTime * 60.0f;
    for (auto& bb : mBodies) {
        if (bb.mode != 2 || bb.boneIndex < 0 || bb.boneIndex >= (int)poseWorld.size())
            continue;
        if (bb.boneIndex >= (int)mBoneBindWorld.size())
            continue;

        btVector3 tgtPos;
        btQuaternion tgtRot;
        computeBoneTarget(bb, poseWorld, tgtPos, tgtRot);

        // Position correction: PD controller (proportional + derivative)
        //   force = (posError * kP - velocity * kD) * dtScale
        // kP=50 provides strong pull; kD=15 adds damping to prevent oscillation
        btTransform cur = bb.body->getCenterOfMassTransform();
        btVector3 posErr = tgtPos - cur.getOrigin();
        float errLen = posErr.length();
        if (errLen > 0.005f) {
            bb.body->activate(true);
            bb.body->applyCentralForce((posErr * 50.0f - bb.body->getLinearVelocity() * 15.0f) *
                                       dtScale);
        }
        // Rotation correction: compute quaternion difference, convert to axis-angle,
        // then set angular velocity toward the target. The 0.85 damping factor prevents
        // overshoot and oscillation in the rotation correction.
        btQuaternion curRot = cur.getRotation();
        btQuaternion diff = curRot.inverse() * tgtRot;
        if (diff.w() < 0)
            diff = btQuaternion(-diff.x(), -diff.y(), -diff.z(), -diff.w());
        float axLen = sqrtf(diff.x() * diff.x() + diff.y() * diff.y() + diff.z() * diff.z());
        if (axLen > 0.001f) {
            bb.body->activate(true);
            float angle = 2.0f * atan2f(axLen, diff.w());
            btVector3 axis(diff.x() / axLen, diff.y() / axLen, diff.z() / axLen);
            bb.body->setAngularVelocity(bb.body->getAngularVelocity() * powf(0.85f, dtScale) +
                                        axis * angle * 10.0f * dtScale);
        }
    }

    // Cap deltaTime to prevent physics explosion on frame spikes
    mWorld->stepSimulation(std::min(deltaTime, kMaxTimestep), kSubsteps, kFixedTimestep);

    // debugTrackCloth();
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
        btTransform t = bb.body->getCenterOfMassTransform();
        btVector3 bodyPos = t.getOrigin();
        btQuaternion bodyRot = t.getRotation();

        // Body's delta from its initial configuration
        btQuaternion bodyInitRot(bb.initRotX, bb.initRotY, bb.initRotZ, bb.initRotW);
        btQuaternion bodyDeltaRot = bodyRot * bodyInitRot.inverse();
        btVector3 bodyInitPos(bb.initPosX, bb.initPosY, bb.initPosZ);
        btVector3 disp = bodyPos - bodyInitPos;

        // Apply the same displacement to the bone's bind-world position/rotation
        btVector3 boneInitPos(bb.bonePosX, bb.bonePosY, bb.bonePosZ);
        btQuaternion boneInitRot(bb.boneRotX, bb.boneRotY, bb.boneRotZ, bb.boneRotW);
        btVector3 boneNewPos = boneInitPos + disp;
        btQuaternion boneNewRot = bodyDeltaRot * boneInitRot;

        // Convert back to model space and write as column-major 4x4 matrix
        float tx = boneNewPos.x() + mCenter.x;
        float ty = boneNewPos.y() + mMinY;
        float tz = boneNewPos.z() + mCenter.z;
        auto& m = out[bb.boneIndex];
        const btQuaternion& r = boneNewRot;
        // Quaternion to rotation matrix (column-major)
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
