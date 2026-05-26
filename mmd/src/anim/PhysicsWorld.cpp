#include "anim/PhysicsWorld.h"
#include "anim/BoneSkinning.h"

#include <btBulletDynamicsCommon.h>
#include <BulletDynamics/ConstraintSolver/btGeneric6DofSpringConstraint.h>

#include <algorithm>
#include <cmath>
#include <iostream>

namespace {
    constexpr float kGravityY = -9.8f;
    constexpr int   kSolverIterations = 15;
    constexpr int   kSubsteps = 10;
    constexpr float kFixedTimestep = 1.0f / 240.0f;
    constexpr float kMaxTimestep = 1.0f / 30.0f;
    constexpr float kSleepLinearThreshold = 0.08f;
    constexpr float kSleepAngularThreshold = 0.02f;
    constexpr float kDeactivationTime = 0.5f;
}

PhysicsWorld::PhysicsWorld()
{
    mCollisionCfg = std::make_unique<btDefaultCollisionConfiguration>();
    mDispatcher = std::make_unique<btCollisionDispatcher>(mCollisionCfg.get());
    mBroadphase = std::make_unique<btDbvtBroadphase>();
    mSolver = std::make_unique<btSequentialImpulseConstraintSolver>();
    mWorld = std::make_unique<btDiscreteDynamicsWorld>(
        mDispatcher.get(), mBroadphase.get(), mSolver.get(), mCollisionCfg.get());
    mWorld->setGravity(btVector3(0, kGravityY, 0));
    mWorld->getSolverInfo().m_numIterations = kSolverIterations;
    mWorld->getSolverInfo().m_erp2 = 0.8f; // match Bullet 2.75 default for non-contact constraints
}

PhysicsWorld::~PhysicsWorld()
{
    for (auto& c : mConstraints)
        if (c) mWorld->removeConstraint(c.get());
    for (auto& b : mBodies)
        if (b.body) mWorld->removeRigidBody(b.body);
}

