#ifdef DIA_DEBUG

#include "Modules/TestStages/Drawers/Geometry2DLabelsDrawer.h"

#include <imgui.h>
#include <DiaGraphics/Frame/FrameData.h>

namespace CluicheTest {

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

void Geometry2DLabelsDrawer::Draw(Dia::Graphics::FrameData& /*frameData*/)
{
    // Labels are rendered as ImGui overlays — no FrameData draw calls needed.
}

void Geometry2DLabelsDrawer::DrawImGui()
{
    // World→screen: screenX = worldX, screenY = viewportH - worldY (Y-UP to Y-DOWN)
    const float viewH = ImGui::GetIO().DisplaySize.y;

    auto labelAt = [viewH](const char* text, float worldX, float worldY)
    {
        const float sx = worldX - 15.0f;
        const float sy = viewH - worldY - 18.0f;
        ImGui::GetForegroundDrawList()->AddText(
            ImVec2(sx, sy),
            IM_COL32(220, 220, 220, 200),
            text);
    };

    labelAt("Circle",     mCircle.GetCenter().X(),       mCircle.GetCenter().Y() + mCircle.GetRadius() + 5.0f);
    labelAt("AARect",     mAARect.CalculateCenter().X(), mAARect.GetTopRight().Y() + 5.0f);
    labelAt("OORect",     mOORect.CalculateCenter().X(), mOORect.CalculateCenter().Y() + 45.0f);
    labelAt("Line",       mLine.CalculateCenter().X(),   mLine.GetPt1().Y() + 30.0f);
    labelAt("Ray",        mRay.GetOrigin().X(),          mRay.GetOrigin().Y() + 30.0f);
    labelAt("Triangle",   mTriangle.CenterOfGravity().X(), mTriangle.GetPt(Dia::Geometry2D::Triangle::kPt2).Y() + 5.0f);
    labelAt("Capsule",    mCapsule.GetCenter().X(),      mCapsule.GetPoint2().Y() + 30.0f);
    labelAt("ConvexPoly", mConvexPoly.CalculateCenter().X(), mConvexPoly.GetVertex(0).Y() + 5.0f);
    labelAt("Arc",        mArc.GetFocal().X(),           mArc.GetFocal().Y() + mArc.GetRadius() + 5.0f);
    labelAt("Sector",     mSector.GetCenter().X(),       mSector.GetCenter().Y() + mSector.GetRadius() + 5.0f);

    ImGui::SliderFloat("Font scale", &mFontScale, 0.5f, 2.0f);
}

} // namespace CluicheTest

#endif // DIA_DEBUG



