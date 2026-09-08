////////////////////////////////////////////////////////////////////////////////
// Filename: Coord2DAxesDrawer.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaVisualDebugger/Coord2D/Coord2DAxesDrawer.h"

#ifdef DIA_DEBUG

#include <DiaCore/DebugDraw/IDebugDraw.h>
#include <DiaGraphics/Camera/ViewportTransform.h>
#include <DiaGeometry2D/Shapes/AARect.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include "DiaVisualDebugger/DebugLayerManager.h"
#include "DiaVisualDebugger/DebugColourPalette.h"
#include "DiaVisualDebugger/DebugLayerNames.h"

namespace Dia::Debug
{

Coord2DAxesDrawer::Coord2DAxesDrawer(const Dia::Debug::DebugLayerManager& manager)
    : mManager(manager)
{}

Dia::Core::StringCRC Coord2DAxesDrawer::GetLayerName() const
{
    return Dia::Debug::LayerNames::kCoord2DAxes;
}

void Coord2DAxesDrawer::Draw(Dia::Core::IDebugDraw& draw)
{
    DIA_TRACE_ZONE("coord2d.axes", ::Dia::Observation::Trace::Category::kDiaGraphics);

    const Dia::Graphics::ViewportTransform vp     = mManager.GetViewportTransform();
    const Dia::Geometry2D::AARect          bounds = vp.GetWorldBounds();

    const Dia::Maths::Vector2D& bl = bounds.GetBottomLeft();
    const Dia::Maths::Vector2D& tr = bounds.GetTopRight();

    // X axis (horizontal) — draw only if world Y=0 is within vertical bounds
    const bool originYVisible = (0.0f >= bl.y && 0.0f <= tr.y);
    if (originYVisible)
    {
        draw.RequestDraw(
            Dia::Maths::Vector2D(bl.x, 0.0f),
            Dia::Maths::Vector2D(tr.x, 0.0f),
            Dia::Debug::DebugColourPalette::kError);
    }

    // Y axis (vertical) — draw only if world X=0 is within horizontal bounds
    const bool originXVisible = (0.0f >= bl.x && 0.0f <= tr.x);
    if (originXVisible)
    {
        draw.RequestDraw(
            Dia::Maths::Vector2D(0.0f, bl.y),
            Dia::Maths::Vector2D(0.0f, tr.y),
            Dia::Debug::DebugColourPalette::kHealthy);
    }
}

} // namespace Dia::Debug

#endif // DIA_DEBUG
