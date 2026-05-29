////////////////////////////////////////////////////////////////////////////////
// Filename: Coord2DGridDrawer.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaVisualDebugger/Coord2D/Coord2DGridDrawer.h"

#ifdef DIA_DEBUG

#include <DiaGraphics/Frame/FrameData.h>
#include <DiaGraphics/Camera/ViewportTransform.h>
#include <DiaGeometry2D/Shapes/AARect.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include "DiaVisualDebugger/DebugLayerManager.h"
#include "DiaVisualDebugger/DebugColourPalette.h"
#include "DiaVisualDebugger/DebugLayerNames.h"

#include <cmath>
#include <cstdio>

namespace Dia::Debug
{

Coord2DGridDrawer::Coord2DGridDrawer(const Dia::Debug::DebugLayerManager& manager)
    : mManager(manager)
{}

Dia::Core::StringCRC Coord2DGridDrawer::GetLayerName() const
{
    return Dia::Debug::LayerNames::kCoord2DGrid;
}

void Coord2DGridDrawer::Draw(Dia::Graphics::FrameData& frameData)
{
    DIA_TRACE_ZONE("coord2d.grid", ::Dia::Observation::Trace::Category::kDiaGraphics);

    const Dia::Graphics::ViewportTransform vp     = mManager.GetViewportTransform();
    const Dia::Geometry2D::AARect          bounds = vp.GetWorldBounds();

    const Dia::Maths::Vector2D& bl = bounds.GetBottomLeft();
    const Dia::Maths::Vector2D& tr = bounds.GetTopRight();

    const float worldWidth  = tr.x - bl.x;
    const float worldHeight = tr.y - bl.y;

    if (worldWidth <= 0.0f || worldHeight <= 0.0f)
        return;

    // Power-of-10 spacing: largest power of 10 that is <= narrowAxis/5
    const float narrowAxis = (worldWidth < worldHeight) ? worldWidth : worldHeight;
    const float rawSpacing = narrowAxis / 5.0f;

    float spacing = powf(10.0f, floorf(log10f(rawSpacing)));

    if (spacing <= 0.0f)
        return;

    // Line budget: at most 200 total lines (vertical + horizontal)
    static const int kMaxLines = 200;

    const int startNx = static_cast<int>(floorf(bl.x / spacing));
    const int endNx   = static_cast<int>(floorf(tr.x / spacing)) + 1;
    const int startNy = static_cast<int>(floorf(bl.y / spacing));
    const int endNy   = static_cast<int>(floorf(tr.y / spacing)) + 1;

    int lineCount = 0;

    // ----------------------------------------------------------------
    // Vertical grid lines (constant x)
    // ----------------------------------------------------------------
    for (int n = startNx; n <= endNx && lineCount < kMaxLines; ++n)
    {
        const float x = n * spacing;
        if (x < bl.x || x > tr.x)
            continue;

        frameData.RequestDraw(
            Dia::Maths::Vector2D(x, bl.y),
            Dia::Maths::Vector2D(x, tr.y),
            Dia::Debug::DebugColourPalette::kInactive);
        ++lineCount;

        // Label at y=0 crossing (clamp to viewport if y=0 is off-screen)
        if (n == 0)
            continue; // origin label handled by Coord2DOriginDrawer

        float labelY = 0.0f;
        if (labelY < bl.y) labelY = bl.y;
        if (labelY > tr.y) labelY = tr.y;

        char buf[32];
        if (spacing >= 1.0f)
            snprintf(buf, sizeof(buf), "%d", static_cast<int>(x));
        else
            snprintf(buf, sizeof(buf), "%.1f", x);

        frameData.RequestDrawText(
            Dia::Maths::Vector2D(x, labelY),
            buf,
            12.0f,
            Dia::Debug::DebugColourPalette::kWarning);
    }

    // ----------------------------------------------------------------
    // Horizontal grid lines (constant y)
    // ----------------------------------------------------------------
    for (int n = startNy; n <= endNy && lineCount < kMaxLines; ++n)
    {
        const float y = n * spacing;
        if (y < bl.y || y > tr.y)
            continue;

        frameData.RequestDraw(
            Dia::Maths::Vector2D(bl.x, y),
            Dia::Maths::Vector2D(tr.x, y),
            Dia::Debug::DebugColourPalette::kInactive);
        ++lineCount;

        // Label at x=0 crossing (clamp to viewport if x=0 is off-screen)
        if (n == 0)
            continue; // origin label handled by Coord2DOriginDrawer

        float labelX = 0.0f;
        if (labelX < bl.x) labelX = bl.x;
        if (labelX > tr.x) labelX = tr.x;

        char buf[32];
        if (spacing >= 1.0f)
            snprintf(buf, sizeof(buf), "%d", static_cast<int>(y));
        else
            snprintf(buf, sizeof(buf), "%.1f", y);

        frameData.RequestDrawText(
            Dia::Maths::Vector2D(labelX, y),
            buf,
            12.0f,
            Dia::Debug::DebugColourPalette::kWarning);
    }
}

} // namespace Dia::Debug

#endif // DIA_DEBUG
