#include "DiaRigidBody2D/Bodies/Body2DBase.h"

#include "DiaCore/Core/Assert.h"

namespace Dia::RigidBody2D {

Body2DBase::Body2DBase(const Body2DDef& def)
{
    DIA_ASSERT((def.layer & (def.layer - 1)) == 0, "Layer must have exactly one bit set");
    DIA_ASSERT(!(def.circleShape && def.polyShape), "Body cannot have both circle and polygon shape");

    mId            = def.id;
    mType          = def.type;
    mMass          = def.mass;
    mRestitution   = def.restitution;
    mFriction      = def.friction;
    mAllowSleeping = def.allowSleeping;
    mLayer         = def.layer;
    mMask          = def.mask;

    if (def.circleShape)
        mShapeKind = ShapeKind::kCircle;
    else if (def.polyShape)
        mShapeKind = ShapeKind::kPoly;
    else
        mShapeKind = ShapeKind::kNone;

    if (mType == BodyType::kDynamic && mMass > 0.0f)
        mInvMass = 1.0f / mMass;
    else
        mInvMass = 0.0f;
}

} // namespace Dia::RigidBody2D
