////////////////////////////////////////////////////////////////////////////////
// Filename: ShapeLabelsDrawer.cpp
// Feature spec: docs/specs/features/dia/diavisualdebugger/drawer-boundary-consistency.md
////////////////////////////////////////////////////////////////////////////////
#include "DiaGeometry2DVisualDebugger/ShapeLabelsDrawer.h"

#ifdef DIA_DEBUG

#include <DiaCore/DebugDraw/IDebugDraw.h>
#include <DiaGraphics/Misc/RGBA.h>
#include <DiaCore/DebugDraw/DebugLayerNames.h>
#include <imgui.h>

namespace Dia::Geometry2DVisualDebugger
{

static const Dia::Graphics::RGBA kLabelColour(90, 122, 170, 255);

ShapeLabelsDrawer::ShapeLabelsDrawer(const Dia::Core::IDebugContext& manager)
    : mManager(manager)
{}

Dia::Core::StringCRC ShapeLabelsDrawer::GetLayerName() const
{
    return Dia::Debug::LayerNames::kGeoLabels;
}

void ShapeLabelsDrawer::SubmitLabel(Dia::Maths::Vector2D position, const char* text)
{
    if (mPending.IsFull()) return;
    LabelEntry e{ position.x, position.y, text };
    mPending.Add(e);
}

void ShapeLabelsDrawer::Draw(Dia::Core::IDebugDraw& draw)
{
    Dia::Core::Containers::DynamicArrayC<LabelEntry, kMaxLabels> pending;
    pending.Swap(mPending);

    if (!IsEnabled()) return;

    const float fontSize = kDefaultFontSize * mFontScale;
    for (unsigned int i = 0; i < pending.Size(); ++i)
    {
        const LabelEntry& e = pending[i];
        draw.RequestDrawText(
            Dia::Maths::Vector2D(e.x, e.y),
            e.text,
            fontSize,
            kLabelColour);
    }
}

void ShapeLabelsDrawer::DrawImGui()
{
    ImGui::SliderFloat("Font scale", &mFontScale, 0.5f, 2.0f);
}

} // namespace Dia::Geometry2DVisualDebugger

#endif // DIA_DEBUG
