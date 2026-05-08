#pragma once

#include "DiaRigidBody2D/Bodies/Body2DBase.h"
#include "DiaGeometry2D/Transform/Transform.h"
#include "DiaGeometry2D/Shapes/AARect.h"
#include "DiaGeometry2D/Shapes/Circle.h"
#include "DiaGeometry2D/Shapes/ConvexPolygon.h"
#include "DiaMaths/Vector/Vector2D.h"

namespace Dia::RigidBody2D {

using PointBodyDef = Body2DDef;

class PointBody2D : public Body2DBase {
public:
    static const Dia::Core::StringCRC kUniqueId;

    explicit PointBody2D(const PointBodyDef& def);

    void ApplyForce(const Dia::Maths::Vector2D& force)     override;
    void ApplyImpulse(const Dia::Maths::Vector2D& impulse) override;
    void ClearForces()                                      override;

    void SetVelocity(const Dia::Maths::Vector2D& vel)      override;
    const Dia::Maths::Vector2D& GetVelocity()         const override;

    float GetLinearDamping() const;

    Dia::Geometry2D::Transform*       GetTransform()       override;
    const Dia::Geometry2D::Transform* GetTransform() const override;

    const Dia::Geometry2D::Circle*        GetCircleShape() const override;
    const Dia::Geometry2D::ConvexPolygon* GetPolyShape()   const override;

    const Dia::Maths::Vector2D& GetForceAccum() const;

private:
    Dia::Geometry2D::Transform           mTransform;
    float                                mLinearDamping;
    Dia::Maths::Vector2D                 mVelocity;
    Dia::Maths::Vector2D                 mForceAccum;
    Dia::Geometry2D::Circle              mCircleShape;
    Dia::Geometry2D::ConvexPolygon       mPolyShape;
};

} // namespace Dia::RigidBody2D
