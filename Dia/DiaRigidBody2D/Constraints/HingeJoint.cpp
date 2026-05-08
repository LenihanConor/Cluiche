#include "DiaRigidBody2D/Constraints/HingeJoint.h"
#include "DiaRigidBody2D/Bodies/RigidBody2D.h"

#include <cmath>

namespace Dia::RigidBody2D {

static Dia::Maths::Vector2D RotateVec(const Dia::Maths::Vector2D& v, float rad)
{
    const float c = std::cos(rad);
    const float s = std::sin(rad);
    return Dia::Maths::Vector2D(c * v.x - s * v.y, s * v.x + c * v.y);
}

static inline float Cross2D(const Dia::Maths::Vector2D& a, const Dia::Maths::Vector2D& b)
{
    return a.x * b.y - a.y * b.x;
}

HingeJoint::HingeJoint(RigidBody2D* bodyA, const Dia::Maths::Vector2D& anchorA,
                        RigidBody2D* bodyB, const Dia::Maths::Vector2D& anchorB)
    : mBodyA(bodyA)
    , mBodyB(bodyB)
    , mLocalAnchorA(anchorA)
    , mLocalAnchorB(anchorB)
    , mHasLimits(false)
    , mMinAngle(Dia::Maths::Angle::FromRadians(0.0f))
    , mMaxAngle(Dia::Maths::Angle::FromRadians(0.0f))
    , mRa(Dia::Maths::Vector2D::Zero())
    , mRb(Dia::Maths::Vector2D::Zero())
    , mBiasXY(Dia::Maths::Vector2D::Zero())
    , mEffMassX(0.0f)
    , mEffMassY(0.0f)
    , mAccumImpulseXY(Dia::Maths::Vector2D::Zero())
    , mAngleBias(0.0f)
    , mAngleEffMass(0.0f)
    , mAccumAngleImpulse(0.0f)
{}

void HingeJoint::SetAngleLimits(const Dia::Maths::Angle& min, const Dia::Maths::Angle& max)
{
    mHasLimits = true;
    mMinAngle  = min;
    mMaxAngle  = max;
}

void HingeJoint::ClearAngleLimits()
{
    mHasLimits = false;
}

void HingeJoint::PreStep(float dt)
{
    const float rotA = mBodyA->GetTransform() ? mBodyA->GetTransform()->GetLocalRotation().AsRadians() : 0.0f;
    const Dia::Maths::Vector2D posA = mBodyA->GetTransform() ? mBodyA->GetTransform()->GetWorldPosition() : Dia::Maths::Vector2D::Zero();
    mRa = RotateVec(mLocalAnchorA, rotA);
    const Dia::Maths::Vector2D worldAnchorA = posA + mRa;

    const float rotB = mBodyB->GetTransform() ? mBodyB->GetTransform()->GetLocalRotation().AsRadians() : 0.0f;
    const Dia::Maths::Vector2D posB = mBodyB->GetTransform() ? mBodyB->GetTransform()->GetWorldPosition() : Dia::Maths::Vector2D::Zero();
    mRb = RotateVec(mLocalAnchorB, rotB);
    const Dia::Maths::Vector2D worldAnchorB = posB + mRb;

    const Dia::Maths::Vector2D C = worldAnchorA - worldAnchorB;
    const float biasFactor = kBeta / dt;
    mBiasXY.x = biasFactor * C.x;
    mBiasXY.y = biasFactor * C.y;

    const float invMA = mBodyA->GetInverseMass();
    const float invIA = mBodyA->GetInverseInertia();
    const float invMB = mBodyB->GetInverseMass();
    const float invIB = mBodyB->GetInverseInertia();

    const float rxA_x = -mRa.y;
    const float rxB_x = -mRb.y;
    const float kX = invMA + invMB + rxA_x * rxA_x * invIA + rxB_x * rxB_x * invIB;
    mEffMassX = (kX > 1e-10f) ? 1.0f / kX : 0.0f;

    const float rxA_y = mRa.x;
    const float rxB_y = mRb.x;
    const float kY = invMA + invMB + rxA_y * rxA_y * invIA + rxB_y * rxB_y * invIB;
    mEffMassY = (kY > 1e-10f) ? 1.0f / kY : 0.0f;

    // Angle limit
    if (mHasLimits)
    {
        const float relAngle = rotA - rotB;
        float angleErr = 0.0f;
        if (relAngle < mMinAngle.AsRadians())
            angleErr = relAngle - mMinAngle.AsRadians();
        else if (relAngle > mMaxAngle.AsRadians())
            angleErr = relAngle - mMaxAngle.AsRadians();

        mAngleBias = biasFactor * angleErr;
        const float kAngular = invIA + invIB;
        mAngleEffMass = (kAngular > 1e-10f) ? 1.0f / kAngular : 0.0f;
    }

    // Warm start (translation)
    mBodyA->ApplyImpulse( mAccumImpulseXY);
    if (mBodyA->GetTransform()) mBodyA->ApplyAngularImpulse( Cross2D(mRa, mAccumImpulseXY));
    mBodyB->ApplyImpulse(-mAccumImpulseXY);
    if (mBodyB->GetTransform()) mBodyB->ApplyAngularImpulse(-Cross2D(mRb, mAccumImpulseXY));
}

void HingeJoint::ApplyImpulse()
{
    // --- Translation ---
    Dia::Maths::Vector2D vA = mBodyA->GetVelocity();
    vA.x += -mBodyA->GetAngularVelocity() * mRa.y;
    vA.y +=  mBodyA->GetAngularVelocity() * mRa.x;

    Dia::Maths::Vector2D vB = mBodyB->GetVelocity();
    vB.x += -mBodyB->GetAngularVelocity() * mRb.y;
    vB.y +=  mBodyB->GetAngularVelocity() * mRb.x;

    const Dia::Maths::Vector2D relVel = vA - vB;
    const float lambdaX = -(relVel.x + mBiasXY.x) * mEffMassX;
    const float lambdaY = -(relVel.y + mBiasXY.y) * mEffMassY;
    const Dia::Maths::Vector2D impulse(lambdaX, lambdaY);
    mAccumImpulseXY.x += lambdaX;
    mAccumImpulseXY.y += lambdaY;

    mBodyA->ApplyImpulse( impulse);
    if (mBodyA->GetTransform()) mBodyA->ApplyAngularImpulse( Cross2D(mRa, impulse));
    mBodyB->ApplyImpulse(-impulse);
    if (mBodyB->GetTransform()) mBodyB->ApplyAngularImpulse(-Cross2D(mRb, impulse));

    // --- Angle limit ---
    if (mHasLimits && mAngleEffMass > 0.0f)
    {
        const float relOmega = mBodyA->GetAngularVelocity() - mBodyB->GetAngularVelocity();
        const float lambdaAngle = -(relOmega + mAngleBias) * mAngleEffMass;
        mAccumAngleImpulse += lambdaAngle;

        mBodyA->ApplyAngularImpulse( lambdaAngle);
        mBodyB->ApplyAngularImpulse(-lambdaAngle);
    }
}

bool HingeJoint::InvolvesBody(const RigidBody2D* body) const
{
    return body == mBodyA || body == mBodyB;
}

Dia::Maths::Vector2D HingeJoint::GetWorldAnchorA() const
{
    if (!mBodyA->GetTransform()) return mLocalAnchorA;
    const float rot = mBodyA->GetTransform()->GetLocalRotation().AsRadians();
    return mBodyA->GetTransform()->GetWorldPosition() + RotateVec(mLocalAnchorA, rot);
}

Dia::Maths::Vector2D HingeJoint::GetWorldAnchorB() const
{
    if (!mBodyB->GetTransform()) return mLocalAnchorB;
    const float rot = mBodyB->GetTransform()->GetLocalRotation().AsRadians();
    return mBodyB->GetTransform()->GetWorldPosition() + RotateVec(mLocalAnchorB, rot);
}

} // namespace Dia::RigidBody2D
