#include "DiaRigidBody2D/Constraints/PinJoint.h"
#include "DiaRigidBody2D/Bodies/RigidBody2D.h"

#include <cmath>

namespace Dia::RigidBody2D {

// Helper: rotate a vector by angle (radians)
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

PinJoint::PinJoint(RigidBody2D* bodyA, const Dia::Maths::Vector2D& anchorA,
                   RigidBody2D* bodyB, const Dia::Maths::Vector2D& anchorB)
    : mBodyA(bodyA)
    , mBodyB(bodyB)
    , mLocalAnchorA(anchorA)
    , mLocalAnchorB(anchorB)
    , mRa(Dia::Maths::Vector2D::Zero())
    , mRb(Dia::Maths::Vector2D::Zero())
    , mBias(Dia::Maths::Vector2D::Zero())
    , mEffMassX(0.0f)
    , mEffMassY(0.0f)
    , mAccumImpulse(Dia::Maths::Vector2D::Zero())
{}

void PinJoint::PreStep(float dt)
{
    const float rotA = mBodyA->GetTransform() ? mBodyA->GetTransform()->GetLocalRotation().AsRadians() : 0.0f;
    const Dia::Maths::Vector2D posA = mBodyA->GetTransform() ? mBodyA->GetTransform()->GetWorldPosition() : Dia::Maths::Vector2D::Zero();

    mRa = RotateVec(mLocalAnchorA, rotA);
    const Dia::Maths::Vector2D worldAnchorA = posA + mRa;

    Dia::Maths::Vector2D worldAnchorB;
    if (mBodyB && mBodyB->GetTransform())
    {
        const float rotB = mBodyB->GetTransform()->GetLocalRotation().AsRadians();
        const Dia::Maths::Vector2D posB = mBodyB->GetTransform()->GetWorldPosition();
        mRb = RotateVec(mLocalAnchorB, rotB);
        worldAnchorB = posB + mRb;
    }
    else
    {
        mRb         = Dia::Maths::Vector2D::Zero();
        worldAnchorB = mLocalAnchorB;  // fixed world point
    }

    // Position error
    const Dia::Maths::Vector2D C = worldAnchorA - worldAnchorB;

    // Baumgarte bias (clamp small errors)
    const float bias_factor = kBeta / dt;
    mBias.x = bias_factor * C.x;
    mBias.y = bias_factor * C.y;

    // Effective mass: 1 / (invMA + invMB + (rA x n)^2 * invIA + (rB x n)^2 * invIB)
    // For pin joint, each axis is independent
    const float invMA = mBodyA->GetInverseMass();
    const float invIA = mBodyA->GetInverseInertia();
    const float invMB = mBodyB ? mBodyB->GetInverseMass() : 0.0f;
    const float invIB = mBodyB ? mBodyB->GetInverseInertia() : 0.0f;

    // X axis
    const float rxA_x = -mRa.y;  // (rA x eX) — cross with unit X = -rA.y
    const float rxB_x = -mRb.y;
    const float kX = invMA + invMB + rxA_x * rxA_x * invIA + rxB_x * rxB_x * invIB;
    mEffMassX = (kX > 1e-10f) ? 1.0f / kX : 0.0f;

    // Y axis
    const float rxA_y = mRa.x;   // (rA x eY) = rA.x
    const float rxB_y = mRb.x;
    const float kY = invMA + invMB + rxA_y * rxA_y * invIA + rxB_y * rxB_y * invIB;
    mEffMassY = (kY > 1e-10f) ? 1.0f / kY : 0.0f;

    // Warm start: re-apply accumulated impulse
    mBodyA->ApplyImpulse( mAccumImpulse);
    if (mBodyA->GetTransform())
        mBodyA->ApplyAngularImpulse(Cross2D(mRa, mAccumImpulse));
    if (mBodyB)
    {
        mBodyB->ApplyImpulse(-mAccumImpulse);
        if (mBodyB->GetTransform())
            mBodyB->ApplyAngularImpulse(-Cross2D(mRb, mAccumImpulse));
    }
}

void PinJoint::ApplyImpulse()
{
    // Relative velocity at anchor A minus anchor B
    Dia::Maths::Vector2D vA = mBodyA->GetVelocity();
    vA.x += -mBodyA->GetAngularVelocity() * mRa.y;
    vA.y +=  mBodyA->GetAngularVelocity() * mRa.x;

    Dia::Maths::Vector2D vB(0.0f, 0.0f);
    if (mBodyB)
    {
        vB = mBodyB->GetVelocity();
        vB.x += -mBodyB->GetAngularVelocity() * mRb.y;
        vB.y +=  mBodyB->GetAngularVelocity() * mRb.x;
    }

    const Dia::Maths::Vector2D relVel = vA - vB;

    // Lambda per axis
    const float lambdaX = -(relVel.x + mBias.x) * mEffMassX;
    const float lambdaY = -(relVel.y + mBias.y) * mEffMassY;

    const Dia::Maths::Vector2D impulse(lambdaX, lambdaY);
    mAccumImpulse.x += lambdaX;
    mAccumImpulse.y += lambdaY;

    mBodyA->ApplyImpulse(impulse);
    if (mBodyA->GetTransform())
        mBodyA->ApplyAngularImpulse(Cross2D(mRa, impulse));

    if (mBodyB)
    {
        mBodyB->ApplyImpulse(-impulse);
        if (mBodyB->GetTransform())
            mBodyB->ApplyAngularImpulse(-Cross2D(mRb, impulse));
    }
}

bool PinJoint::InvolvesBody(const RigidBody2D* body) const
{
    return body == mBodyA || body == mBodyB;
}

} // namespace Dia::RigidBody2D
