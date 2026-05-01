#include "DiaRigidBody2D/Bodies/PointBody2D.h"

namespace Dia::RigidBody2D {

const Dia::Core::StringCRC PointBody2D::kUniqueId("PointBody2D");

PointBody2D::PointBody2D(const PointBodyDef& def)
    : Body2DBase(def)
    , mTransform(def.transform ? *def.transform : Dia::Geometry2D::Transform())
    , mLinearDamping(def.linearDamping)
    , mVelocity(Dia::Maths::Vector2D::Zero())
    , mForceAccum(Dia::Maths::Vector2D::Zero())
    , mCircleShape(def.circleShape ? *def.circleShape : Dia::Geometry2D::Circle())
    , mPolyShape(def.polyShape ? *def.polyShape : Dia::Geometry2D::ConvexPolygon())
{
}

void PointBody2D::ApplyForce(const Dia::Maths::Vector2D& force)
{
    if (mInvMass == 0.0f) return;
    mForceAccum += force;
    Wake();
}

void PointBody2D::ApplyImpulse(const Dia::Maths::Vector2D& impulse)
{
    if (mInvMass == 0.0f) return;
    if (mType == BodyType::kKinematic) return;
    mVelocity += impulse * mInvMass;
    Wake();
}

void PointBody2D::ClearForces()
{
    mForceAccum = Dia::Maths::Vector2D::Zero();
}

void PointBody2D::SetVelocity(const Dia::Maths::Vector2D& vel)
{
    mVelocity = vel;
}

const Dia::Maths::Vector2D& PointBody2D::GetVelocity() const
{
    return mVelocity;
}

float PointBody2D::GetLinearDamping() const
{
    return mLinearDamping;
}

Dia::Geometry2D::Transform* PointBody2D::GetTransform()
{
    return &mTransform;
}

const Dia::Geometry2D::Transform* PointBody2D::GetTransform() const
{
    return &mTransform;
}

const Dia::Geometry2D::Circle* PointBody2D::GetCircleShape() const
{
    return mShapeKind == ShapeKind::kCircle ? &mCircleShape : nullptr;
}

const Dia::Geometry2D::ConvexPolygon* PointBody2D::GetPolyShape() const
{
    return mShapeKind == ShapeKind::kPoly ? &mPolyShape : nullptr;
}

const Dia::Maths::Vector2D& PointBody2D::GetForceAccum() const
{
    return mForceAccum;
}

} // namespace Dia::RigidBody2D