void PhysicsWorld::build(const PmxModel& model, float modelScale)
{
    // Compute bounds
    float minX = 1e9f, minY = 1e9f, minZ = 1e9f, maxX = -1e9f, maxY = -1e9f, maxZ = -1e9f;
    for (const auto& v : model.vertices) {
        minX = std::min(minX, v.position.x); maxX = std::max(maxX, v.position.x);
        minY = std::min(minY, v.position.y); maxY = std::max(maxY, v.position.y);
        minZ = std::min(minZ, v.position.z); maxZ = std::max(maxZ, v.position.z);
    }
    mCenter = {(minX + maxX) * 0.5f, (minY + maxY) * 0.5f, (minZ + maxZ) * 0.5f};
    mMinY = minY;
    mModelScale = modelScale;
    mWorld->setGravity(btVector3(0, kGravityY / modelScale, 0));

    mBoneBindWorld = BoneSkinning::computeBindWorldMatrices(model);

    for (auto& c : mConstraints) { if (c) mWorld->removeConstraint(c.get()); }
    mConstraints.clear();
    for (auto& b : mBodies) { if (b.body) mWorld->removeRigidBody(b.body); }
    mBodies.clear();
    mShapes.clear();

    int countMode[3] = {};
    for (const auto& rb : model.rigidbodies) {
        if (rb.mode >= 0 && rb.mode < 3) countMode[rb.mode]++;
    }
    std::cout << "Physics: " << model.rigidbodies.size()
              << " bodies (mode0/static=" << countMode[0] << " mode1/dyn=" << countMode[1]
              << " mode2/align=" << countMode[2] << "), "
              << model.joints.size() << " joints" << std::endl;

    for (const auto& rb : model.rigidbodies) addRigidBody(rb);

    int dynMassCount = 0;
    for (const auto& rb : model.rigidbodies) {
        if ((rb.mode == 1 || rb.mode == 2) && rb.mass > 0) dynMassCount++;
    }
    std::cout << "  Dynamic mass bodies: " << dynMassCount << std::endl;

    // --- Box bodies ---
    std::cout << "\nBox bodies (shape_type=1):" << std::endl;
    for (const auto& rb : model.rigidbodies) {
        if (rb.shape_type != RIGID_SHAPE_BOX) continue;
        const char* bn = (rb.bone_index >= 0 && rb.bone_index < model.boneCount())
            ? model.bones[rb.bone_index].name.c_str() : "-";
        printf("  [%d] %-28s bone=%-20s mode=%d mass=%.3f size=(%.4f,%.4f,%.4f)\n",
            rb.index, rb.name.c_str(), bn, rb.mode, rb.mass,
            rb.shape_size.x, rb.shape_size.y, rb.shape_size.z);
    }

    // --- Sphere bodies ---
    std::cout << "\nSphere bodies (shape_type=0):" << std::endl;
    for (const auto& rb : model.rigidbodies) {
        if (rb.shape_type != RIGID_SHAPE_SPHERE) continue;
        const char* bn = (rb.bone_index >= 0 && rb.bone_index < model.boneCount())
            ? model.bones[rb.bone_index].name.c_str() : "-";
        printf("  [%d] %-28s bone=%-20s mode=%d mass=%.3f size=(%.4f,%.4f,%.4f)\n",
            rb.index, rb.name.c_str(), bn, rb.mode, rb.mass,
            rb.shape_size.x, rb.shape_size.y, rb.shape_size.z);
    }

    // --- Capsule-shaped bodies ---
    std::cout << "\nCapsule bodies (shape_type=2):" << std::endl;
    for (const auto& rb : model.rigidbodies) {
        if (rb.shape_type != RIGID_SHAPE_CAPSULE) continue;
        const char* bn = (rb.bone_index >= 0 && rb.bone_index < model.boneCount())
            ? model.bones[rb.bone_index].name.c_str() : "-";
        printf("  [%d] %-28s bone=%-20s mode=%d mass=%.3f size=(%.4f,%.4f,%.4f)\n",
            rb.index, rb.name.c_str(), bn, rb.mode, rb.mass,
            rb.shape_size.x, rb.shape_size.y, rb.shape_size.z);
    }

    // --- Debug: bodies with exactly 1 joint (single-body chains) ---
    {
        std::vector<int> jointCount(model.rigidbodies.size(), 0);
        for (const auto& jt : model.joints) {
            if (jt.rigidbody_index_a >= 0 && jt.rigidbody_index_a < (int)model.rigidbodies.size())
                jointCount[jt.rigidbody_index_a]++;
            if (jt.rigidbody_index_b >= 0 && jt.rigidbody_index_b < (int)model.rigidbodies.size())
                jointCount[jt.rigidbody_index_b]++;
        }
        std::cout << "\n=== Single-joint bodies (degree=1) ===" << std::endl;
        std::cout << "idx name                         mode mass  bone                      joint limits(TL/TH) springs(kT/kR)" << std::endl;
        for (size_t i = 0; i < model.rigidbodies.size(); ++i) {
            if (jointCount[i] != 1) continue;
            const auto& rb = model.rigidbodies[i];
            if (rb.mode != 1) continue; // only dynamic bodies
            // Find the joint
            const PmxJoint* jt = nullptr;
            for (const auto& j : model.joints) {
                if (j.rigidbody_index_a == (int)i || j.rigidbody_index_b == (int)i) { jt = &j; break; }
            }
            if (!jt) continue;
            const char* boneName = (rb.bone_index >= 0 && rb.bone_index < model.boneCount())
                ? model.bones[rb.bone_index].name.c_str() : "-";
            printf("%3zu %-28s %4d %5.3f %-25s T=(%+.3f,%+.3f,%+.3f)/(%+.3f,%+.3f,%+.3f) kT=(%.0f,%.0f,%.0f) kR=(%.0f,%.0f,%.0f)\n",
                i, rb.name.c_str(), rb.mode, rb.mass, boneName,
                jt->translation_limit_min.x, jt->translation_limit_min.y, jt->translation_limit_min.z,
                jt->translation_limit_max.x, jt->translation_limit_max.y, jt->translation_limit_max.z,
                jt->spring_constant_translation.x, jt->spring_constant_translation.y, jt->spring_constant_translation.z,
                jt->spring_constant_rotation.x, jt->spring_constant_rotation.y, jt->spring_constant_rotation.z);
        }
        std::cout << "=== Single-joint bodies end ===" << std::endl;
    }

    for (const auto& jt : model.joints) addJoint(jt);

    // Mark cloth-like bodies: connected to joints with rotation springs + freedom
    for (const auto& jt : model.joints) {
        bool hasRotSpring = jt.spring_constant_rotation.x != 0
                         || jt.spring_constant_rotation.y != 0
                         || jt.spring_constant_rotation.z != 0;
        float rlRange = fabsf(jt.rotation_limit_max.x - jt.rotation_limit_min.x)
                      + fabsf(jt.rotation_limit_max.y - jt.rotation_limit_min.y)
                      + fabsf(jt.rotation_limit_max.z - jt.rotation_limit_min.z);
        if (!hasRotSpring || rlRange < 0.01f) continue;
        auto mark = [&](int idx) {
            if (idx >= 0 && idx < (int)mBodies.size() && mBodies[idx].mode == 1)
                mBodies[idx].clothLike = true;
        };
        mark(jt.rigidbody_index_a);
        mark(jt.rigidbody_index_b);
    }
    int clothCount = 0;
    std::cout << "  Cloth-like bodies:";
    for (const auto& b : mBodies) {
        if (b.clothLike) {
            clothCount++;
            std::cout << " [" << b.rigidBodyIndex << "]" << b.name;
        }
    }
    std::cout << " (" << clothCount << " total)" << std::endl;

    // Dump joints for hair-chain bodies (bones with 后发, 马尾, 侧发, 前发, chain in name)
    std::cout << "\n=== Hair-chain joint analysis ===" << std::endl;
    for (const auto& jt : model.joints) {
        if (jt.rigidbody_index_a < 0 || jt.rigidbody_index_b < 0) continue;
        if (jt.rigidbody_index_a >= (int)model.rigidbodies.size()) continue;
        if (jt.rigidbody_index_b >= (int)model.rigidbodies.size()) continue;
        const auto& ra = model.rigidbodies[jt.rigidbody_index_a];
        const auto& rb = model.rigidbodies[jt.rigidbody_index_b];
        // Check if either body's bone name contains hair-related keywords
        auto checkBone = [&](int rbIdx) -> bool {
            if (rbIdx < 0 || rbIdx >= (int)model.rigidbodies.size()) return false;
            int bi = model.rigidbodies[rbIdx].bone_index;
            if (bi < 0 || bi >= model.boneCount()) return false;
            const auto& nm = model.bones[bi].name;
            return nm.find("后发") != std::string::npos
                || nm.find("马尾") != std::string::npos
                || nm.find("侧发") != std::string::npos
                || nm.find("前发") != std::string::npos
                || nm.find("chain") != std::string::npos;
        };
        if (!checkBone(jt.rigidbody_index_a) && !checkBone(jt.rigidbody_index_b)) continue;
        const auto& ba = model.bones[ra.bone_index >= 0 ? ra.bone_index : 0];
        const auto& bb = model.bones[rb.bone_index >= 0 ? rb.bone_index : 0];
        printf("  JT[%d]: %s(%d)<->%s(%d) type=%d TL=(%.3f,%.3f) TH=(%.3f,%.3f) RL=(%.3f,%.3f) RH=(%.3f,%.3f) "
               "kLin=(%.0f,%.0f,%.0f) kRot=(%.0f,%.0f,%.0f)\n",
               jt.index,
               ba.name.c_str(), jt.rigidbody_index_a,
               bb.name.c_str(), jt.rigidbody_index_b,
               jt.joint_type,
               jt.translation_limit_min.x, jt.translation_limit_max.x,
               jt.translation_limit_min.y, jt.translation_limit_max.y,
               jt.rotation_limit_min.x, jt.rotation_limit_max.x,
               jt.rotation_limit_min.y, jt.rotation_limit_max.y,
               jt.spring_constant_translation.x, jt.spring_constant_translation.y, jt.spring_constant_translation.z,
               jt.spring_constant_rotation.x, jt.spring_constant_rotation.y, jt.spring_constant_rotation.z);
    }
    std::cout << "=== Hair joint analysis end ===" << std::endl;

    // Dump hair bone flags and rigid body links
    std::cout << "\n=== Hair bone analysis ===" << std::endl;
    std::cout << "bodyIdx bodyName                  boneIdx boneName                      mode mass afterPhys parentIdx" << std::endl;
    for (const auto& rb : model.rigidbodies) {
        if (rb.bone_index < 0 || rb.bone_index >= model.boneCount()) continue;
        const auto& bn = model.bones[rb.bone_index];
        // Filter: hair-related bones
        bool isHair = bn.name.find("后发") != std::string::npos
                   || bn.name.find("侧发") != std::string::npos
                   || bn.name.find("前发") != std::string::npos
                   || bn.name == "chain"
                   || bn.name == "chain root"
                   || bn.name == "halo"
                   || bn.name.find("small wing") != std::string::npos;
        if (!isHair) continue;
        printf("%7d %-25s %7d %-28s %4d %5.2f %9s %9d\n",
            rb.index, rb.name.c_str(),
            rb.bone_index, bn.name.c_str(),
            rb.mode, rb.mass,
            bn.hasFlag(BONEFLAG_IS_AFTER_PHYSICS_DEFORM) ? "AFTER" : "-",
            bn.parent_index);
    }
    std::cout << "=== Hair bone analysis end ===" << std::endl;

    // Check which small hair bodies have joints
    auto bodyHasJoint = [&](int rbIdx) {
        for (const auto& jt : model.joints) {
            if (jt.rigidbody_index_a == rbIdx || jt.rigidbody_index_b == rbIdx) return true;
        }
        return false;
    };
    std::cout << "\n=== Floating body check (no joints) ===" << std::endl;
    for (const auto& rb : model.rigidbodies) {
        if (rb.bone_index < 0 || rb.bone_index >= model.boneCount()) continue;
        const auto& bn = model.bones[rb.bone_index];
        bool isSmall = bn.name.find("后发") != std::string::npos
                    || bn.name.find("侧发") != std::string::npos
                    || bn.name.find("前发") != std::string::npos
                    || bn.name == "chain"
                    || bn.name == "halo"
                    || bn.name.find("small wing") != std::string::npos;
        if (!isSmall) continue;
        if (!bodyHasJoint(rb.index)) {
            printf("  NO JOINT: body[%d] %s mode=%d mass=%.2f bone=%s\n",
                rb.index, rb.name.c_str(), rb.mode, rb.mass, bn.name.c_str());
        }
    }
    std::cout << "=== Floating check end ===" << std::endl;
}

