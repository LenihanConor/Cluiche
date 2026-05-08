#pragma once

#include "DiaRigidBody2D/Constraints/IConstraint.h"
#include "DiaMaths/Vector/Vector2D.h"

namespace Dia::RigidBody2D {

class DistanceConstraint : public IConstraint {
public:
    DistanceConstraint(RigidBody2D* bodyA, const Dia::Maths::Vector2D& anchorA,
                       RigidBody2D* bodyB, const Dia::Maths::Vector2D& anchorB,
                       float targetDistance);

    void  SetTargetDistance(float distance) { mTargetDist = distance; }
    float GetTargetDistance() const         { return mTargetDist; }

    void PreStep(float dt) override;
    void ApplyImpulse()    override;
    bool InvolvesBody(const RigidBody2D* body) const override;
    RigidBody2D* GetBodyA() const override { return mBodyA; }
    RigidBody2D* GetBodyB() const override { return mBodyB; }
    Dia::Maths::Vector2D GetWorldAnchorA() const override;
    Dia::Maths::Vector2D GetWorldAnchorB() const override;

private:
    RigidBody2D*         mBodyA;
    RigidBody2D*         mBodyB;
    Dia::Maths::Vector2D mLocalAnchorA;
    Dia::Maths::Vector2D mLocalAnchorB;
    float                mTargetDist;

    // Cached per-step
    Dia::Maths::Vector2D mRa;
    Dia::Maths::Vector2D mRb;
    Dia::Maths::Vector2D mAxis;    // unit vector from B anchor to A anchor
    float                mBias;
    float                mEffMass;
    float                mAccumImpulse;

    static constexpr float kBeta = 0.2f;
};

} // namespace Dia::RigidBody2D
