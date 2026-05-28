#ifdef DIA_DEBUG

#include "Modules/TestStages/Drawers/Geometry2DShapesDrawer.h"

#include <DiaGeometry2DVisualDebugger/ShapeDrawer.h>
#include <DiaGeometry2DVisualDebugger/ArcDrawHelper.h>
#include <DiaGeometry2DVisualDebugger/SectorDrawHelper.h>
#include <DiaGeometry2DVisualDebugger/OORectDrawHelper.h>
#include <DiaGeometry2DVisualDebugger/CapsuleDrawHelper.h>
#include <DiaGraphics/Misc/RGBA.h>
#include <DiaGraphics/Frame/FrameData.h>

namespace CluicheTest {

static const Dia::Graphics::RGBA kWhite(255, 255, 255, 255);
static constexpr float               kRayLen = 150.0f;

Geometry2DShapesDrawer::Geometry2DShapesDrawer(
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
{}

Dia::Core::StringCRC Geometry2DShapesDrawer::GetLayerName() const
{
    return Dia::Core::StringCRC("geo2d.shapes");
}

void Geometry2DShapesDrawer::Draw(Dia::Graphics::FrameData& frameData)
{
    Dia::Geometry2DVisualDebugger::ShapeDrawer drawer(mManager);

    drawer.SubmitCircle    (mCircle,     kWhite);
    drawer.SubmitAARect    (mAARect,     kWhite);
    drawer.SubmitLine      (mLine,       kWhite);
    drawer.SubmitRay       (mRay,        kRayLen, kWhite);
    drawer.SubmitTriangle  (mTriangle,   kWhite);
    drawer.SubmitConvexPoly(mConvexPoly, kWhite);

    drawer.SubmitConvexPoly(Dia::Geometry2DVisualDebugger::OORectToConvexPolygon(mOORect),    kWhite);
    drawer.SubmitConvexPoly(Dia::Geometry2DVisualDebugger::CapsuleToConvexPolygon(mCapsule),  kWhite);
    drawer.SubmitConvexPoly(Dia::Geometry2DVisualDebugger::ArcToConvexPolygon(mArc),          kWhite);
    drawer.SubmitConvexPoly(Dia::Geometry2DVisualDebugger::SectorToConvexPolygon(mSector),    kWhite);

    drawer.Draw(frameData);
}

} // namespace CluicheTest

#endif // DIA_DEBUG

