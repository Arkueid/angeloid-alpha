#include "anim/PhysicsWorld.h"
#include "anim/BoneSkinning.h"

#include <btBulletDynamicsCommon.h>

#include <algorithm>
#include <cmath>
#include <iostream>

PhysicsWorld::PhysicsWorld()
{
    mCollisionCfg = std::make_unique<btDefaultCollisionConfiguration>();
    mDispatcher = std::make_unique<btCollisionDispatcher>(mCollisionCfg.get());
    mBroadphase = std::make_unique<btDbvtBroadphase>();
    mSolver = std::make_unique<btSequentialImpulseConstraintSolver>();
    mWorld = std::make_unique<btDiscreteDynamicsWorld>(
        mDispatcher.get(), mBroadphase.get(), mSolver.get(), mCollisionCfg.get());
    mWorld->setGravity(btVector3(0, -9.8f, 0));
    mWorld->getSolverInfo().m_numIterations = 10;
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

    for (const auto& jt : model.joints) addJoint(jt);
}

void PhysicsWorld::debugDump() const
{
    float s = mModelScale;
    std::cout << "=== Physics dump (scale=" << s << " center=" << mCenter.x << "," << mCenter.y << "," << mCenter.z << ")" << std::endl;
    int moved = 0, active = 0;
    for (size_t i = 0; i < mBodies.size(); ++i) {
        const auto& bb = mBodies[i];
        if (!bb.body) continue;
        btTransform t = bb.body->getCenterOfMassTransform();
        btVector3 p = t.getOrigin();
        btVector3 init(bb.initPosX, bb.initPosY, bb.initPosZ);
        float disp = (p - init).length();
        float dispMMD = disp / s;
        if (bb.body->isActive()) active++;
        if (dispMMD > 0.02f) {
            moved++;
            // Convert from Bullet space back to MMD space
            float mx = p.x()/s + mCenter.x, my = p.y()/s + mMinY, mz = p.z()/s + mCenter.z;
            float ix = init.x()/s + mCenter.x, iy = init.y()/s + mMinY, iz = init.z()/s + mCenter.z;
            const char* modeStr = bb.mode == 0 ? "STATIC" : (bb.mode == 1 ? "dyn" : "ALIGN");
            std::cout << "  [" << i << "] " << modeStr << " " << bb.name
                      << " bone=" << bb.boneIndex
                      << " dMMD=" << dispMMD << " active=" << bb.body->isActive()
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
    float s = mModelScale;
    float px = (rb.shape_position.x - mCenter.x) * s;
    float py = (rb.shape_position.y - mMinY) * s;
    float pz = (rb.shape_position.z - mCenter.z) * s;

    btCollisionShape* shape = nullptr;
    if (rb.shape_type == RIGID_SHAPE_SPHERE)
        shape = new btSphereShape(rb.shape_size.x * 0.5f * s);
    else if (rb.shape_type == RIGID_SHAPE_BOX)
        shape = new btBoxShape(btVector3(rb.shape_size.x*0.5f*s, rb.shape_size.y*0.5f*s, rb.shape_size.z*0.5f*s));
    else if (rb.shape_type == RIGID_SHAPE_CAPSULE)
        shape = new btCapsuleShape(rb.shape_size.x * 0.5f * s, rb.shape_size.y * 0.5f * s);
    else
        shape = new btSphereShape(rb.shape_size.x * 0.5f * s);
    mShapes.emplace_back(shape);

    btQuaternion rot; rot.setEulerZYX(rb.shape_rotation.z, rb.shape_rotation.y, rb.shape_rotation.x);
    btTransform t; t.setIdentity(); t.setOrigin(btVector3(px, py, pz)); t.setRotation(rot);

    btScalar mass = (rb.mode == 0) ? 0 : rb.mass; // mode0=static(bone-follow), mode1/2=dynamic
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
    body->setSleepingThresholds(0.08f, 0.02f);
    body->setDeactivationTime(0.5f);
    body->setDamping(ci.m_linearDamping, ci.m_angularDamping);
    mWorld->addRigidBody(body,
        rb.collision_group != 0 ? (1 << rb.collision_group) : 1,
        rb.no_collision_group);
    btQuaternion initRot = t.getRotation();
    btVector3 initPos = t.getOrigin();
    mBodies.push_back({body, rb.bone_index, rb.index, rb.mode,
        initPos.x(), initPos.y(), initPos.z(),
        initRot.x(), initRot.y(), initRot.z(), initRot.w(),
        rb.name});
}

void PhysicsWorld::addJoint(const PmxJoint& jt)
{
    if (jt.rigidbody_index_a < 0 || jt.rigidbody_index_b < 0) return;
    if (jt.rigidbody_index_a >= (int)mBodies.size() || jt.rigidbody_index_b >= (int)mBodies.size()) return;
    auto *a = mBodies[jt.rigidbody_index_a].body, *b = mBodies[jt.rigidbody_index_b].body;
    if (!a || !b) return;

    float s = mModelScale;
    btVector3 pos((jt.position.x - mCenter.x)*s, (jt.position.y - mMinY)*s, (jt.position.z - mCenter.z)*s);

    btQuaternion jtRot; jtRot.setEulerZYX(jt.rotation.z, jt.rotation.y, jt.rotation.x);
    btMatrix3x3 jtBasis; jtBasis.setRotation(jtRot);

    btTransform jointTransform;
    jointTransform.setIdentity();
    jointTransform.setOrigin(pos);
    jointTransform.setBasis(jtBasis);

    btTransform invA = a->getCenterOfMassTransform().inverse();
    btTransform invB = b->getCenterOfMassTransform().inverse();
    invA = invA * jointTransform;
    invB = invB * jointTransform;

    auto* c = new btGeneric6DofSpring2Constraint(*a, *b, invA, invB);

    c->setLinearLowerLimit(btVector3(
        jt.translation_limit_min.x * s, jt.translation_limit_min.y * s, jt.translation_limit_min.z * s));
    c->setLinearUpperLimit(btVector3(
        jt.translation_limit_max.x * s, jt.translation_limit_max.y * s, jt.translation_limit_max.z * s));
    c->setAngularLowerLimit(btVector3(jt.rotation_limit_min.x, jt.rotation_limit_min.y, jt.rotation_limit_min.z));
    c->setAngularUpperLimit(btVector3(jt.rotation_limit_max.x, jt.rotation_limit_max.y, jt.rotation_limit_max.z));

    const float* st = &jt.spring_constant_translation.x;
    const float* sr = &jt.spring_constant_rotation.x;
    // PMX springs as-is
    if (st[0] != 0) { c->enableSpring(0, true); c->setStiffness(0, st[0]); }
    if (st[1] != 0) { c->enableSpring(1, true); c->setStiffness(1, st[1]); }
    if (st[2] != 0) { c->enableSpring(2, true); c->setStiffness(2, st[2]); }
    if (sr[0] != 0) { c->enableSpring(3, true); c->setStiffness(3, sr[0]); }
    if (sr[1] != 0) { c->enableSpring(4, true); c->setStiffness(4, sr[1]); }
    if (sr[2] != 0) { c->enableSpring(5, true); c->setStiffness(5, sr[2]); }
    // Springs for translation DOFs with small/tight limits, to hold shape
    float lo[3] = {jt.translation_limit_min.x, jt.translation_limit_min.y, jt.translation_limit_min.z};
    float hi[3] = {jt.translation_limit_max.x, jt.translation_limit_max.y, jt.translation_limit_max.z};
    for (int i = 0; i < 3; ++i) {
        float range = fabsf(hi[i] - lo[i]);
        if (st[i] != 0) continue; // PMX spring already set
        float k = 0;
        if (range < 0.001f)      k = 2000.0f; // locked: very stiff
        else if (range < 0.2f)   k = 500.0f;  // tight: moderate
        else if (range < 0.5f)   k = 100.0f;  // narrow: gentle
        if (k > 0) {
            c->enableSpring(i, true);
            c->setStiffness(i, k);
            c->setDamping(i, 2.0f * sqrtf(k));
        }
    }

    mWorld->addConstraint(c, true);
    mConstraints.emplace_back(c);

}

void PhysicsWorld::updateMode0Bodies(const std::vector<std::array<float, 16>>& poseWorld)
{
    float s = mModelScale;
    for (auto& bb : mBodies) {
        if (bb.mode != 0 || bb.boneIndex < 0 || bb.boneIndex >= (int)poseWorld.size()) continue;
        if (bb.boneIndex >= (int)mBoneBindWorld.size()) continue;

        const auto& pw = poseWorld[bb.boneIndex];    // bone animated world
        const auto& bw = mBoneBindWorld[bb.boneIndex]; // bone bind world

        // Body's bind-pose model-space position
        float bodyModelX = bb.initPosX / s + mCenter.x;
        float bodyModelY = bb.initPosY / s + mMinY;
        float bodyModelZ = bb.initPosZ / s + mCenter.z;

        // Bone bind-pose world position and rotation (from 4x4 column-major)
        float bbx = bw[12], bby = bw[13], bbz = bw[14];
        // btMatrix3x3 takes row-major; column-major storage needs transpose
        btMatrix3x3 bwBasis(bw[0], bw[4], bw[8],  bw[1], bw[5], bw[9],  bw[2], bw[6], bw[10]);
        btQuaternion bwRot; bwBasis.getRotation(bwRot);

        // Bone animated world position and rotation
        float bax = pw[12], bay = pw[13], baz = pw[14];
        btMatrix3x3 pwBasis(pw[0], pw[4], pw[8],  pw[1], pw[5], pw[9],  pw[2], pw[6], pw[10]);
        btQuaternion pwRot; pwBasis.getRotation(pwRot);

        // Body offset from bone in world space, rotated by bone animation delta
        btVector3 offset(bodyModelX - bbx, bodyModelY - bby, bodyModelZ - bbz);
        btQuaternion deltaRot = pwRot * bwRot.inverse();
        btVector3 rotatedOffset = btMatrix3x3(deltaRot) * offset;

        float newX = bax + rotatedOffset.x();
        float newY = bay + rotatedOffset.y();
        float newZ = baz + rotatedOffset.z();

        // Also rotate body: body_anim_rot = delta_rot * body_init_rot
        btQuaternion bodyInitRot(bb.initRotX, bb.initRotY, bb.initRotZ, bb.initRotW);
        btQuaternion bodyNewRot = deltaRot * bodyInitRot;

        btVector3 newPos((newX - mCenter.x) * s, (newY - mMinY) * s, (newZ - mCenter.z) * s);
        btTransform cur; cur.setIdentity();
        cur.setOrigin(newPos);
        cur.setRotation(bodyNewRot);
        bb.body->getMotionState()->setWorldTransform(cur);
        bb.body->setCenterOfMassTransform(cur);
    }
}

void PhysicsWorld::step(float deltaTime)
{
    if (!enabled) return;
    // Mode 2: pull toward initial transform (only when significantly displaced)
    for (auto& bb : mBodies) {
        if (bb.mode != 2) continue;
        btTransform cur = bb.body->getCenterOfMassTransform();
        btVector3 tgtPos(bb.initPosX, bb.initPosY, bb.initPosZ);
        btVector3 posErr = tgtPos - cur.getOrigin();
        float errLen = posErr.length();
        if (errLen > 0.005f) {
            bb.body->activate(true);
            bb.body->applyCentralForce(posErr * 50.0f - bb.body->getLinearVelocity() * 15.0f);
        }
        btQuaternion curRot = cur.getRotation();
        btQuaternion tgtRot(bb.initRotX, bb.initRotY, bb.initRotZ, bb.initRotW);
        btQuaternion diff = curRot.inverse() * tgtRot;
        if (diff.w() < 0) diff = btQuaternion(-diff.x(), -diff.y(), -diff.z(), -diff.w());
        float axLen = sqrtf(diff.x()*diff.x() + diff.y()*diff.y() + diff.z()*diff.z());
        if (axLen > 0.001f) {
            bb.body->activate(true);
            float angle = 2.0f * atan2f(axLen, diff.w());
            btVector3 axis(diff.x()/axLen, diff.y()/axLen, diff.z()/axLen);
            bb.body->setAngularVelocity(bb.body->getAngularVelocity() * 0.85f + axis * angle * 10.0f);
        }
    }
    mWorld->stepSimulation(std::min(deltaTime, 1.0f/30.0f), 6, 1.0f/240.0f);
}

void PhysicsWorld::getBoneTransforms(std::vector<std::array<float, 16>>& out) const
{
    for (const auto& bb : mBodies) {
        if (bb.boneIndex < 0 || bb.boneIndex >= (int)out.size()) continue;
        if (!bb.body->isActive()) continue;
        btTransform t = bb.body->getCenterOfMassTransform();
        btVector3 p = t.getOrigin(); btQuaternion r = t.getRotation();
        float tx = p.x()/mModelScale + mCenter.x, ty = p.y()/mModelScale + mMinY, tz = p.z()/mModelScale + mCenter.z;
        auto& m = out[bb.boneIndex];
        m[0]=1-2*(r.y()*r.y()+r.z()*r.z()); m[4]=2*(r.x()*r.y()-r.z()*r.w()); m[8]=2*(r.x()*r.z()+r.y()*r.w()); m[12]=tx;
        m[1]=2*(r.x()*r.y()+r.z()*r.w());  m[5]=1-2*(r.x()*r.x()+r.z()*r.z()); m[9]=2*(r.y()*r.z()-r.x()*r.w()); m[13]=ty;
        m[2]=2*(r.x()*r.z()-r.y()*r.w());  m[6]=2*(r.y()*r.z()+r.x()*r.w());  m[10]=1-2*(r.x()*r.x()+r.y()*r.y()); m[14]=tz;
        m[3]=0; m[7]=0; m[11]=0; m[15]=1;
    }
}
