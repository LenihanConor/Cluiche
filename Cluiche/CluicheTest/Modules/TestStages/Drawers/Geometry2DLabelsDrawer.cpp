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
    // Labels placed above each shape (higher worldY = above shape = lower screenY)
    const float viewH = ImGui::GetIO().DisplaySize.y;

    auto labelAt = [viewH](const char* text, float worldX, float worldY)
    {
        const float sx = worldX - 20.0f;
        const float sy = viewH - worldY;
        ImGui::GetForegroundDrawList()->AddText(
            ImVec2(sx, sy),
            IM_COL32(220, 220, 220, 200),
            text);
    };

    // Row 1 labels (above each shape)
    labelAt("Circle",     mCircle.GetCenter().X(),         mCircle.GetCenter().Y() + mCircle.GetRadius() + 12.0f);
    labelAt("AARect",     mAARect.CalculateCenter().X(),   mAARect.GetTopRight().Y() + 12.0f);
    labelAt("OORect",     mOORect.CalculateCenter().X(),   mOORect.CalculateCenter().Y() + 50.0f);
    labelAt("Line",       mLine.CalculateCenter().X(),     mLine.CalculateCenter().Y() + 35.0f);
    labelAt("Ray",        mRay.GetOrigin().X(),            mRay.GetOrigin().Y() + 35.0f);

    // Row 2 labels (above each shape)
    labelAt("Triangle",   mTriangle.CenterOfGravity().X(),   mTriangle.GetPt(Dia::Geometry2D::Triangle::kPt2).Y() + 12.0f);
    labelAt("Capsule",    mCapsule.GetCenter().X(),          mCapsule.GetPoint2().Y() + 30.0f);
    labelAt("ConvexPoly", mConvexPoly.CalculateCenter().X(), mConvexPoly.GetVertex(0).Y() + 12.0f);
    labelAt("Arc",        mArc.GetFocal().X(),               mArc.GetFocal().Y() + mArc.GetRadius() + 12.0f);
    labelAt("Sector",     mSector.GetCenter().X(),           mSector.GetCenter().Y() + mSector.GetRadius() + 12.0f);

    // Spatial structure labels (below each structure area)
    labelAt("BVH",         720.0f, 70.0f);
    labelAt("Quadtree",    960.0f, 70.0f);
    labelAt("SpatialGrid", 1190.0f, 70.0f);
    labelAt("HexGrid",     250.0f, 40.0f);

    ImGui::SliderFloat("Font scale", &mFontScale, 0.5f, 2.0f);
}

} // namespace CluicheTest

#endif // DIA_DEBUG



