#pragma once

#include "DiaRigidBody2D/Bodies/Body2DBase.h"
#include "DiaRigidBody2D/Constraints/IConstraint.h"
#include "DiaGeometry2D/Transform/Transform.h"
#include "DiaGeometry2D/Shapes/AARect.h"
#include "DiaGeometry2D/Shapes/Circle.h"
#include "DiaGeometry2D/Shapes/ConvexPolygon.h"
#include "DiaMaths/Vector/Vector2D.h"
#include "DiaCore/Containers/Arrays/DynamicArrayC.h"

namespace Dia::RigidBody2D {

struct RigidBodyDef : public Body2DDef {
    float angularDamping  = 0.0f;   // Per-step angular velocity decay
    float momentOfInertia = 1.0f;   // Rotational inertia (I); invInertia = 1/I for dynamic bodies
};

class RigidBody2D : public Body2DBase {
public:
    static const Dia::Core::StringCRC kUniqueId;

    explicit RigidBody2D(const RigidBodyDef& def);

    void ApplyForce(const Dia::Maths::Vector2D& force)     override;
    void ApplyForceAtPoint(const Dia::Maths::Vector2D& force,
                           const Dia::Maths::Vector2D& worldPoint);
    void ApplyTorque(float torque);
    void ApplyImpulse(const Dia::Maths::Vector2D& impulse) override;
    void ApplyAngularImpulse(float impulse)                 override;
    void ClearForces()                                      override;

    void SetVelocity(const Dia::Maths::Vector2D& vel)      override;
    void SetAngularVelocity(float omega);
    const Dia::Maths::Vector2D& GetVelocity()        const override;
    float                       GetAngularVelocity() const override;

    float GetLinearDamping()  const;
    float GetAngularDamping() const;
    float GetInverseInertia() const override;

    Dia::Geometry2D::Transform*       GetTransform()       override;
    const Dia::Geometry2D::Transform* GetTransform() const override;

    const Dia::Geometry2D::Circle*        GetCircleShape() const override;
    const Dia::Geometry2D::ConvexPolygon* GetPolyShape()   const override;

    void         AddConstraint(IConstraint* constraint);
    void         RemoveConstraint(IConstraint* constraint);
    int          GetConstraintCount() const;
    IConstraint* GetConstraint(int index);

    const Dia::Maths::Vector2D& GetForceAccum()  const;
    float                       GetTorqueAccum() const;

private:
    static constexpr unsigned int kMaxConstraints = 16;

    Dia::Geometry2D::Transform                          mTransform;
    float                                               mLinearDamping;
    float                                               mAngularDamping;
    float                                               mInvInertia;
    Dia::Maths::Vector2D                                mVelocity;
    float                                               mAngularVelocity;
    Dia::Maths::Vector2D                                mForceAccum;
    float                                               mTorqueAccum;
    Dia::Core::Containers::DynamicArrayC<IConstraint*, kMaxConstraints> mConstraints;
    Dia::Geometry2D::Circle                             mCircleShape;
    Dia::Geometry2D::ConvexPolygon                      mPolyShape;
};

} // namespace Dia::RigidBody2D
