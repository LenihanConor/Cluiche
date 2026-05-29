////////////////////////////////////////////////////////////////////////////////
// Filename: Coord2DCursorDrawer.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaVisualDebugger/Coord2D/Coord2DCursorDrawer.h"

#ifdef DIA_DEBUG

#include <cstdio>

#include <DiaGraphics/Frame/FrameData.h>
#include <DiaGraphics/Camera/ViewportTransform.h>
#include <DiaGeometry2D/Shapes/AARect.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include "DiaVisualDebugger/DebugLayerManager.h"
#include "DiaVisualDebugger/DebugColourPalette.h"
#include "DiaVisualDebugger/DebugLayerNames.h"

namespace Dia::Debug
{

Coord2DCursorDrawer::Coord2DCursorDrawer(const Dia::Debug::DebugLayerManager& manager)
    : mManager(manager)
{}

Dia::Core::StringCRC Coord2DCursorDrawer::GetLayerName() const
{
    return Dia::Debug::LayerNames::kCoord2DCursor;
}

void Coord2DCursorDrawer::Draw(Dia::Graphics::FrameData& frameData)
{
    DIA_TRACE_ZONE("coord2d.cursor", ::Dia::Observation::Trace::Category::kDiaGraphics);

    const Dia::Maths::Vector2D& pixel = frameData.GetMousePixel();

    // Skip if mouse pixel has not been set this frame
    if (pixel.x == 0.0f && pixel.y == 0.0f)
    {
        return;
    }

    const Dia::Graphics::ViewportTransform vt = mManager.GetViewportTransform();
    const Dia::Maths::Vector2D worldPos = vt.ScreenToWorld(pixel);

    // Crosshair arms: 1.5% of the narrower viewport axis
    const Dia::Geometry2D::AARect bounds = vt.GetWorldBounds();
    const float worldWidth  = bounds.GetTopRight().x - bounds.GetBottomLeft().x;
    const float worldHeight = bounds.GetTopRight().y - bounds.GetBottomLeft().y;
    const float narrowAxis = (worldWidth < worldHeight) ? worldWidth : worldHeight;
    const float armLength = narrowAxis * 0.015f;

    frameData.RequestDraw(
        Dia::Maths::Vector2D(worldPos.x - armLength, worldPos.y),
        Dia::Maths::Vector2D(worldPos.x + armLength, worldPos.y),
        Dia::Debug::DebugColourPalette::kGoal);

    frameData.RequestDraw(
        Dia::Maths::Vector2D(worldPos.x, worldPos.y - armLength),
        Dia::Maths::Vector2D(worldPos.x, worldPos.y + armLength),
        Dia::Debug::DebugColourPalette::kGoal);

    // Text label showing world-space coordinates
    char buf[64];
    snprintf(buf, sizeof(buf), "(%.1f, %.1f)", worldPos.x, worldPos.y);

    frameData.RequestDrawText(worldPos, buf, 12.0f, Dia::Debug::DebugColourPalette::kGoal);
}

} // namespace Dia::Debug

#endif // DIA_DEBUG
