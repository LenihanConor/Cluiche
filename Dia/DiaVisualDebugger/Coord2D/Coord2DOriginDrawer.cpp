////////////////////////////////////////////////////////////////////////////////
// Filename: Coord2DOriginDrawer.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaVisualDebugger/Coord2D/Coord2DOriginDrawer.h"

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

Coord2DOriginDrawer::Coord2DOriginDrawer(const Dia::Debug::DebugLayerManager& manager)
    : mManager(manager)
{}

Dia::Core::StringCRC Coord2DOriginDrawer::GetLayerName() const
{
    return Dia::Debug::LayerNames::kCoord2DOrigin;
}

void Coord2DOriginDrawer::Draw(Dia::Core::IDebugDraw& draw)
{
    DIA_TRACE_ZONE("coord2d.origin", ::Dia::Observation::Trace::Category::kDiaGraphics);

    const Dia::Graphics::ViewportTransform vp = mManager.GetViewportTransform();
    const Dia::Geometry2D::AARect bounds = vp.GetWorldBounds();

    const float worldWidth  = bounds.GetTopRight().x - bounds.GetBottomLeft().x;
    const float worldHeight = bounds.GetTopRight().y - bounds.GetBottomLeft().y;
    const float narrowAxis = (worldWidth < worldHeight) ? worldWidth : worldHeight;

    // Short crosshair: 2% of the narrower viewport axis
    const float armLength = narrowAxis * 0.02f;

    // Horizontal arm
    draw.RequestDraw(
        Dia::Maths::Vector2D(-armLength, 0.0f),
        Dia::Maths::Vector2D( armLength, 0.0f),
        Dia::Debug::DebugColourPalette::kActive);

    // Vertical arm
    draw.RequestDraw(
        Dia::Maths::Vector2D(0.0f, -armLength),
        Dia::Maths::Vector2D(0.0f,  armLength),
        Dia::Debug::DebugColourPalette::kActive);

    // Dot at origin
    draw.RequestDrawPoint(
        Dia::Maths::Vector2D(0.0f, 0.0f),
        Dia::Debug::DebugColourPalette::kGoal);
}

} // namespace Dia::Debug

#endif // DIA_DEBUG
