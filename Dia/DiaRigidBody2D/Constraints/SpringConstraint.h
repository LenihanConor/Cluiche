#pragma once

#include "DiaRigidBody2D/Constraints/IConstraint.h"
#include "DiaMaths/Vector/Vector2D.h"

namespace Dia::RigidBody2D {

class SpringConstraint : public IConstraint {
public:
    SpringConstraint(RigidBody2D* bodyA, const Dia::Maths::Vector2D& anchorA,
                     RigidBody2D* bodyB, const Dia::Maths::Vector2D& anchorB,
                     float restLength, float stiffness, float damping);

    void  SetStiffness(float k)   { mStiffness = k; }
    void  SetDamping(float d)     { mDamping   = d; }
    void  SetRestLength(float len){ mRestLength = len; }

    float GetStiffness()  const { return mStiffness; }
    float GetDamping()    const { return mDamping; }
    float GetRestLength() const { return mRestLength; }

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
    float                mRestLength;
    float                mStiffness;
    float                mDamping;

    // Cached per-step
    Dia::Maths::Vector2D mRa;
    Dia::Maths::Vector2D mRb;
    Dia::Maths::Vector2D mAxis;
    float                mBias;
    float                mEffMass;   // includes compliance term
    float                mAccumImpulse;
    float                mDt;
};

} // namespace Dia::RigidBody2D
