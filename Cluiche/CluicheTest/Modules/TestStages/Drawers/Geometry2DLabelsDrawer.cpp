#ifdef DIA_DEBUG

#include "Modules/TestStages/Drawers/Geometry2DLabelsDrawer.h"

#include <DiaGeometry2DVisualDebugger/ShapeLabelsDrawer.h>
#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaGraphics/Frame/FrameData.h>
#include <DiaGeometry2D/Shapes/Spline.h>

namespace CluicheTest {

static constexpr float kLabelOffsetY = 55.0f;

Geometry2DLabelsDrawer::Geometry2DLabelsDrawer(
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
    const Dia::Debug::DebugLayerManager&  mgr)
    : mCircle(circle), mAARect(aaRect), mOORect(ooRect)
    , mLine(line), mRay(ray), mTriangle(triangle)
    , mCapsule(capsule), mConvexPoly(convexPoly)
    , mSector(sector)
    , mSpline(spline)
    , mManager(mgr)
{}

Dia::Core::StringCRC Geometry2DLabelsDrawer::GetLayerName() const
{
    return Dia::Core::StringCRC("geometry2d.labels");
}

void Geometry2DLabelsDrawer::Draw(Dia::Graphics::FrameData& frameData)
{
    Dia::Geometry2DVisualDebugger::ShapeLabelsDrawer drawer(mManager);

    const auto aaCentre = (mAARect.GetBottomLeft() + mAARect.GetTopRight()) * 0.5f;
    const auto ooCentre = (mOORect.GetPt(0) + mOORect.GetPt(1) + mOORect.GetPt(2) + mOORect.GetPt(3)) * 0.25f;
    const auto lineCentre = (mLine.GetPt1() + mLine.GetPt2()) * 0.5f;
    const auto triCentre = (mTriangle.GetPt(Dia::Geometry2D::Triangle::kPt0)
        + mTriangle.GetPt(Dia::Geometry2D::Triangle::kPt1)
        + mTriangle.GetPt(Dia::Geometry2D::Triangle::kPt2)) * (1.0f / 3.0f);

    Dia::Maths::Vector2D polyCentre(0.0f, 0.0f);
    for (int i = 0; i < mConvexPoly.GetVertexCount(); ++i)
        polyCentre = polyCentre + mConvexPoly.GetVertex(i);
    polyCentre = polyCentre * (1.0f / static_cast<float>(mConvexPoly.GetVertexCount()));

    const Dia::Maths::Vector2D splineMid = mSpline.Evaluate(0.5f);

    drawer.SubmitLabel({ mCircle.GetCenter().x,   mCircle.GetCenter().y   + kLabelOffsetY }, "circle");
    drawer.SubmitLabel({ aaCentre.x,              aaCentre.y              + kLabelOffsetY }, "aa_rect");
    drawer.SubmitLabel({ ooCentre.x,              ooCentre.y              + kLabelOffsetY }, "oo_rect*");
    drawer.SubmitLabel({ lineCentre.x,            lineCentre.y            + kLabelOffsetY }, "line");
    drawer.SubmitLabel({ mRay.GetOrigin().x,      mRay.GetOrigin().y      + kLabelOffsetY }, "ray");
    drawer.SubmitLabel({ triCentre.x,             triCentre.y             + kLabelOffsetY }, "triangle");
    drawer.SubmitLabel({ mCapsule.GetCenter().x,  mCapsule.GetCenter().y  + kLabelOffsetY }, "capsule*");
    drawer.SubmitLabel({ polyCentre.x,            polyCentre.y            + kLabelOffsetY }, "convex_poly");
    drawer.SubmitLabel({ mSector.GetCenter().x,   mSector.GetCenter().y   + kLabelOffsetY }, "sector*");
    drawer.SubmitLabel({ splineMid.x,             splineMid.y             + kLabelOffsetY }, "spline");

    drawer.Draw(frameData);
}

void Geometry2DLabelsDrawer::DrawImGui()
{
    // Font scale now lives in the engine ShapeLabelsDrawer's own ImGui panel
}

} // namespace CluicheTest

#endif // DIA_DEBUG
