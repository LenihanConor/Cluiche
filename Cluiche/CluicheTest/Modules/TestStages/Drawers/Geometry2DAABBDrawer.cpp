#ifdef DIA_DEBUG

#include "Modules/TestStages/Drawers/Geometry2DAABBDrawer.h"

#include <DiaGeometry2DVisualDebugger/AABBOverlayDrawer.h>
#include <DiaCore/DebugDraw/IDebugDraw.h>

namespace CluicheTest {

Geometry2DAABBDrawer::Geometry2DAABBDrawer(
    const Dia::Geometry2D::Circle&        circle,
    const Dia::Geometry2D::AARect&        aaRect,
    const Dia::Geometry2D::OORect&        ooRect,
    const Dia::Geometry2D::Line&          line,
    const Dia::Geometry2D::Ray&           ray,
    const Dia::Geometry2D::Triangle&      triangle,
    const Dia::Geometry2D::Capsule&       capsule,
    const Dia::Geometry2D::ConvexPolygon& convexPoly,
    const Dia::Geometry2D::Sector&        sector,
    const Dia::Debug::DebugLayerManager&  mgr)
    : mCircle(circle), mAARect(aaRect), mOORect(ooRect)
    , mLine(line), mRay(ray), mTriangle(triangle)
    , mCapsule(capsule), mConvexPoly(convexPoly)
    , mSector(sector)
    , mManager(mgr)
{
    SetEnabled(false);
}

Dia::Core::StringCRC Geometry2DAABBDrawer::GetLayerName() const
{
    return Dia::Core::StringCRC("geometry2d.aabbs");
}

#pragma warning(push)
#pragma warning(disable: 6262)
void Geometry2DAABBDrawer::Draw(Dia::Core::IDebugDraw& draw)
{
    Dia::Geometry2DVisualDebugger::AABBOverlayDrawer drawer(mManager);

    drawer.SubmitCircle    (mCircle);
    drawer.SubmitAARect    (mAARect);
    drawer.SubmitOORect    (mOORect);
    drawer.SubmitLine      (mLine);
    drawer.SubmitTriangle  (mTriangle);
    drawer.SubmitConvexPoly(mConvexPoly);
    drawer.SubmitCapsule   (mCapsule);

    drawer.Draw(draw);
}
#pragma warning(pop)

} // namespace CluicheTest

#endif // DIA_DEBUG
