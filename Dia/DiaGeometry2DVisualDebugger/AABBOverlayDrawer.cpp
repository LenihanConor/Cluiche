////////////////////////////////////////////////////////////////////////////////
// Filename: AABBOverlayDrawer.cpp
// Feature spec: docs/specs/features/dia/diavisualdebugger/drawer-boundary-consistency.md
////////////////////////////////////////////////////////////////////////////////
#include "DiaGeometry2DVisualDebugger/AABBOverlayDrawer.h"

#ifdef DIA_DEBUG

#include <DiaGeometry2D/Shapes/Circle.h>
#include <DiaGeometry2D/Shapes/AARect.h>
#include <DiaGeometry2D/Shapes/OORect.h>
#include <DiaGeometry2D/Shapes/Line.h>
#include <DiaGeometry2D/Shapes/Triangle.h>
#include <DiaGeometry2D/Shapes/ConvexPolygon.h>
#include <DiaGeometry2D/Shapes/Capsule.h>
#include <DiaGraphics/Frame/FrameData.h>
#include <DiaVisualDebugger/DebugLayerNames.h>
#include <DiaGraphics/Misc/RGBA.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <imgui.h>
#include <algorithm>

namespace Dia::Geometry2DVisualDebugger
{

static const Dia::Graphics::RGBA kAABBColour(255, 200, 50, 180);

AABBOverlayDrawer::AABBOverlayDrawer(const Dia::Debug::DebugLayerManager& manager)
    : mManager(manager)
{}

Dia::Core::StringCRC AABBOverlayDrawer::GetLayerName() const
{
    return Dia::Debug::LayerNames::kGeoAABB;
}

void AABBOverlayDrawer::Submit(float minX, float minY, float maxX, float maxY)
{
    if (mPending.IsFull()) return;
    AABBEntry e{ minX, minY, maxX, maxY };
    mPending.Add(e);
}

void AABBOverlayDrawer::SubmitCircle(const Dia::Geometry2D::Circle& shape)
{
    const float r = shape.GetRadius();
    Submit(shape.GetCenter().x - r, shape.GetCenter().y - r,
           shape.GetCenter().x + r, shape.GetCenter().y + r);
}

void AABBOverlayDrawer::SubmitAARect(const Dia::Geometry2D::AARect& shape)
{
    Submit(shape.GetBottomLeft().x, shape.GetBottomLeft().y,
           shape.GetTopRight().x,   shape.GetTopRight().y);
}

void AABBOverlayDrawer::SubmitOORect(const Dia::Geometry2D::OORect& shape)
{
    using P = Dia::Geometry2D::OORect;
    const Dia::Maths::Vector2D pts[4] = {
        shape.GetPt(P::kPt0), shape.GetPt(P::kPt1),
        shape.GetPt(P::kPt2), shape.GetPt(P::kPt3) };
    float minX = pts[0].x, maxX = minX, minY = pts[0].y, maxY = minY;
    for (int i = 1; i < 4; ++i)
    {
        minX = std::min(minX, pts[i].x); maxX = std::max(maxX, pts[i].x);
        minY = std::min(minY, pts[i].y); maxY = std::max(maxY, pts[i].y);
    }
    Submit(minX, minY, maxX, maxY);
}

void AABBOverlayDrawer::SubmitLine(const Dia::Geometry2D::Line& shape)
{
    Submit(std::min(shape.GetPt1().x, shape.GetPt2().x),
           std::min(shape.GetPt1().y, shape.GetPt2().y),
           std::max(shape.GetPt1().x, shape.GetPt2().x),
           std::max(shape.GetPt1().y, shape.GetPt2().y));
}

void AABBOverlayDrawer::SubmitTriangle(const Dia::Geometry2D::Triangle& shape)
{
    float minX = shape.GetPt(0).x, maxX = minX;
    float minY = shape.GetPt(0).y, maxY = minY;
    for (int i = 1; i < 3; ++i)
    {
        minX = std::min(minX, shape.GetPt(i).x); maxX = std::max(maxX, shape.GetPt(i).x);
        minY = std::min(minY, shape.GetPt(i).y); maxY = std::max(maxY, shape.GetPt(i).y);
    }
    Submit(minX, minY, maxX, maxY);
}

void AABBOverlayDrawer::SubmitConvexPoly(const Dia::Geometry2D::ConvexPolygon& shape)
{
    if (shape.GetVertexCount() == 0) return;
    float minX = shape.GetVertex(0).x, maxX = minX;
    float minY = shape.GetVertex(0).y, maxY = minY;
    for (int i = 1; i < shape.GetVertexCount(); ++i)
    {
        minX = std::min(minX, shape.GetVertex(i).x); maxX = std::max(maxX, shape.GetVertex(i).x);
        minY = std::min(minY, shape.GetVertex(i).y); maxY = std::max(maxY, shape.GetVertex(i).y);
    }
    Submit(minX, minY, maxX, maxY);
}

void AABBOverlayDrawer::SubmitCapsule(const Dia::Geometry2D::Capsule& shape)
{
    const float r = shape.GetRadius();
    Submit(std::min(shape.GetPoint1().x, shape.GetPoint2().x) - r,
           std::min(shape.GetPoint1().y, shape.GetPoint2().y) - r,
           std::max(shape.GetPoint1().x, shape.GetPoint2().x) + r,
           std::max(shape.GetPoint1().y, shape.GetPoint2().y) + r);
}

#pragma warning(push)
#pragma warning(disable: 6262)
void AABBOverlayDrawer::Draw(Dia::Graphics::FrameData& frameData)
{
    Dia::Core::Containers::DynamicArrayC<AABBEntry, kMaxAABBs> pending;
    pending.Swap(mPending);

    if (!IsEnabled()) return;

    for (unsigned int i = 0; i < pending.Size(); ++i)
    {
        const AABBEntry& e = pending[i];
        frameData.RequestDrawRect(
            Dia::Maths::Vector2D(e.minX, e.minY),
            Dia::Maths::Vector2D(e.maxX, e.maxY),
            kAABBColour);
    }
}
#pragma warning(pop)

void AABBOverlayDrawer::DrawImGui()
{
    ImGui::Text("AABB count: %u", mPending.Size());
}

} // namespace Dia::Geometry2DVisualDebugger

#endif // DIA_DEBUG
