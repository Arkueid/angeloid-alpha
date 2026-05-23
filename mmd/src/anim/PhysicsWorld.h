#pragma once

#include "pmx/PmxModel.h"

#include <array>
#include <memory>
#include <vector>

class btDiscreteDynamicsWorld;
class btRigidBody;
class btCollisionShape;
class btTypedConstraint;
class btDefaultCollisionConfiguration;
class btCollisionDispatcher;
class btBroadphaseInterface;
class btSequentialImpulseConstraintSolver;
class btVector3;
class btQuaternion;

struct BulletBody {
    btRigidBody* body = nullptr;
    int boneIndex = -1;
    int rigidBodyIndex = -1;
    int mode = 0;
    float initPosX = 0, initPosY = 0, initPosZ = 0;
    float initRotX = 0, initRotY = 0, initRotZ = 0, initRotW = 1;
    std::string name;
};

class PhysicsWorld {
public:
    PhysicsWorld();
    ~PhysicsWorld();

    void build(const PmxModel& model, float modelScale);
    void step(float deltaTime, const std::vector<std::array<float, 16>>& poseWorld);
    void getBoneTransforms(std::vector<std::array<float, 16>>& out) const;

    // For debug visualization
    const std::vector<BulletBody>& bodies() const { return mBodies; }
    float modelScale() const { return mModelScale; }
    void debugDump() const;

    // Update mode 0 bodies to follow their linked bones
    // poseWorld: bone world matrices (model space, column-major 4x4)
    void updateMode0Bodies(const std::vector<std::array<float, 16>>& poseWorld);

    bool enabled = true;

private:
    void addRigidBody(const PmxRigidBody& rb);
    void addJoint(const PmxJoint& jt);
    void computeBoneTarget(const BulletBody& bb, const std::vector<std::array<float, 16>>& poseWorld,
                           btVector3& outPos, btQuaternion& outRot) const;

    std::unique_ptr<btDefaultCollisionConfiguration> mCollisionCfg;
    std::unique_ptr<btCollisionDispatcher> mDispatcher;
    std::unique_ptr<btBroadphaseInterface> mBroadphase;
    std::unique_ptr<btSequentialImpulseConstraintSolver> mSolver;
    std::unique_ptr<btDiscreteDynamicsWorld> mWorld;

    std::vector<std::unique_ptr<btCollisionShape>> mShapes;
    std::vector<BulletBody> mBodies;
    std::vector<std::unique_ptr<btTypedConstraint>> mConstraints;

    float mModelScale = 1.0f;
    Vec3 mCenter;
    float mMinY = 0;
    std::vector<std::array<float, 16>> mBoneBindWorld; // bone bind-pose world matrices
};