void PhysicsWorld::resetPhysics(const std::vector<std::array<float, 16>>& poseWorld)
{
    // Set all dynamic bodies to kinematic, align them to bone targets,
    // run one step, then switch back to dynamic. Prevents initial overlap explosion.
    for (auto& bb : mBodies) {
        if (bb.mode == 0 || bb.boneIndex < 0 || bb.boneIndex >= (int)poseWorld.size()) continue;
        if (bb.boneIndex >= (int)mBoneBindWorld.size()) continue;

        // Make kinematic and align to bone
        bb.body->setCollisionFlags(bb.body->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
        btVector3 tgtPos; btQuaternion tgtRot;
        computeBoneTarget(bb, poseWorld, tgtPos, tgtRot);
        btTransform cur; cur.setIdentity();
        cur.setOrigin(tgtPos); cur.setRotation(tgtRot);
        bb.body->getMotionState()->setWorldTransform(cur);
        bb.body->setCenterOfMassTransform(cur);
    }

    mWorld->stepSimulation(1.0f / 60.0f, 2, 1.0f / 120.0f);

    // Clear forces and switch back to dynamic
    for (auto& bb : mBodies) {
        if (bb.mode == 0) continue;
        bb.body->setCollisionFlags(bb.body->getCollisionFlags() & ~btCollisionObject::CF_KINEMATIC_OBJECT);
        bb.body->clearForces();
        bb.body->setLinearVelocity(btVector3(0, 0, 0));
        bb.body->setAngularVelocity(btVector3(0, 0, 0));
        // Sync motion state to current body transform
        bb.body->getMotionState()->setWorldTransform(bb.body->getCenterOfMassTransform());
    }

    std::cout << "Physics: reset complete, bodies aligned" << std::endl;
}

void PhysicsWorld::debugDump() const
{
    std::cout << "=== Physics dump (scale=" << mModelScale << " center=" << mCenter.x << "," << mCenter.y << "," << mCenter.z << ")" << std::endl;
    int moved = 0, active = 0;
    for (size_t i = 0; i < mBodies.size(); ++i) {
        const auto& bb = mBodies[i];
        if (!bb.body) continue;
        btTransform t = bb.body->getCenterOfMassTransform();
        btVector3 p = t.getOrigin();
        btVector3 init(bb.initPosX, bb.initPosY, bb.initPosZ);
        float disp = (p - init).length();
        if (bb.body->isActive()) active++;
        if (disp > 0.02f) {
            moved++;
            float mx = p.x() + mCenter.x, my = p.y() + mMinY, mz = p.z() + mCenter.z;
            float ix = init.x() + mCenter.x, iy = init.y() + mMinY, iz = init.z() + mCenter.z;
            const char* modeStr = bb.mode == 0 ? "STATIC" : (bb.mode == 1 ? "dyn" : "ALIGN");
            std::cout << "  [" << i << "] " << modeStr << " " << bb.name
                      << " bone=" << bb.boneIndex
                      << " dMMD=" << disp << " active=" << bb.body->isActive()
                      << " mass=" << (bb.body->getInvMass() > 0 ? 1.0f/bb.body->getInvMass() : 0)
                      << " now=(" << mx << "," << my << "," << mz << ")"
                      << " init=(" << ix << "," << iy << "," << iz << ")" << std::endl;
        }
    }
    std::cout << "  Bodies displaced>0.02mmd: " << moved << "  active: " << active << "  total: " << mBodies.size() << std::endl;
    std::cout << "=== End dump ===" << std::endl;
}

void PhysicsWorld::addRigidBody(const PmxRigidBody& rb)
{
    // PMX-native positions (offset from model center)
    float px = rb.shape_position.x - mCenter.x;
    float py = rb.shape_position.y - mMinY;
    float pz = rb.shape_position.z - mCenter.z;

    // Shapes at PMX-native scale (no modelScale — physics runs in PMX space)
    // PMX spec: sphere size.x=radius, capsule size.x=radius size.y=height — pass directly
    // Box size=(w,h,d) — Bullet btBoxShape expects half-extents, so *0.5f
    btCollisionShape* shape = nullptr;
    if (rb.shape_type == RIGID_SHAPE_SPHERE)
        shape = new btSphereShape(rb.shape_size.x);
    else if (rb.shape_type == RIGID_SHAPE_BOX)
        shape = new btBoxShape(btVector3(rb.shape_size.x*0.5f, rb.shape_size.y*0.5f, rb.shape_size.z*0.5f));
    else if (rb.shape_type == RIGID_SHAPE_CAPSULE) {
        float capR = rb.shape_size.x;
        float capH = rb.shape_size.y;
        float minR = 0.01f;
        if (capR < minR) { capR = minR; }
        shape = new btCapsuleShape(capR, capH);
    }
    else
        shape = new btSphereShape(rb.shape_size.x);
    mShapes.emplace_back(shape);

    // MMD rotation order: Y * X * Z (matches saba: ry * rx * rz)
    btQuaternion qx, qy, qz;
    qx.setRotation(btVector3(1,0,0), rb.shape_rotation.x);
    qy.setRotation(btVector3(0,1,0), rb.shape_rotation.y);
    qz.setRotation(btVector3(0,0,1), rb.shape_rotation.z);
    btQuaternion rot = qy * qx * qz;
    btTransform t; t.setIdentity(); t.setOrigin(btVector3(px, py, pz)); t.setRotation(rot);

    btScalar mass = (rb.mode == 0) ? 0 : rb.mass;
    btVector3 inertia(0,0,0);
    if (mass > 0) shape->calculateLocalInertia(mass, inertia);

    auto* ms = new btDefaultMotionState(t);
    btRigidBody::btRigidBodyConstructionInfo ci(mass, ms, shape, inertia);
    ci.m_restitution = rb.restitution; ci.m_friction = rb.friction;
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
        float ccdRadius = rb.shape_size.x; // sphere/capsule: PMX size.x is radius
        if (rb.shape_type == RIGID_SHAPE_BOX)
            ccdRadius = std::min({rb.shape_size.x, rb.shape_size.y, rb.shape_size.z}) * 0.25f;
        body->setCcdMotionThreshold(ccdRadius * 0.5f);
        body->setCcdSweptSphereRadius(ccdRadius);
    }

    mWorld->addRigidBody(body,
        1 << rb.collision_group,
        ~rb.no_collision_group & 0xFFFF);
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
        btQuaternion bwRot; bwBasis.getRotation(bwRot);
        brx = bwRot.x(); bry = bwRot.y(); brz = bwRot.z(); brw = bwRot.w();
    }

    mBodies.push_back({body, rb.bone_index, rb.index, rb.mode,
        initPos.x(), initPos.y(), initPos.z(),
        initRot.x(), initRot.y(), initRot.z(), initRot.w(),
        bpx, bpy, bpz, brx, bry, brz, brw,
        false, rb.name});
}

