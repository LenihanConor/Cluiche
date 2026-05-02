#include "DiaRigidBody2D/Constraints/DistanceConstraint.h"
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

static inline float Dot2D(const Dia::Maths::Vector2D& a, const Dia::Maths::Vector2D& b)
{
    return a.x * b.x + a.y * b.y;
}

DistanceConstraint::DistanceConstraint(
    RigidBody2D* bodyA, const Dia::Maths::Vector2D& anchorA,
    RigidBody2D* bodyB, const Dia::Maths::Vector2D& anchorB,
    float targetDistance)
    : mBodyA(bodyA)
    , mBodyB(bodyB)
    , mLocalAnchorA(anchorA)
    , mLocalAnchorB(anchorB)
    , mTargetDist(targetDistance)
    , mRa(Dia::Maths::Vector2D::Zero())
    , mRb(Dia::Maths::Vector2D::Zero())
    , mAxis(Dia::Maths::Vector2D(1.0f, 0.0f))
    , mBias(0.0f)
    , mEffMass(0.0f)
    , mAccumImpulse(0.0f)
{}

void DistanceConstraint::PreStep(float dt)
{
    const float rotA = mBodyA->GetTransform() ? mBodyA->GetTransform()->GetLocalRotation().AsRadians() : 0.0f;
    const Dia::Maths::Vector2D posA = mBodyA->GetTransform() ? mBodyA->GetTransform()->GetWorldPosition() : Dia::Maths::Vector2D::Zero();
    mRa = RotateVec(mLocalAnchorA, rotA);
    const Dia::Maths::Vector2D worldAnchorA = posA + mRa;

    const float rotB = mBodyB->GetTransform() ? mBodyB->GetTransform()->GetLocalRotation().AsRadians() : 0.0f;
    const Dia::Maths::Vector2D posB = mBodyB->GetTransform() ? mBodyB->GetTransform()->GetWorldPosition() : Dia::Maths::Vector2D::Zero();
    mRb = RotateVec(mLocalAnchorB, rotB);
    const Dia::Maths::Vector2D worldAnchorB = posB + mRb;

    Dia::Maths::Vector2D delta = worldAnchorA - worldAnchorB;
    float dist = std::sqrt(delta.x * delta.x + delta.y * delta.y);
    if (dist < 1e-6f) { mAxis = Dia::Maths::Vector2D(1.0f, 0.0f); dist = 0.0f; }
    else              { mAxis = Dia::Maths::Vector2D(delta.x / dist, delta.y / dist); }

    const float C = dist - mTargetDist;
    mBias = (kBeta / dt) * C;

    const float invMA = mBodyA->GetInverseMass();
    const float invIA = mBodyA->GetInverseInertia();
    const float invMB = mBodyB->GetInverseMass();
    const float invIB = mBodyB->GetInverseInertia();

    const float raCrossN = Cross2D(mRa, mAxis);
    const float rbCrossN = Cross2D(mRb, mAxis);
    const float k = invMA + invMB + raCrossN * raCrossN * invIA + rbCrossN * rbCrossN * invIB;
    mEffMass = (k > 1e-10f) ? 1.0f / k : 0.0f;

    // Warm start
    const Dia::Maths::Vector2D warmImpulse(mAxis.x * mAccumImpulse, mAxis.y * mAccumImpulse);
    mBodyA->ApplyImpulse( warmImpulse);
    if (mBodyA->GetTransform()) mBodyA->ApplyAngularImpulse( Cross2D(mRa, warmImpulse));
    mBodyB->ApplyImpulse(-warmImpulse);
    if (mBodyB->GetTransform()) mBodyB->ApplyAngularImpulse(-Cross2D(mRb, warmImpulse));
}

void DistanceConstraint::ApplyImpulse()
{
    Dia::Maths::Vector2D vA = mBodyA->GetVelocity();
    vA.x += -mBodyA->GetAngularVelocity() * mRa.y;
    vA.y +=  mBodyA->GetAngularVelocity() * mRa.x;

    Dia::Maths::Vector2D vB = mBodyB->GetVelocity();
    vB.x += -mBodyB->GetAngularVelocity() * mRb.y;
    vB.y +=  mBodyB->GetAngularVelocity() * mRb.x;

    const float relVelAlongAxis = Dot2D(vA - vB, mAxis);
    const float lambda = -(relVelAlongAxis + mBias) * mEffMass;
    mAccumImpulse += lambda;

    const Dia::Maths::Vector2D impulse(mAxis.x * lambda, mAxis.y * lambda);
    mBodyA->ApplyImpulse( impulse);
    if (mBodyA->GetTransform()) mBodyA->ApplyAngularImpulse( Cross2D(mRa, impulse));
    mBodyB->ApplyImpulse(-impulse);
    if (mBodyB->GetTransform()) mBodyB->ApplyAngularImpulse(-Cross2D(mRb, impulse));
}

bool DistanceConstraint::InvolvesBody(const RigidBody2D* body) const
{
    return body == mBodyA || body == mBodyB;
}

Dia::Maths::Vector2D DistanceConstraint::GetWorldAnchorA() const
{
    if (!mBodyA->GetTransform()) return mLocalAnchorA;
    const float rot = mBodyA->GetTransform()->GetLocalRotation().AsRadians();
    return mBodyA->GetTransform()->GetWorldPosition() + RotateVec(mLocalAnchorA, rot);
}

Dia::Maths::Vector2D DistanceConstraint::GetWorldAnchorB() const
{
    if (!mBodyB->GetTransform()) return mLocalAnchorB;
    const float rot = mBodyB->GetTransform()->GetLocalRotation().AsRadians();
    return mBodyB->GetTransform()->GetWorldPosition() + RotateVec(mLocalAnchorB, rot);
}

} // namespace Dia::RigidBody2D
