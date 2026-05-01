#include "DiaRigidBody2D/Triggers/TriggerVolume2D.h"

#include "DiaCore/Core/Assert.h"

namespace Dia::RigidBody2D {

TriggerVolume2D::TriggerVolume2D(const TriggerVolumeDef& def)
    : mTransform(def.transform ? *def.transform : Dia::Geometry2D::Transform())
    , mCircleShape(def.circleShape ? *def.circleShape : Dia::Geometry2D::Circle())
    , mPolyShape(def.polyShape ? *def.polyShape : Dia::Geometry2D::ConvexPolygon())
{
    DIA_ASSERT((def.layer & (def.layer - 1)) == 0, "Layer must have exactly one bit set");
    DIA_ASSERT(!(def.circleShape && def.polyShape), "Trigger cannot have both circle and polygon shape");
    DIA_ASSERT(def.circleShape || def.polyShape, "Trigger must have a shape");

    mId    = def.id;
    mLayer = def.layer;
    mMask  = def.mask;

    if (def.circleShape)
        mShapeKind = ShapeKind::kCircle;
    else
        mShapeKind = ShapeKind::kPoly;
}

const Dia::Geometry2D::Circle* TriggerVolume2D::GetCircleShape() const
{
    return mShapeKind == ShapeKind::kCircle ? &mCircleShape : nullptr;
}

const Dia::Geometry2D::ConvexPolygon* TriggerVolume2D::GetPolyShape() const
{
    return mShapeKind == ShapeKind::kPoly ? &mPolyShape : nullptr;
}

} // namespace Dia::RigidBody2D
