#ifdef DIA_DEBUG

#include "Modules/TestStages/Drawers/Geometry2DShapesDrawer.h"

#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaGeometry2DVisualDebugger/ShapeDrawer.h>
#include <DiaGeometry2DVisualDebugger/SectorDrawHelper.h>
#include <DiaGeometry2D/Shapes/SplineFactory.h>
#include <DiaGeometry2DVisualDebugger/OORectDrawHelper.h>
#include <DiaGeometry2DVisualDebugger/CapsuleDrawHelper.h>
#include <DiaGraphics/Misc/RGBA.h>
#include <DiaCore/DebugDraw/IDebugDraw.h>

namespace CluicheTest {

static const Dia::Graphics::RGBA kWhite(255, 255, 255, 255);
static constexpr float kRayLenShort    = 1.2f;   // *debugScale(50) = 60 world units
static constexpr float kRayLenExtended = 6.0f;   // *debugScale(50) = 300 world units

Geometry2DShapesDrawer::Geometry2DShapesDrawer(
    const Dia::Geometry2D::Circle&        circle,
    const Dia::Geometry2D::AARect&        aaRect,
    const Dia::Geometry2D::OORect&        ooRect,
    const Dia::Geometry2D::Line&          line,
    const Dia::Geometry2D::Ray&           ray,
    const Dia::Geometry2D::Triangle&      triangle,
    const Dia::Geometry2D::Capsule&       capsule,
    const Dia::Geometry2D::ConvexPolygon& convexPoly,
    const Dia::Geometry2D::Sector&        sector,
    const Dia::Geometry2D::Spline&        spline,
    const Dia::Core::Containers::DynamicArrayC<Dia::Geometry2D::AARect, 24>& spatialScatter,
    const Dia::Debug::DebugLayerManager&  mgr)
    : mCircle(circle), mAARect(aaRect), mOORect(ooRect)
    , mLine(line), mRay(ray), mTriangle(triangle)
    , mCapsule(capsule), mConvexPoly(convexPoly)
    , mSector(sector)
    , mSpline(spline)
    , mSpatialScatter(spatialScatter)
    , mManager(mgr)
{}

Dia::Core::StringCRC Geometry2DShapesDrawer::GetLayerName() const
{
    return Dia::Core::StringCRC("geometry2d.shapes");
}

#pragma warning(push)
#pragma warning(disable: 6262)  // ShapeDrawer::mPending is intentionally stack-allocated
void Geometry2DShapesDrawer::Draw(Dia::Core::IDebugDraw& draw)
{
    Dia::Geometry2DVisualDebugger::ShapeDrawer drawer(mManager);

    drawer.SubmitCircle    (mCircle,     kWhite);
    drawer.SubmitAARect    (mAARect,     kWhite);
    drawer.SubmitLine      (mLine,       kWhite);
    drawer.SubmitRay       (mRay,        mExtendedRay ? kRayLenExtended : kRayLenShort, kWhite);
    drawer.SubmitTriangle  (mTriangle,   kWhite);
    drawer.SubmitConvexPoly(mConvexPoly, kWhite);

    drawer.SubmitConvexPoly(Dia::Geometry2DVisualDebugger::OORectToConvexPolygon(mOORect),    kWhite);
    drawer.SubmitConvexPoly(Dia::Geometry2DVisualDebugger::CapsuleToConvexPolygon(mCapsule),  kWhite);
    drawer.SubmitConvexPoly(Dia::Geometry2DVisualDebugger::SectorToConvexPolygon(mSector),    kWhite);

    drawer.SubmitSpline(mSpline, 32, kWhite);

    // Spatial scatter shapes — small filled rects inside BVH/Quadtree/Grid bounds
    static const Dia::Graphics::RGBA kScatterColour(247, 200, 126, 200);
    for (unsigned int i = 0; i < mSpatialScatter.Size(); ++i)
        drawer.SubmitAARect(mSpatialScatter[i], kScatterColour);

    drawer.Draw(draw);
}
#pragma warning(pop)

} // namespace CluicheTest

#endif // DIA_DEBUG