void PhysicsWorld::addJoint(const PmxJoint& jt)
{
    if (jt.rigidbody_index_a < 0 || jt.rigidbody_index_b < 0) return;
    if (jt.rigidbody_index_a >= (int)mBodies.size() || jt.rigidbody_index_b >= (int)mBodies.size()) return;
    auto *a = mBodies[jt.rigidbody_index_a].body, *b = mBodies[jt.rigidbody_index_b].body;
    if (!a || !b) return;

    btVector3 pos(jt.position.x - mCenter.x, jt.position.y - mMinY, jt.position.z - mCenter.z);

    btQuaternion jqx, jqy, jqz;
    jqx.setRotation(btVector3(1,0,0), jt.rotation.x);
    jqy.setRotation(btVector3(0,1,0), jt.rotation.y);
    jqz.setRotation(btVector3(0,0,1), jt.rotation.z);
    btQuaternion jtRot = jqy * jqx * jqz;
    btMatrix3x3 jtBasis; jtBasis.setRotation(jtRot);

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
    case 0: // 6DOF Spring — btGeneric6DofSpring2Constraint
    case 1: // 6DOF — same constraint, PMX springs disabled per spec
        break; // Handled below with the generic 6DOF path

    case 2: { // P2P — btPoint2PointConstraint, limits/springs ignored
        c = new btPoint2PointConstraint(*a, *b, invA.getOrigin(), invB.getOrigin());
        break;
    }
    case 3: { // ConeTwist — btConeTwistConstraint
        auto* ct = new btConeTwistConstraint(*a, *b, invA, invB);

        // Rotation limits map to swing/twist spans
        ct->setLimit(
            jt.rotation_limit_min.z,          // swingSpan1
            jt.rotation_limit_min.y,          // swingSpan2
            jt.rotation_limit_min.x,          // twistSpan
            jt.spring_constant_translation.x, // softness
            jt.spring_constant_translation.y, // biasFactor
            jt.spring_constant_translation.z  // relaxationFactor
        );

        // Damping / fixThresh from translation limit fields
        float damping = jt.translation_limit_min.x;
        float fixThresh = jt.translation_limit_max.x;
        if (damping == 0 && fixThresh == 0) { damping = 0.1f; fixThresh = 0.1f; }
        ct->setDamping(damping);
        ct->setFixThresh(fixThresh);

        // Motor: enabled when translation_limit_min.z != 0
        if (jt.translation_limit_min.z != 0) {
            ct->enableMotor(true);
            ct->setMaxMotorImpulse(jt.translation_limit_max.z);
            btQuaternion motorTarget;
            motorTarget.setEulerZYX(
                jt.spring_constant_rotation.z,
                jt.spring_constant_rotation.y,
                jt.spring_constant_rotation.x);
            ct->setMotorTargetInConstraintSpace(motorTarget);
        }
        c = ct;
        break;
    }
    case 4: { // Slider — btSliderConstraint (X-axis linear, X-axis angular)
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
    case 5: { // Hinge — btHingeConstraint (Z-axis rotation)
        auto* h = new btHingeConstraint(*a, *b, invA, invB);
        float softness    = jt.spring_constant_translation.x;
        float biasFactor  = jt.spring_constant_translation.y;
        float relaxFactor = jt.spring_constant_translation.z;
        if (softness == 0 && biasFactor == 0 && relaxFactor == 0) {
            softness = 0.9f; biasFactor = 0.3f; relaxFactor = 1.0f;
        }
        h->setLimit(jt.rotation_limit_min.x, jt.rotation_limit_max.x,
                    softness, biasFactor, relaxFactor);

        if (jt.spring_constant_rotation.x != 0) {
            h->enableAngularMotor(true, jt.spring_constant_rotation.y, jt.spring_constant_rotation.z);
        }
        c = h;
        break;
    }
    default: { // Unknown — fall back to 6DOF spring
        // Falls through to same logic as case 0 below
        break;
    }
    }

    // Types 0, 1, and default use btGeneric6DofSpringConstraint
    if (!c && (jt.joint_type == 0 || jt.joint_type == 1 || jt.joint_type < 0 || jt.joint_type > 5)) {
        auto* sc = new btGeneric6DofSpringConstraint(*a, *b, invA, invB, true);

        sc->setLinearLowerLimit(btVector3(
            jt.translation_limit_min.x, jt.translation_limit_min.y, jt.translation_limit_min.z));
        sc->setLinearUpperLimit(btVector3(
            jt.translation_limit_max.x, jt.translation_limit_max.y, jt.translation_limit_max.z));
        sc->setAngularLowerLimit(btVector3(jt.rotation_limit_min.x, jt.rotation_limit_min.y, jt.rotation_limit_min.z));
        sc->setAngularUpperLimit(btVector3(jt.rotation_limit_max.x, jt.rotation_limit_max.y, jt.rotation_limit_max.z));

        // PMX springs (type 0 and default only; type 1 skips per spec)
        if (jt.joint_type != 1) {
            const float* st = &jt.spring_constant_translation.x;
            const float* sr = &jt.spring_constant_rotation.x;
            if (st[0] != 0) { sc->enableSpring(0, true); sc->setStiffness(0, st[0]); }
            if (st[1] != 0) { sc->enableSpring(1, true); sc->setStiffness(1, st[1]); }
            if (st[2] != 0) { sc->enableSpring(2, true); sc->setStiffness(2, st[2]); }
            if (sr[0] != 0) { sc->enableSpring(3, true); sc->setStiffness(3, sr[0]); }
            if (sr[1] != 0) { sc->enableSpring(4, true); sc->setStiffness(4, sr[1]); }
            if (sr[2] != 0) { sc->enableSpring(5, true); sc->setStiffness(5, sr[2]); }
        }

        // Tiered spring fallback for tight translation DOFs (applies to both type 0 and 1)
        float lo[3] = {jt.translation_limit_min.x, jt.translation_limit_min.y, jt.translation_limit_min.z};
        float hi[3] = {jt.translation_limit_max.x, jt.translation_limit_max.y, jt.translation_limit_max.z};
        const float* st = &jt.spring_constant_translation.x;
        for (int i = 0; i < 3; ++i) {
            float range = fabsf(hi[i] - lo[i]);
            if (jt.joint_type == 0 && st[i] != 0) continue;
            float k = 0;
            if (range < 0.001f)      k = 10000.0f;
            else if (range < 0.2f)   k = 2000.0f;
            else if (range < 0.5f)   k = 500.0f;
            if (k > 0) {
                sc->enableSpring(i, true);
                sc->setStiffness(i, k);
                sc->setDamping(i, 0.02f);
            }
        }

        sc->setEquilibriumPoint();
        c = sc;
    }

    if (!c) return;
    mWorld->addConstraint(c, true);
    mConstraints.emplace_back(c);
}

