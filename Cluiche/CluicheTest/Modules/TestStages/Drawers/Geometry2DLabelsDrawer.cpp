#ifdef DIA_DEBUG

#include "Modules/TestStages/Drawers/Geometry2DLabelsDrawer.h"

#include <imgui.h>
#include <DiaGraphics/Frame/FrameData.h>
#include <DiaGraphics/Misc/RGBA.h>

namespace CluicheTest {

static const Dia::Graphics::RGBA kLabelColour(90, 122, 170, 255);
static constexpr float kLabelFontSize = 12.0f;
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
    const Dia::Geometry2D::Arc&           arc,
    const Dia::Geometry2D::Sector&        sector)
    : mCircle(circle), mAARect(aaRect), mOORect(ooRect)
    , mLine(line), mRay(ray), mTriangle(triangle)
    , mCapsule(capsule), mConvexPoly(convexPoly)
    , mArc(arc), mSector(sector)
{}

Dia::Core::StringCRC Geometry2DLabelsDrawer::GetLayerName() const
{
    return Dia::Core::StringCRC("geo2d.labels");
}

void Geometry2DLabelsDrawer::Draw(Dia::Graphics::FrameData& frameData)
{
    const float fs = kLabelFontSize * mFontScale;

    // Row 1 labels — positioned below each shape
    frameData.RequestDrawText(
        Dia::Maths::Vector2D(mCircle.GetCenter().x, mCircle.GetCenter().y + kLabelOffsetY),
        "circle", fs, kLabelColour);

    const auto aaCentre = (mAARect.GetBottomLeft() + mAARect.GetTopRight()) * 0.5f;
    frameData.RequestDrawText(
        Dia::Maths::Vector2D(aaCentre.x, aaCentre.y + kLabelOffsetY),
        "aa_rect", fs, kLabelColour);

    const auto ooCentre = (mOORect.GetPt(0) + mOORect.GetPt(1) + mOORect.GetPt(2) + mOORect.GetPt(3)) * 0.25f;
    frameData.RequestDrawText(
        Dia::Maths::Vector2D(ooCentre.x, ooCentre.y + kLabelOffsetY),
        "oo_rect*", fs, kLabelColour);

    const auto lineCentre = (mLine.GetPt1() + mLine.GetPt2()) * 0.5f;
    frameData.RequestDrawText(
        Dia::Maths::Vector2D(lineCentre.x, lineCentre.y + kLabelOffsetY),
        "line", fs, kLabelColour);

    frameData.RequestDrawText(
        Dia::Maths::Vector2D(mRay.GetOrigin().x, mRay.GetOrigin().y + kLabelOffsetY),
        "ray", fs, kLabelColour);

    // Row 2 labels
    const auto triCentre = (mTriangle.GetPt(Dia::Geometry2D::Triangle::kPt0)
        + mTriangle.GetPt(Dia::Geometry2D::Triangle::kPt1)
        + mTriangle.GetPt(Dia::Geometry2D::Triangle::kPt2)) * (1.0f / 3.0f);
    frameData.RequestDrawText(
        Dia::Maths::Vector2D(triCentre.x, triCentre.y + kLabelOffsetY),
        "triangle", fs, kLabelColour);

    frameData.RequestDrawText(
        Dia::Maths::Vector2D(mCapsule.GetCenter().x, mCapsule.GetCenter().y + kLabelOffsetY),
        "capsule*", fs, kLabelColour);

    {
        Dia::Maths::Vector2D polyCentre(0.0f, 0.0f);
        for (int i = 0; i < mConvexPoly.GetVertexCount(); ++i)
            polyCentre = polyCentre + mConvexPoly.GetVertex(i);
        polyCentre = polyCentre * (1.0f / static_cast<float>(mConvexPoly.GetVertexCount()));
        frameData.RequestDrawText(
            Dia::Maths::Vector2D(polyCentre.x, polyCentre.y + kLabelOffsetY),
            "convex_poly", fs, kLabelColour);
    }

    frameData.RequestDrawText(
        Dia::Maths::Vector2D(mArc.GetFocal().x, mArc.GetFocal().y + kLabelOffsetY),
        "arc*", fs, kLabelColour);

    frameData.RequestDrawText(
        Dia::Maths::Vector2D(mSector.GetCenter().x, mSector.GetCenter().y + kLabelOffsetY),
        "sector*", fs, kLabelColour);
}

void Geometry2DLabelsDrawer::DrawImGui()
{
    ImGui::SliderFloat("Font scale", &mFontScale, 0.5f, 2.0f);
}

} // namespace CluicheTest

#endif // DIA_DEBUG



