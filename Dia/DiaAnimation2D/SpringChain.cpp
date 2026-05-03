#include "SpringChain.h"
#include <DiaRig2D/Skeleton.h>
#include <DiaRig2D/Pose.h>
#include <DiaRig2D/BoneTransform.h>
#include <DiaRig2D/Bone.h>
#include <DiaCore/Core/Assert.h>
#include <cmath>

namespace Dia { namespace Animation2D {

static constexpr float kMaxSubstepDt   = 1.0f / 120.0f; // substep cap for numerical stability
static constexpr float kGravityEpsilon = 1e-6f;          // minimum gravity direction magnitude

SpringChain::SpringChain(const SpringChainDef& def, const Dia::Rig2D::Skeleton& skeleton)
    : mId(def.id)
    , mGravityDirection(def.gravityDirection)
    , mGravityStrength(def.gravityStrength)
{
    mRootBoneIndex = skeleton.FindBoneIndex(def.rootBoneId);
    DIA_ASSERT(mRootBoneIndex != -1, "Root bone not found in skeleton");

    DIA_ASSERT(def.nodeOverrides.Size() == 0 || def.nodeOverrides.Size() == def.boneIds.Size(),
        "nodeOverrides must be empty or same size as boneIds");

    int prevIdx = mRootBoneIndex;
    for (unsigned int i = 0; i < def.boneIds.Size(); ++i) {
        int idx = skeleton.FindBoneIndex(def.boneIds[i]);
        DIA_ASSERT(idx != -1, "Spring chain bone not found in skeleton");
        DIA_ASSERT(idx == prevIdx + 1, "Spring chain bones must be contiguous");
        prevIdx = idx;

        NodeState node;
        node.boneIndex = idx;
        node.params = (def.nodeOverrides.Size() > 0) ? def.nodeOverrides[i] : def.defaultNode;
        node.angularVelocity = 0.0f;
        node.externalTorque = 0.0f;
        mNodes.Add(node);
    }

    float len = std::sqrt(mGravityDirection.x * mGravityDirection.x + mGravityDirection.y * mGravityDirection.y);
    if (len > kGravityEpsilon)
        mGravityDirection = Dia::Maths::Vector2D(mGravityDirection.x / len, mGravityDirection.y / len);
    else if (mGravityStrength > 0.0f)
        mGravityStrength = 0.0f;
}

void SpringChain::Update(float dt,
    Dia::Rig2D::Pose& pose,
    const Dia::Core::Containers::DynamicArrayC<Dia::Rig2D::BoneTransform, 128>& worldTransforms)
{
    DIA_ASSERT(dt >= 0.0f, "dt must be non-negative");
    if (dt == 0.0f) return;

    const float kMaxStep = kMaxSubstepDt;
    float remaining = dt;
    bool firstSubstep = true;

    while (remaining > 0.0f) {
        float step = (remaining > kMaxStep) ? kMaxStep : remaining;
        remaining -= step;

        for (int i = 0; i < (int)mNodes.Size(); ++i) {
            NodeState& node = mNodes[i];
            float localRot = pose.GetLocalTransform(node.boneIndex).rotation;

            float worldAngle = (node.boneIndex < (int)worldTransforms.Size())
                ? worldTransforms[node.boneIndex].rotation : 0.0f;
            float gravTorque = 0.0f;
            if (mGravityStrength > 0.0f) {
                float boneX = std::cos(worldAngle);
                float boneY = std::sin(worldAngle);
                float perpDotGrav     = (-boneY) * mGravityDirection.x + boneX * mGravityDirection.y;
                float parallelDotGrav =   boneX  * mGravityDirection.x + boneY * mGravityDirection.y;
                gravTorque = mGravityStrength * (perpDotGrav + parallelDotGrav);
            }

            float extTorque = firstSubstep ? node.externalTorque : 0.0f;

            float acceleration = -node.params.stiffness * localRot
                                 - node.params.damping * node.angularVelocity
                                 + gravTorque + extTorque;

            node.angularVelocity += acceleration * step;
            if (node.angularVelocity > node.params.maxAngularVelocity)
                node.angularVelocity = node.params.maxAngularVelocity;
            else if (node.angularVelocity < -node.params.maxAngularVelocity)
                node.angularVelocity = -node.params.maxAngularVelocity;

            localRot += node.angularVelocity * step;
            pose.GetLocalTransform(node.boneIndex).rotation = localRot;
        }
        firstSubstep = false;
    }

    for (int i = 0; i < (int)mNodes.Size(); ++i)
        mNodes[i].externalTorque = 0.0f;
}

void SpringChain::ApplyExternalTorque(int nodeIndex, float torque) {
    DIA_ASSERT(nodeIndex >= 0 && nodeIndex < (int)mNodes.Size(), "nodeIndex out of range");
    mNodes[nodeIndex].externalTorque += torque;
}

void SpringChain::Update(float dt, const Dia::Rig2D::Skeleton& skeleton, Dia::Rig2D::Pose& pose) {
    Dia::Core::Containers::DynamicArrayC<Dia::Rig2D::BoneTransform, 128> worldTransforms;
    Dia::Rig2D::BoneTransform identity;
    pose.ComputeWorldTransforms(skeleton, identity, worldTransforms);
    Update(dt, pose, worldTransforms);
}

void SpringChain::Reset(const Dia::Rig2D::Pose& pose) {
    (void)pose;
    for (int i = 0; i < (int)mNodes.Size(); ++i) {
        mNodes[i].angularVelocity = 0.0f;
        mNodes[i].externalTorque = 0.0f;
    }
}

void SpringChain::Reset(const Dia::Rig2D::Skeleton& skeleton, Dia::Rig2D::Pose& pose) {
    (void)skeleton;
    // Zero velocity, external torques, and spring displacement (rotation) for all chain bones.
    // This makes the current configuration the new rest state.
    for (int i = 0; i < (int)mNodes.Size(); ++i) {
        mNodes[i].angularVelocity = 0.0f;
        mNodes[i].externalTorque = 0.0f;
        pose.GetLocalTransform(mNodes[i].boneIndex).rotation = 0.0f;
    }
}

void SpringChain::SetGravity(const Dia::Maths::Vector2D& direction) {
    float len = std::sqrt(direction.x * direction.x + direction.y * direction.y);
    SetGravity(direction, len);
}

void SpringChain::SetNodeStiffness(int nodeIndex, float stiffness) {
    DIA_ASSERT(nodeIndex >= 0 && nodeIndex < (int)mNodes.Size(), "nodeIndex out of range");
    mNodes[nodeIndex].params.stiffness = stiffness;
}

void SpringChain::SetNodeDamping(int nodeIndex, float damping) {
    DIA_ASSERT(nodeIndex >= 0 && nodeIndex < (int)mNodes.Size(), "nodeIndex out of range");
    mNodes[nodeIndex].params.damping = damping;
}

void SpringChain::SetNodeMaxAngularVelocity(int nodeIndex, float maxAngularVelocity) {
    DIA_ASSERT(nodeIndex >= 0 && nodeIndex < (int)mNodes.Size(), "nodeIndex out of range");
    mNodes[nodeIndex].params.maxAngularVelocity = maxAngularVelocity;
}

void SpringChain::SetGravity(const Dia::Maths::Vector2D& direction, float strength) {
    mGravityStrength = strength;
    mGravityDirection = direction;
    float len = std::sqrt(direction.x * direction.x + direction.y * direction.y);
    if (len > kGravityEpsilon)
        mGravityDirection = Dia::Maths::Vector2D(direction.x / len, direction.y / len);
    else if (strength > 0.0f)
        mGravityStrength = 0.0f;
}

const Dia::Core::StringCRC& SpringChain::GetId() const { return mId; }
int SpringChain::GetNodeCount() const { return (int)mNodes.Size(); }

} }
