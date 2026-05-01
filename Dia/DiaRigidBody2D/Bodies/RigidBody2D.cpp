#include "DiaRigidBody2D/Bodies/RigidBody2D.h"

#include "DiaCore/Core/Assert.h"

namespace Dia::RigidBody2D {

const Dia::Core::StringCRC RigidBody2D::kUniqueId("RigidBody2D");

RigidBody2D::RigidBody2D(const RigidBodyDef& def)
    : Body2DBase(def)
    , mTransform(def.transform ? *def.transform : Dia::Geometry2D::Transform())
    , mLinearDamping(def.linearDamping)
    , mAngularDamping(def.angularDamping)
    , mInvInertia(0.0f)
    , mVelocity(Dia::Maths::Vector2D::Zero())
    , mAngularVelocity(0.0f)
    , mForceAccum(Dia::Maths::Vector2D::Zero())
    , mTorqueAccum(0.0f)
    , mCircleShape(def.circleShape ? *def.circleShape : Dia::Geometry2D::Circle())
    , mPolyShape(def.polyShape ? *def.polyShape : Dia::Geometry2D::ConvexPolygon())
{
    if (mType == BodyType::kDynamic && mMass > 0.0f && def.momentOfInertia > 0.0f)
        mInvInertia = 1.0f / def.momentOfInertia;
}

void RigidBody2D::ApplyForce(const Dia::Maths::Vector2D& force)
{
    if (mInvMass == 0.0f) return;
    mForceAccum += force;
    Wake();
}

void RigidBody2D::ApplyForceAtPoint(const Dia::Maths::Vector2D& force,
                                    const Dia::Maths::Vector2D& worldPoint)
{
    if (mInvMass == 0.0f) return;
    mForceAccum += force;

    const Dia::Maths::Vector2D r = worldPoint - mTransform.GetWorldPosition();
    mTorqueAccum += r.x * force.y - r.y * force.x;
    Wake();
}

void RigidBody2D::ApplyTorque(float torque)
{
    if (mInvInertia == 0.0f) return;
    mTorqueAccum += torque;
}

void RigidBody2D::ApplyImpulse(const Dia::Maths::Vector2D& impulse)
{
    if (mInvMass == 0.0f) return;
    if (mType == BodyType::kKinematic) return;
    mVelocity += impulse * mInvMass;
    Wake();
}

void RigidBody2D::ApplyAngularImpulse(float impulse)
{
    if (mInvInertia == 0.0f) return;
    if (mType == BodyType::kKinematic) return;
    mAngularVelocity += impulse * mInvInertia;
    Wake();
}

void RigidBody2D::ClearForces()
{
    mForceAccum     = Dia::Maths::Vector2D::Zero();
    mTorqueAccum    = 0.0f;
}

void RigidBody2D::SetVelocity(const Dia::Maths::Vector2D& vel)
{
    mVelocity = vel;
}

void RigidBody2D::SetAngularVelocity(float omega)
{
    mAngularVelocity = omega;
}

const Dia::Maths::Vector2D& RigidBody2D::GetVelocity() const
{
    return mVelocity;
}

float RigidBody2D::GetAngularVelocity() const
{
    return mAngularVelocity;
}

float RigidBody2D::GetLinearDamping() const
{
    return mLinearDamping;
}

float RigidBody2D::GetAngularDamping() const
{
    return mAngularDamping;
}

float RigidBody2D::GetInverseInertia() const
{
    return mInvInertia;
}

Dia::Geometry2D::Transform* RigidBody2D::GetTransform()
{
    return &mTransform;
}

const Dia::Geometry2D::Transform* RigidBody2D::GetTransform() const
{
    return &mTransform;
}

const Dia::Geometry2D::Circle* RigidBody2D::GetCircleShape() const
{
    return mShapeKind == ShapeKind::kCircle ? &mCircleShape : nullptr;
}

const Dia::Geometry2D::ConvexPolygon* RigidBody2D::GetPolyShape() const
{
    return mShapeKind == ShapeKind::kPoly ? &mPolyShape : nullptr;
}

void RigidBody2D::AddConstraint(IConstraint* constraint)
{
    DIA_ASSERT(constraint != nullptr, "Cannot add null constraint");
    mConstraints.Add(constraint);
}

void RigidBody2D::RemoveConstraint(IConstraint* constraint)
{
    for (unsigned int i = 0; i < mConstraints.Size(); ++i)
    {
        if (mConstraints[i] == constraint)
        {
            mConstraints.RemoveAt(i);
            return;
        }
    }
}

int RigidBody2D::GetConstraintCount() const
{
    return static_cast<int>(mConstraints.Size());
}

IConstraint* RigidBody2D::GetConstraint(int index)
{
    DIA_ASSERT(index >= 0 && static_cast<unsigned int>(index) < mConstraints.Size(), "Constraint index out of range");
    return mConstraints[index];
}

const Dia::Maths::Vector2D& RigidBody2D::GetForceAccum() const
{
    return mForceAccum;
}

float RigidBody2D::GetTorqueAccum() const
{
    return mTorqueAccum;
}

} // namespace Dia::RigidBody2D