void PhysicsWorld::computeBoneTarget(const BulletBody& bb,
    const std::vector<std::array<float, 16>>& poseWorld,
    btVector3& outPos, btQuaternion& outRot) const
{
    const auto& pw = poseWorld[bb.boneIndex];
    const auto& bw = mBoneBindWorld[bb.boneIndex];

    // Body init position in MMD world space
    float bodyModelX = bb.initPosX + mCenter.x;
    float bodyModelY = bb.initPosY + mMinY;
    float bodyModelZ = bb.initPosZ + mCenter.z;

    float bbx = bw[12], bby = bw[13], bbz = bw[14];
    btMatrix3x3 bwBasis(bw[0], bw[4], bw[8],  bw[1], bw[5], bw[9],  bw[2], bw[6], bw[10]);
    btQuaternion bwRot; bwBasis.getRotation(bwRot);

    float bax = pw[12], bay = pw[13], baz = pw[14];
    btMatrix3x3 pwBasis(pw[0], pw[4], pw[8],  pw[1], pw[5], pw[9],  pw[2], pw[6], pw[10]);
    btQuaternion pwRot; pwBasis.getRotation(pwRot);

    btVector3 offset(bodyModelX - bbx, bodyModelY - bby, bodyModelZ - bbz);
    btQuaternion deltaRot = pwRot * bwRot.inverse();
    btVector3 rotatedOffset = btMatrix3x3(deltaRot) * offset;

    float tgtX = bax + rotatedOffset.x();
    float tgtY = bay + rotatedOffset.y();
    float tgtZ = baz + rotatedOffset.z();
    btQuaternion bodyInitRot(bb.initRotX, bb.initRotY, bb.initRotZ, bb.initRotW);

    // Output in PMX-native space (offset from center)
    outPos = btVector3(tgtX - mCenter.x, tgtY - mMinY, tgtZ - mCenter.z);
    outRot = deltaRot * bodyInitRot;
}

