////////////////////////////////////////////////////////////////////////////////
// Filename: ShapePickable.h
// Description: IPickable2D adapter for individual 2D shapes (Circle, AARect,
//              ConvexPolygon). One class with a Kind switch — not one per shape.
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaGeometry2DPicking/IPickable2D.h>
#include <DiaGeometry2DPicking/PickHit2D.h>
#include <DiaGeometry2D/Shapes/Circle.h>
#include <DiaGeometry2D/Shapes/AARect.h>
#include <DiaGeometry2D/Shapes/ConvexPolygon.h>

namespace Dia::Geometry2DPicking {

class ShapePickable : public IPickable2D
{
public:
    enum class ShapeKind { kCircle, kAARect, kConvexPolygon };

    // Construct for Circle
    ShapePickable(const Dia::Geometry2D::Circle* circle,
                  Dia::Core::StringCRC id,
                  unsigned int objectIdx = 0,
                  int priority = 0,
                  Dia::Picking::PickLayer layer = Dia::Picking::PickLayer::kDefault);

    // Construct for AARect
    ShapePickable(const Dia::Geometry2D::AARect* rect,
                  Dia::Core::StringCRC id,
                  unsigned int objectIdx = 0,
                  int priority = 0,
                  Dia::Picking::PickLayer layer = Dia::Picking::PickLayer::kDefault);

    // Construct for ConvexPolygon
    ShapePickable(const Dia::Geometry2D::ConvexPolygon* poly,
                  Dia::Core::StringCRC id,
                  unsigned int objectIdx = 0,
                  int priority = 0,
                  Dia::Picking::PickLayer layer = Dia::Picking::PickLayer::kDefault);

    Dia::Core::StringCRC    GetPickableId() const override { return mId; }
    int                     GetPriority()   const override { return mPriority; }
    Dia::Picking::PickLayer GetLayer()      const override { return mLayer; }

    bool TryPick(const Dia::Maths::Vector2D& worldPos, PickHit2D& out) const override;
    void PickArea(const Dia::Geometry2D::AARect& worldRect,
                  Dia::Core::Containers::DynamicArrayC<PickHit2D, kMaxAreaHits>& out) const override;

private:
    bool HitTest(const Dia::Maths::Vector2D& worldPos) const;
    bool OverlapsRect(const Dia::Geometry2D::AARect& worldRect) const;

    ShapeKind mShapeKind;
    union {
        const Dia::Geometry2D::Circle*       circle;
        const Dia::Geometry2D::AARect*       rect;
        const Dia::Geometry2D::ConvexPolygon* poly;
    } mShape;

    Dia::Core::StringCRC    mId;
    unsigned int            mObjectIdx;
    int                     mPriority;
    Dia::Picking::PickLayer mLayer;
};

} // namespace Dia::Geometry2DPicking
