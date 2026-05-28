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
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always, ImVec2(0, 0));

    ImGui::SetWindowFontScale(mFontScale);

    auto labelAt = [](const char* text, float x, float y)
    {
        ImVec2 screen = ImGui::GetWindowPos();
        ImGui::GetWindowDrawList()->AddText(
            ImVec2(screen.x + x, screen.y + y),
            IM_COL32(220, 220, 220, 200),
            text);
    };

    // Labels at fixed offsets — positions match SetupGallery() layout in the module.
    labelAt("Circle",      mCircle.GetCenter().X(),        mCircle.GetCenter().Y()        - mCircle.GetRadius() - 12.0f);
    labelAt("AARect",      mAARect.CalculateCenter().X(),       mAARect.CalculateCenter().Y()       - 40.0f);
    labelAt("OORect",      mOORect.CalculateCenter().X(),       mOORect.CalculateCenter().Y()       - 40.0f);
    labelAt("Line",        mLine.GetPt1().X(),          mLine.GetPt1().Y()          - 12.0f);
    labelAt("Ray",         mRay.GetOrigin().X(),          mRay.GetOrigin().Y()          - 12.0f);
    labelAt("Triangle",    mTriangle.GetPt(Dia::Geometry2D::Triangle::kPt0).X(), mTriangle.GetPt(Dia::Geometry2D::Triangle::kPt0).Y() - 12.0f);
    labelAt("Capsule",     mCapsule.GetPoint1().X(),      mCapsule.GetPoint1().Y()      - 45.0f);
    labelAt("ConvexPoly",  mConvexPoly.GetVertex(0).X(),  mConvexPoly.GetVertex(0).Y()  - 12.0f);
    labelAt("Arc",         mArc.GetFocal().X(),           mArc.GetFocal().Y()           - mArc.GetRadius() - 12.0f);
    labelAt("Sector",      mSector.GetCenter().X(),       mSector.GetCenter().Y()       - mSector.GetRadius() - 12.0f);

    ImGui::SliderFloat("Font scale", &mFontScale, 0.5f, 2.0f);
}

} // namespace CluicheTest

#endif // DIA_DEBUG



