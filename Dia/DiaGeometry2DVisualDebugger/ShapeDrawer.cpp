////////////////////////////////////////////////////////////////////////////////
// Filename: ShapeDrawer.cpp
// Feature spec: docs/specs/features/dia/diavisualdebugger/geometry2d-visual-debugger-stack.md
////////////////////////////////////////////////////////////////////////////////
#include "DiaGeometry2DVisualDebugger/ShapeDrawer.h"

#ifdef DIA_DEBUG

#include <DiaGeometry2D/Shapes/Circle.h>
#include <DiaGeometry2D/Shapes/AARect.h>
#include <DiaGeometry2D/Shapes/Line.h>
#include <DiaGeometry2D/Shapes/Ray.h>
#include <DiaGeometry2D/Shapes/Triangle.h>
#include <DiaGeometry2D/Shapes/ConvexPolygon.h>
#include <DiaGraphics/Frame/FrameData.h>
#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaVisualDebugger/DebugLayerNames.h>
#include <DiaMaths/Vector/Vector2D.h>

namespace Dia::Geometry2DVisualDebugger
{

ShapeDrawer::ShapeDrawer(const Dia::Debug::DebugLayerManager& manager)
    : mManager(manager)
{}

Dia::Core::StringCRC ShapeDrawer::GetLayerName() const
{
    return Dia::Debug::LayerNames::kGeoShapes;
}

// ---------------------------------------------------------------------------

void ShapeDrawer::SubmitCircle(const Dia::Geometry2D::Circle& shape, Dia::Graphics::RGBA colour)
{
    if (mPending.IsFull()) return;
    ShapeEntry e;
    e.type        = ShapeType::Circle;
    e.colour      = colour;
    e.circle.cx   = shape.GetCenter().x;
    e.circle.cy   = shape.GetCenter().y;
    e.circle.radius = shape.GetRadius();
    mPending.Add(e);
}

void ShapeDrawer::SubmitAARect(const Dia::Geometry2D::AARect& shape, Dia::Graphics::RGBA colour)
{
    if (mPending.IsFull()) return;
    ShapeEntry e;
    e.type         = ShapeType::AARect;
    e.colour       = colour;
    e.aarect.minX  = shape.GetBottomLeft().x;
    e.aarect.minY  = shape.GetBottomLeft().y;
    e.aarect.maxX  = shape.GetTopRight().x;
    e.aarect.maxY  = shape.GetTopRight().y;
    mPending.Add(e);
}

void ShapeDrawer::SubmitLine(const Dia::Geometry2D::Line& shape, Dia::Graphics::RGBA colour)
{
    if (mPending.IsFull()) return;
    ShapeEntry e;
    e.type    = ShapeType::Line;
    e.colour  = colour;
    e.line.x1 = shape.GetPt1().x;
    e.line.y1 = shape.GetPt1().y;
    e.line.x2 = shape.GetPt2().x;
    e.line.y2 = shape.GetPt2().y;
    mPending.Add(e);
}

void ShapeDrawer::SubmitRay(const Dia::Geometry2D::Ray& shape, float displayLength,
                            Dia::Graphics::RGBA colour)
{
    if (mPending.IsFull()) return;
    ShapeEntry e;
    e.type               = ShapeType::Ray;
    e.colour             = colour;
    e.ray.ox             = shape.GetOrigin().x;
    e.ray.oy             = shape.GetOrigin().y;
    e.ray.dx             = shape.GetDirection().x;
    e.ray.dy             = shape.GetDirection().y;
    e.ray.displayLength  = displayLength;
    mPending.Add(e);
}

void ShapeDrawer::SubmitTriangle(const Dia::Geometry2D::Triangle& shape, Dia::Graphics::RGBA colour)
{
    if (mPending.IsFull()) return;
    ShapeEntry e;
    e.type       = ShapeType::Triangle;
    e.colour     = colour;
    e.triangle.x0 = shape.GetPt(0).x;
    e.triangle.y0 = shape.GetPt(0).y;
    e.triangle.x1 = shape.GetPt(1).x;
    e.triangle.y1 = shape.GetPt(1).y;
    e.triangle.x2 = shape.GetPt(2).x;
    e.triangle.y2 = shape.GetPt(2).y;
    mPending.Add(e);
}

void ShapeDrawer::SubmitConvexPoly(const Dia::Geometry2D::ConvexPolygon& shape, Dia::Graphics::RGBA colour)
{
    if (mPending.IsFull()) return;
    ShapeEntry e;
    e.type       = ShapeType::ConvexPoly;
    e.colour     = colour;
    const int n  = shape.GetVertexCount();
    e.poly.count = (n <= ConvexPolyData::kMaxVerts) ? n : ConvexPolyData::kMaxVerts;
    for (int i = 0; i < e.poly.count; ++i)
    {
        e.poly.vx[i] = shape.GetVertex(i).x;
        e.poly.vy[i] = shape.GetVertex(i).y;
    }
    mPending.Add(e);
}

// ---------------------------------------------------------------------------

void ShapeDrawer::Draw(Dia::Graphics::FrameData& frameData)
{
    // Clear pending at the start so next frame starts fresh
    const auto pendingCopy = mPending;
    mPending.RemoveAll();

    if (!IsEnabled()) return;

    const float scale = mManager.GetDebugScale();

    for (unsigned int i = 0; i < pendingCopy.Size(); ++i)
    {
        const ShapeEntry& e = pendingCopy[i];
        switch (e.type)
        {
            case ShapeType::Circle:
            {
                const Dia::Maths::Vector2D center(e.circle.cx, e.circle.cy);
                // Radius is a world-space extent — drawn as-is; scale does not distort
                frameData.RequestDraw(center, e.circle.radius, e.colour);
                break;
            }
            case ShapeType::AARect:
            {
                const Dia::Maths::Vector2D min(e.aarect.minX, e.aarect.minY);
                const Dia::Maths::Vector2D max(e.aarect.maxX, e.aarect.maxY);
                frameData.RequestDrawRect(min, max, e.colour);
                break;
            }
            case ShapeType::Line:
            {
                const Dia::Maths::Vector2D pt1(e.line.x1, e.line.y1);
                const Dia::Maths::Vector2D pt2(e.line.x2, e.line.y2);
                frameData.RequestDraw(pt1, pt2, e.colour);
                break;
            }
            case ShapeType::Ray:
            {
                const Dia::Maths::Vector2D origin(e.ray.ox, e.ray.oy);
                const Dia::Maths::Vector2D dir(e.ray.dx, e.ray.dy);
                // displayLength is a decorative length — multiply by debugScale
                frameData.RequestDrawRay(origin, dir, e.ray.displayLength * scale, e.colour);
                break;
            }
            case ShapeType::Triangle:
            {
                const Dia::Maths::Vector2D p0(e.triangle.x0, e.triangle.y0);
                const Dia::Maths::Vector2D p1(e.triangle.x1, e.triangle.y1);
                const Dia::Maths::Vector2D p2(e.triangle.x2, e.triangle.y2);
                frameData.RequestDraw(p0, p1, p2, e.colour);
                break;
            }
            case ShapeType::ConvexPoly:
            {
                const int n = e.poly.count;
                for (int j = 0; j < n; ++j)
                {
                    const Dia::Maths::Vector2D vi(e.poly.vx[j],         e.poly.vy[j]);
                    const Dia::Maths::Vector2D vj(e.poly.vx[(j+1) % n], e.poly.vy[(j+1) % n]);
                    frameData.RequestDraw(vi, vj, e.colour);
                }
                break;
            }
        }
    }
}

} // namespace Dia::Geometry2DVisualDebugger

#endif // DIA_DEBUG
