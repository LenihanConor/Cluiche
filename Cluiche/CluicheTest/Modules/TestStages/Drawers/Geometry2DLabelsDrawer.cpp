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
    // TODO: labels disabled until world→screen projection is properly wired
    ImGui::SliderFloat("Font scale", &mFontScale, 0.5f, 2.0f);
}

} // namespace CluicheTest

#endif // DIA_DEBUG



