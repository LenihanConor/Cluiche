#pragma once

#include "DiaRigidBody2D/Constraints/IConstraint.h"
#include "DiaMaths/Vector/Vector2D.h"

namespace Dia::RigidBody2D {

class PinJoint : public IConstraint {
public:
    // bodyB == nullptr pins bodyA's anchorA to a fixed world point (anchorB)
    PinJoint(RigidBody2D* bodyA, const Dia::Maths::Vector2D& anchorA,
             RigidBody2D* bodyB, const Dia::Maths::Vector2D& anchorB);

    void PreStep(float dt) override;
    void ApplyImpulse()    override;
    bool InvolvesBody(const RigidBody2D* body) const override;
    RigidBody2D* GetBodyA() const override { return mBodyA; }
    RigidBody2D* GetBodyB() const override { return mBodyB; }

private:
    RigidBody2D*         mBodyA;
    RigidBody2D*         mBodyB;      // may be null
    Dia::Maths::Vector2D mLocalAnchorA;
    Dia::Maths::Vector2D mLocalAnchorB;

    // Cached per-step
    Dia::Maths::Vector2D mRa;         // world-space arm from CoM A to anchor
    Dia::Maths::Vector2D mRb;         // world-space arm from CoM B to anchor
    Dia::Maths::Vector2D mBias;       // Baumgarte position correction
    float                mEffMassX;
    float                mEffMassY;
    Dia::Maths::Vector2D mAccumImpulse;

    static constexpr float kBeta = 0.2f;
    static constexpr float kSlop = 0.005f;
};

} // namespace Dia::RigidBody2D
