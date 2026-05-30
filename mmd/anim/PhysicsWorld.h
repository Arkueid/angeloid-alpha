#pragma once

#include "pmx/PmxModel.h"

#include <array>
#include <memory>
#include <vector>

// Shape size multipliers from PMX value to Bullet parameter
inline constexpr float kSphereShapeScale = 0.9f;   // btSphereShape radius
inline constexpr float kBoxShapeScale = 0.9f;      // btBoxShape half-extent
inline constexpr float kCapsuleShapeScale = 0.9f;  // btCapsuleShape radius & height

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

// Tracks a Bullet rigid body and its relationship to the PMX skeleton.
// All positions/rotations stored here are in Bullet-native space (center-relative,
// Y-offset by mMinY) — NOT model space.
struct BulletBody {
    btRigidBody* body = nullptr;
    int boneIndex = -1;       // linked PMX bone index
    int rigidBodyIndex = -1;  // PMX rigidbody index
    int mode = 0;             // 0=kinematic, 1=dynamic, 2=bone-align
    // Body initial center-of-mass in Bullet space (offset from model center)
    float initPosX = 0, initPosY = 0, initPosZ = 0;
    float initRotX = 0, initRotY = 0, initRotZ = 0, initRotW = 1;
    // Bone's bind-pose world transform in Bullet space.
    // Stored at build time; used in getBoneTransforms() to compute the body→bone delta
    // and feed physics results back into the skeleton.
    float bonePosX = 0, bonePosY = 0, bonePosZ = 0;
    float boneRotX = 0, boneRotY = 0, boneRotZ = 0, boneRotW = 1;
    // True when connected to rotation-spring joints with freedom range.
    // Affects getBoneTransforms(): cloth-like bodies contribute physics rotation
    // but use bone-driven position to avoid cloth snapping.
    bool clothLike = false;
    std::string name;
};

// Bullet Physics wrapper for PMX rigid bodies and joints.
// Runs simulation in PMX-native space (no modelScale — gravity is scaled instead).
// Coordinates are offset by model center so physics runs near origin for numerical stability.
// The feedback loop: animation bones → updateMode0Bodies / step() → getBoneTransforms() → GPU.
class PhysicsWorld {
public:
    PhysicsWorld();
    ~PhysicsWorld();

    void build(const PmxModel& model, float modelScale);
    void resetPhysics(const std::vector<std::array<float, 16>>& poseWorld);
    void step(float deltaTime, const std::vector<std::array<float, 16>>& poseWorld);
    void getBoneTransforms(std::vector<std::array<float, 16>>& out) const;

    // For debug visualization
    const std::vector<BulletBody>& bodies() const {
        return mBodies;
    }
    float modelScale() const {
        return mModelScale;
    }
    void debugDump() const;
    void debugTrackCloth() const;

    // Update mode 0 bodies to follow their linked bones
    // poseWorld: bone world matrices (model space, column-major 4x4)
    void updateMode0Bodies(const std::vector<std::array<float, 16>>& poseWorld);

    bool enabled = true;

private:
    void addRigidBody(const PmxRigidBody& rb);
    void addJoint(const PmxJoint& jt);
    void computeBoneTarget(const BulletBody& bb,
                           const std::vector<std::array<float, 16>>& poseWorld, btVector3& outPos,
                           btQuaternion& outRot) const;

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
    std::vector<std::array<float, 16>> mBoneBindWorld;  // bone bind-pose world matrices
};
