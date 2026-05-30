#ifdef DIA_DEBUG

#include "Modules/TestStages/Drawers/Geometry2DAABBDrawer.h"

#include <DiaGeometry2DVisualDebugger/ShapeDrawer.h>
#include <DiaGeometry2D/Shapes/AARect.h>
#include <DiaGraphics/Misc/RGBA.h>
#include <DiaGraphics/Frame/FrameData.h>
#include <algorithm>

namespace CluicheTest {

static const Dia::Graphics::RGBA kAABBColour(255, 200, 50, 180);

Geometry2DAABBDrawer::Geometry2DAABBDrawer(
    const Dia::Geometry2D::Circle&        circle,
    const Dia::Geometry2D::AARect&        aaRect,
    const Dia::Geometry2D::OORect&        ooRect,
    const Dia::Geometry2D::Line&          line,
    const Dia::Geometry2D::Ray&           ray,
    const Dia::Geometry2D::Triangle&      triangle,
    const Dia::Geometry2D::Capsule&       capsule,
    const Dia::Geometry2D::ConvexPolygon& convexPoly,
    const Dia::Geometry2D::Arc&           arc,
    const Dia::Geometry2D::Sector&        sector,
    const Dia::Debug::DebugLayerManager&  mgr)
    : mCircle(circle), mAARect(aaRect), mOORect(ooRect)
    , mLine(line), mRay(ray), mTriangle(triangle)
    , mCapsule(capsule), mConvexPoly(convexPoly)
    , mArc(arc), mSector(sector)
    , mManager(mgr)
{
    SetEnabled(false); // off by default per spec
}

Dia::Core::StringCRC Geometry2DAABBDrawer::GetLayerName() const
{
    return Dia::Core::StringCRC("geo2d.aabbs");
}

static Dia::Geometry2D::AARect MakeAABB(float minX, float minY, float maxX, float maxY)
{
    return Dia::Geometry2D::AARect(
        Dia::Maths::Vector2D(minX, minY),
        Dia::Maths::Vector2D(maxX, maxY));
}

#pragma warning(push)
#pragma warning(disable: 6262)  // ShapeDrawer::mPending is intentionally stack-allocated
void Geometry2DAABBDrawer::Draw(Dia::Graphics::FrameData& frameData)
{
    Dia::Geometry2DVisualDebugger::ShapeDrawer drawer(mManager);

    // Circle
    {
        const float r = mCircle.GetRadius();
        drawer.SubmitAARect(MakeAABB(
            mCircle.GetCenter().X() - r, mCircle.GetCenter().Y() - r,
            mCircle.GetCenter().X() + r, mCircle.GetCenter().Y() + r), kAABBColour);
    }

    // AARect — is its own AABB
    drawer.SubmitAARect(mAARect, kAABBColour);

    // OORect — 4 corners
    {
        using P = Dia::Geometry2D::OORect;
        const Dia::Maths::Vector2D pts[4] = {
            mOORect.GetPt(P::kPt0), mOORect.GetPt(P::kPt1),
            mOORect.GetPt(P::kPt2), mOORect.GetPt(P::kPt3) };
        float minX = pts[0].X(), maxX = minX, minY = pts[0].Y(), maxY = minY;
        for (int i = 1; i < 4; ++i)
        {
            minX = std::min(minX, pts[i].X()); maxX = std::max(maxX, pts[i].X());
            minY = std::min(minY, pts[i].Y()); maxY = std::max(maxY, pts[i].Y());
        }
        drawer.SubmitAARect(MakeAABB(minX, minY, maxX, maxY), kAABBColour);
    }

    // Line
    {
        drawer.SubmitAARect(MakeAABB(
            std::min(mLine.GetPt1().X(), mLine.GetPt2().X()),
            std::min(mLine.GetPt1().Y(), mLine.GetPt2().Y()),
            std::max(mLine.GetPt1().X(), mLine.GetPt2().X()),
            std::max(mLine.GetPt1().Y(), mLine.GetPt2().Y())), kAABBColour);
    }

    // ConvexPolygon
    {
        float minX = mConvexPoly.GetVertex(0).X(), maxX = minX;
        float minY = mConvexPoly.GetVertex(0).Y(), maxY = minY;
        for (int i = 1; i < mConvexPoly.GetVertexCount(); ++i)
        {
            minX = std::min(minX, mConvexPoly.GetVertex(i).X());
            maxX = std::max(maxX, mConvexPoly.GetVertex(i).X());
            minY = std::min(minY, mConvexPoly.GetVertex(i).Y());
            maxY = std::max(maxY, mConvexPoly.GetVertex(i).Y());
        }
        drawer.SubmitAARect(MakeAABB(minX, minY, maxX, maxY), kAABBColour);
    }

    // Capsule
    {
        const float r = mCapsule.GetRadius();
        drawer.SubmitAARect(MakeAABB(
            std::min(mCapsule.GetPoint1().X(), mCapsule.GetPoint2().X()) - r,
            std::min(mCapsule.GetPoint1().Y(), mCapsule.GetPoint2().Y()) - r,
            std::max(mCapsule.GetPoint1().X(), mCapsule.GetPoint2().X()) + r,
            std::max(mCapsule.GetPoint1().Y(), mCapsule.GetPoint2().Y()) + r), kAABBColour);
    }

    drawer.Draw(frameData);
}
#pragma warning(pop)

} // namespace CluicheTest

#endif // DIA_DEBUG