void PhysicsWorld::updateMode0Bodies(const std::vector<std::array<float, 16>>& poseWorld)
{
    for (auto& bb : mBodies) {
        if (bb.mode != 0 || bb.boneIndex < 0 || bb.boneIndex >= (int)poseWorld.size()) continue;
        if (bb.boneIndex >= (int)mBoneBindWorld.size()) continue;

        btVector3 newPos;
        btQuaternion newRot;
        computeBoneTarget(bb, poseWorld, newPos, newRot);

        btTransform cur; cur.setIdentity();
        cur.setOrigin(newPos);
        cur.setRotation(newRot);
        bb.body->getMotionState()->setWorldTransform(cur);
        bb.body->setCenterOfMassTransform(cur);
    }
}

void PhysicsWorld::step(float deltaTime, const std::vector<std::array<float, 16>>& poseWorld)
{
    if (!enabled) return;

    // Mode 2: pull toward bone-animated transform
    // Scale forces by deltaTime so behavior is frame-rate independent
    float dtScale = deltaTime * 60.0f; // = 1.0 at 60fps
    for (auto& bb : mBodies) {
        if (bb.mode != 2 || bb.boneIndex < 0 || bb.boneIndex >= (int)poseWorld.size()) continue;
        if (bb.boneIndex >= (int)mBoneBindWorld.size()) continue;

        btVector3 tgtPos;
        btQuaternion tgtRot;
        computeBoneTarget(bb, poseWorld, tgtPos, tgtRot);

        btTransform cur = bb.body->getCenterOfMassTransform();
        btVector3 posErr = tgtPos - cur.getOrigin();
        float errLen = posErr.length();
        if (errLen > 0.005f) {
            bb.body->activate(true);
            bb.body->applyCentralForce((posErr * 50.0f - bb.body->getLinearVelocity() * 15.0f) * dtScale);
        }
        btQuaternion curRot = cur.getRotation();
        btQuaternion diff = curRot.inverse() * tgtRot;
        if (diff.w() < 0) diff = btQuaternion(-diff.x(), -diff.y(), -diff.z(), -diff.w());
        float axLen = sqrtf(diff.x()*diff.x() + diff.y()*diff.y() + diff.z()*diff.z());
        if (axLen > 0.001f) {
            bb.body->activate(true);
            float angle = 2.0f * atan2f(axLen, diff.w());
            btVector3 axis(diff.x()/axLen, diff.y()/axLen, diff.z()/axLen);
            bb.body->setAngularVelocity(bb.body->getAngularVelocity() * powf(0.85f, dtScale) + axis * angle * 10.0f * dtScale);
        }
    }

    mWorld->stepSimulation(std::min(deltaTime, kMaxTimestep), kSubsteps, kFixedTimestep);

    debugTrackCloth();
}

