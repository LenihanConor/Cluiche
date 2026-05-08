#pragma once

#include "DiaRigidBody2D/Bodies/BodyType.h"
#include "DiaCore/CRC/StringCRC.h"
#include "DiaCore/Core/Assert.h"
#include "DiaGeometry2D/Transform/Transform.h"
#include "DiaGeometry2D/Shapes/Circle.h"
#include "DiaGeometry2D/Shapes/ConvexPolygon.h"
#include "DiaMaths/Vector/Vector2D.h"

namespace Dia::RigidBody2D {

struct TriggerVolumeDef {
    Dia::Core::StringCRC              id;
    const Dia::Geometry2D::Transform*     transform   = nullptr;
    const Dia::Geometry2D::Circle*        circleShape = nullptr;
    const Dia::Geometry2D::ConvexPolygon* polyShape   = nullptr;
    uint32_t layer = 0x1;
    uint32_t mask  = 0xFFFFFFFF;
};

class TriggerVolume2D {
public:
    explicit TriggerVolume2D(const TriggerVolumeDef& def);

    const Dia::Core::StringCRC& GetId() const { return mId; }

    uint32_t GetUniqueId()              const { return mUniqueId; }
    void     SetUniqueId(uint32_t uid)        { mUniqueId = uid; }

    ShapeKind GetShapeKind() const { return mShapeKind; }

    Dia::Geometry2D::Transform*       GetTransform()       { return &mTransform; }
    const Dia::Geometry2D::Transform* GetTransform() const { return &mTransform; }

    const Dia::Geometry2D::Circle*        GetCircleShape() const;
    const Dia::Geometry2D::ConvexPolygon* GetPolyShape()   const;

    uint32_t GetLayer() const { return mLayer; }
    uint32_t GetMask()  const { return mMask; }

    void SetLayer(uint32_t layer)
    {
        DIA_ASSERT((layer & (layer - 1)) == 0, "Layer must have exactly one bit set");
        mLayer = layer;
    }

    void SetMask(uint32_t mask) { mMask = mask; }

private:
    Dia::Core::StringCRC           mId;
    uint32_t                       mUniqueId  = 0;
    ShapeKind                      mShapeKind = ShapeKind::kNone;
    Dia::Geometry2D::Transform     mTransform;
    Dia::Geometry2D::Circle        mCircleShape;
    Dia::Geometry2D::ConvexPolygon mPolyShape;
    uint32_t                       mLayer = 0x1;
    uint32_t                       mMask  = 0xFFFFFFFF;
};

} // namespace Dia::RigidBody2D
