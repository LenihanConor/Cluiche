#pragma once

#include "DiaRigidBody2D/Constraints/IConstraint.h"
#include "DiaMaths/Vector/Vector2D.h"
#include "DiaMaths/Core/Angle.h"

namespace Dia::RigidBody2D {

class HingeJoint : public IConstraint {
public:
    HingeJoint(RigidBody2D* bodyA, const Dia::Maths::Vector2D& anchorA,
               RigidBody2D* bodyB, const Dia::Maths::Vector2D& anchorB);

    // Optional angle limits — call to enable
    void SetAngleLimits(const Dia::Maths::Angle& min, const Dia::Maths::Angle& max);
    void ClearAngleLimits();

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

    bool               mHasLimits;
    Dia::Maths::Angle  mMinAngle;
    Dia::Maths::Angle  mMaxAngle;

    // Cached per-step (translation part — same as PinJoint)
    Dia::Maths::Vector2D mRa;
    Dia::Maths::Vector2D mRb;
    Dia::Maths::Vector2D mBiasXY;
    float                mEffMassX;
    float                mEffMassY;
    Dia::Maths::Vector2D mAccumImpulseXY;

    // Angle limit
    float mAngleBias;
    float mAngleEffMass;
    float mAccumAngleImpulse;

    static constexpr float kBeta = 0.2f;
};

} // namespace Dia::RigidBody2D
