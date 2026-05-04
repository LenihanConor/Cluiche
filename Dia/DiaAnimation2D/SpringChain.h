#pragma once
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaMaths/Vector/Vector2D.h>
#include "SpringChainDef.h"

namespace Dia { namespace Rig2D { class Skeleton; class Pose; struct BoneTransform; } }

namespace Dia { namespace Animation2D {
    class SpringChain {
    public:
        explicit SpringChain(const SpringChainDef& def, const Dia::Rig2D::Skeleton& skeleton);

        void Update(float dt,
                    Dia::Rig2D::Pose& pose,
                    const Dia::Core::Containers::DynamicArrayC<Dia::Rig2D::BoneTransform, 128>& worldTransforms);

        // Convenience overload: no world transforms needed (gravity uses world angle = 0)
        void Update(float dt, const Dia::Rig2D::Skeleton& skeleton, Dia::Rig2D::Pose& pose);

        void ApplyExternalTorque(int nodeIndex, float torque);
        void Reset(const Dia::Rig2D::Pose& pose);
        void Reset(const Dia::Rig2D::Skeleton& skeleton, Dia::Rig2D::Pose& pose);

        void SetNodeStiffness(int nodeIndex, float stiffness);
        void SetNodeDamping(int nodeIndex, float damping);
        void SetNodeMaxAngularVelocity(int nodeIndex, float maxAngularVelocity);
        void SetGravity(const Dia::Maths::Vector2D& direction, float strength);
        void SetGravity(const Dia::Maths::Vector2D& direction);

        const Dia::Core::StringCRC& GetId() const;
        int GetNodeCount() const;

        // Per-node bone identity — node index matches boneIds order in SpringChainDef
        Dia::Core::StringCRC GetNodeBoneId(int nodeIndex) const;

        // Per-node physics state — read-only; updated each Update() call
        float GetNodeAngularVelocity(int nodeIndex) const;
        float GetNodeStiffness(int nodeIndex) const;
        float GetNodeDamping(int nodeIndex) const;

        // Chain-level gravity
        Dia::Maths::Vector2D GetGravityDirection() const;
        float                GetGravityStrength() const;

    private:
        struct NodeState {
            float angularVelocity = 0.0f;
            float externalTorque  = 0.0f;
            int   boneIndex       = -1;
            Dia::Core::StringCRC boneId;
            SpringNodeDef params;
        };

        void StepNode(NodeState& node, float dt, float targetAngle, float currentAngle,
                      float gravityTorque, Dia::Rig2D::Pose& pose);

        Dia::Core::StringCRC mId;
        int mRootBoneIndex = -1;
        Dia::Core::Containers::DynamicArrayC<NodeState, 64> mNodes;
        Dia::Maths::Vector2D mGravityDirection;
        float mGravityStrength = 0.0f;
    };
} }