void PhysicsWorld::getBoneTransforms(std::vector<std::array<float, 16>>& out) const
{
    for (const auto& bb : mBodies) {
        if (bb.boneIndex < 0 || bb.boneIndex >= (int)out.size()) continue;
        if (!bb.body->isActive()) continue;
        btTransform t = bb.body->getCenterOfMassTransform();
        btVector3 bodyPos = t.getOrigin();
        btQuaternion bodyRot = t.getRotation();

        // Body displacement from its initial pose
        btQuaternion bodyInitRot(bb.initRotX, bb.initRotY, bb.initRotZ, bb.initRotW);
        btQuaternion bodyDeltaRot = bodyRot * bodyInitRot.inverse();
        btVector3 bodyInitPos(bb.initPosX, bb.initPosY, bb.initPosZ);
        btVector3 disp = bodyPos - bodyInitPos;

        // Apply displacement to bone's bind-world position
        btVector3 boneInitPos(bb.bonePosX, bb.bonePosY, bb.bonePosZ);
        btQuaternion boneInitRot(bb.boneRotX, bb.boneRotY, bb.boneRotZ, bb.boneRotW);
        btVector3 boneNewPos = boneInitPos + disp;
        btQuaternion boneNewRot = bodyDeltaRot * boneInitRot;

        float tx = boneNewPos.x() + mCenter.x;
        float ty = boneNewPos.y() + mMinY;
        float tz = boneNewPos.z() + mCenter.z;
        auto& m = out[bb.boneIndex];
        const btQuaternion& r = boneNewRot;
        m[0]=1-2*(r.y()*r.y()+r.z()*r.z()); m[4]=2*(r.x()*r.y()-r.z()*r.w()); m[8]=2*(r.x()*r.z()+r.y()*r.w()); m[12]=tx;
        m[1]=2*(r.x()*r.y()+r.z()*r.w());  m[5]=1-2*(r.x()*r.x()+r.z()*r.z()); m[9]=2*(r.y()*r.z()-r.x()*r.w()); m[13]=ty;
        m[2]=2*(r.x()*r.z()-r.y()*r.w());  m[6]=2*(r.y()*r.z()+r.x()*r.w());  m[10]=1-2*(r.x()*r.x()+r.y()*r.y()); m[14]=tz;
        m[3]=0; m[7]=0; m[11]=0; m[15]=1;
    }
}

void PhysicsWorld::debugTrackCloth() const
{
    static int fc = 0;
    if (fc % 60 == 0) {
        for (const auto& bb : mBodies) {
            if (!bb.clothLike || !bb.body) continue;
            btTransform t = bb.body->getCenterOfMassTransform();
            btVector3 p = t.getOrigin(); btQuaternion r = t.getRotation();
            float Y = p.y() + mMinY;
            float initY = bb.initPosY + mMinY;
            float pitch = atan2f(2*(r.w()*r.x()+r.y()*r.z()), 1-2*(r.x()*r.x()+r.y()*r.y()));
            printf("F%4d [%d]%s Y=%.4f(init=%.4f dY=%+.4f) pitch=%.2f\n",
                fc, bb.rigidBodyIndex, bb.name.c_str(), Y, initY, Y-initY, pitch);
        }
    }
    fc++;
}
