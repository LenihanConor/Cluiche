////////////////////////////////////////////////////////////////////////////////
// Filename: Coord2DBoundsDrawer.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaVisualDebugger/Coord2D/Coord2DBoundsDrawer.h"

#ifdef DIA_DEBUG

#include <DiaGraphics/Frame/FrameData.h>
#include <DiaGraphics/Camera/ViewportTransform.h>
#include <DiaGeometry2D/Shapes/AARect.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include "DiaVisualDebugger/DebugLayerManager.h"
#include "DiaVisualDebugger/DebugColourPalette.h"
#include "DiaVisualDebugger/DebugLayerNames.h"

#include <cstdio>

namespace Dia::Debug
{

Coord2DBoundsDrawer::Coord2DBoundsDrawer(const Dia::Debug::DebugLayerManager& manager)
    : mManager(manager)
{}

Dia::Core::StringCRC Coord2DBoundsDrawer::GetLayerName() const
{
    return Dia::Debug::LayerNames::kCoord2DBounds;
}

void Coord2DBoundsDrawer::Draw(Dia::Graphics::FrameData& frameData)
{
    DIA_TRACE_ZONE("coord2d.bounds", ::Dia::Observation::Trace::Category::kDiaGraphics);

    const Dia::Graphics::ViewportTransform vp     = mManager.GetViewportTransform();
    const Dia::Geometry2D::AARect          bounds = vp.GetWorldBounds();

    const Dia::Maths::Vector2D& bl = bounds.GetBottomLeft();
    const Dia::Maths::Vector2D& tr = bounds.GetTopRight();

    // Inset labels by 5% so they render on-screen
    const float insetX = (tr.x - bl.x) * 0.05f;
    const float insetY = (tr.y - bl.y) * 0.05f;

    const Dia::Maths::Vector2D inBL(bl.x + insetX, bl.y + insetY);
    const Dia::Maths::Vector2D inTR(tr.x - insetX, tr.y - insetY);
    const Dia::Maths::Vector2D inTL(bl.x + insetX, tr.y - insetY);
    const Dia::Maths::Vector2D inBR(tr.x - insetX, bl.y + insetY);

    char buf[64];

    // Bottom-left
    snprintf(buf, sizeof(buf), "(%.1f, %.1f)", bl.x, bl.y);
    frameData.RequestDrawText(inBL, buf, 11.0f, Dia::Debug::DebugColourPalette::kActive);

    // Top-right
    snprintf(buf, sizeof(buf), "(%.1f, %.1f)", tr.x, tr.y);
    frameData.RequestDrawText(inTR, buf, 11.0f, Dia::Debug::DebugColourPalette::kActive);

    // Top-left
    snprintf(buf, sizeof(buf), "(%.1f, %.1f)", bl.x, tr.y);
    frameData.RequestDrawText(inTL, buf, 11.0f, Dia::Debug::DebugColourPalette::kActive);

    // Bottom-right
    snprintf(buf, sizeof(buf), "(%.1f, %.1f)", tr.x, bl.y);
    frameData.RequestDrawText(inBR, buf, 11.0f, Dia::Debug::DebugColourPalette::kActive);
}

} // namespace Dia::Debug

#endif // DIA_DEBUG
