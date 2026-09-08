////////////////////////////////////////////////////////////////////////////////
// Filename: ShapePickable.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaGeometry2DPicking/Adapters/ShapePickable.h"
#include <DiaGeometry2D/Shapes/Circle.h>
#include <DiaGeometry2D/Shapes/AARect.h>
#include <DiaGeometry2D/Shapes/ConvexPolygon.h>

namespace Dia::Geometry2DPicking {

ShapePickable::ShapePickable(const Dia::Geometry2D::Circle* circle,
                             Dia::Core::StringCRC id, unsigned int objectIdx,
                             int priority, Dia::Picking::PickLayer layer)
    : mShapeKind(ShapeKind::kCircle)
    , mId(id), mObjectIdx(objectIdx), mPriority(priority), mLayer(layer)
{
    mShape.circle = circle;
}

ShapePickable::ShapePickable(const Dia::Geometry2D::AARect* rect,
                             Dia::Core::StringCRC id, unsigned int objectIdx,
                             int priority, Dia::Picking::PickLayer layer)
    : mShapeKind(ShapeKind::kAARect)
    , mId(id), mObjectIdx(objectIdx), mPriority(priority), mLayer(layer)
{
    mShape.rect = rect;
}

ShapePickable::ShapePickable(const Dia::Geometry2D::ConvexPolygon* poly,
                             Dia::Core::StringCRC id, unsigned int objectIdx,
                             int priority, Dia::Picking::PickLayer layer)
    : mShapeKind(ShapeKind::kConvexPolygon)
    , mId(id), mObjectIdx(objectIdx), mPriority(priority), mLayer(layer)
{
    // ConvexPolygon must be CCW-wound for point-in-polygon test to work correctly
    DIA_ASSERT(poly != nullptr, "ShapePickable: ConvexPolygon pointer must not be null");
    mShape.poly = poly;
}

bool ShapePickable::HitTest(const Dia::Maths::Vector2D& worldPos) const
{
    switch (mShapeKind)
    {
    case ShapeKind::kCircle:
        return mShape.circle && mShape.circle->IsIntersecting(worldPos).IsIntersecting();
    case ShapeKind::kAARect:
        return mShape.rect && mShape.rect->IsIntersecting(worldPos).IsIntersecting();
    case ShapeKind::kConvexPolygon:
    {
        if (!mShape.poly) return false;
        const int n = mShape.poly->GetVertexCount();
        if (n < 3) return false;
        // Point-in-convex-polygon: all cross products same sign (CCW winding)
        for (int i = 0; i < n; ++i)
        {
            const Dia::Maths::Vector2D& a = mShape.poly->GetVertex(i);
            const Dia::Maths::Vector2D& b = mShape.poly->GetVertex((i + 1) % n);
            const float cross = (b.x - a.x) * (worldPos.y - a.y)
                              - (b.y - a.y) * (worldPos.x - a.x);
            if (cross < 0.0f) return false;
        }
        return true;
    }
    }
    return false;
}

bool ShapePickable::OverlapsRect(const Dia::Geometry2D::AARect& worldRect) const
{
    switch (mShapeKind)
    {
    case ShapeKind::kCircle:
        return mShape.circle && worldRect.IsIntersecting(*mShape.circle).IsIntersecting();
    case ShapeKind::kAARect:
    {
        if (!mShape.rect) return false;
        const Dia::Maths::Vector2D& aMin = mShape.rect->GetBottomLeft();
        const Dia::Maths::Vector2D& aMax = mShape.rect->GetTopRight();
        const Dia::Maths::Vector2D& bMin = worldRect.GetBottomLeft();
        const Dia::Maths::Vector2D& bMax = worldRect.GetTopRight();
        return !(aMax.x < bMin.x || aMin.x > bMax.x ||
                 aMax.y < bMin.y || aMin.y > bMax.y);
    }
    case ShapeKind::kConvexPolygon:
        if (!mShape.poly) return false;
        {
            // Check if any polygon vertex is inside the rect
            for (int i = 0; i < mShape.poly->GetVertexCount(); ++i)
                if (worldRect.IsIntersecting(mShape.poly->GetVertex(i)).IsIntersecting())
                    return true;
            // Check if any rect corner is inside the polygon
            const Dia::Maths::Vector2D& bl = worldRect.GetBottomLeft();
            const Dia::Maths::Vector2D& tr = worldRect.GetTopRight();
            Dia::Maths::Vector2D corners[4] = {
                bl,
                Dia::Maths::Vector2D(tr.x, bl.y),
                tr,
                Dia::Maths::Vector2D(bl.x, tr.y)
            };
            for (int c = 0; c < 4; ++c)
                if (HitTest(corners[c])) return true;
        }
        return false;
    }
    return false;
}

bool ShapePickable::TryPick(const Dia::Maths::Vector2D& worldPos, PickHit2D& out) const
{
    if (!HitTest(worldPos)) return false;

    out.pickableId = mId;
    out.kind       = PickHit2D::Kind::kObject;
    out.priority   = mPriority;
    out.worldPos   = worldPos;
    out.objectIdx  = mObjectIdx;
    return true;
}

void ShapePickable::PickArea(const Dia::Geometry2D::AARect& worldRect,
                             Dia::Core::Containers::DynamicArrayC<PickHit2D, kMaxAreaHits>& out) const
{
    if (!OverlapsRect(worldRect) || out.IsFull()) return;

    Dia::Maths::Vector2D center(0.0f, 0.0f);
    switch (mShapeKind)
    {
    case ShapeKind::kCircle:
        if (mShape.circle) center = mShape.circle->GetCenter();
        break;
    case ShapeKind::kAARect:
        if (mShape.rect) center = mShape.rect->CalculateCenter();
        break;
    case ShapeKind::kConvexPolygon:
        if (mShape.poly) center = mShape.poly->CalculateCenter();
        break;
    }

    PickHit2D hit;
    hit.pickableId = mId;
    hit.kind       = PickHit2D::Kind::kObject;
    hit.priority   = mPriority;
    hit.worldPos   = center;
    hit.objectIdx  = mObjectIdx;
    out.Add(hit);
}

} // namespace Dia::Geometry2DPicking
