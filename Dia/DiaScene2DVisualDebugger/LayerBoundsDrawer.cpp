////////////////////////////////////////////////////////////////////////////////
// Filename: LayerBoundsDrawer.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaScene2DVisualDebugger/LayerBoundsDrawer.h"

#ifdef DIA_DEBUG

#include <DiaScene2D/LayerTable.h>
#include <DiaGeometry2DVisualDebugger/ShapeDrawer.h>
#include <DiaGeometry2D/Shapes/AARect.h>
#include <DiaCore/DebugDraw/IDebugDraw.h>
#include <DiaGraphics/Misc/RGBA.h>
#include <DiaCore/DebugDraw/DebugLayerNames.h>

namespace Dia::Scene2DVisualDebugger
{

static const Dia::Graphics::RGBA kWorldBoundsColour(80,  80,  140, 120);
static const Dia::Graphics::RGBA kLayerBgColour    (60,  60,  100,  60);
static const Dia::Graphics::RGBA kLayerMidColour   (80,  80,  120,  50);
static const Dia::Graphics::RGBA kLayerFgColour    (100, 100, 140,  40);

LayerBoundsDrawer::LayerBoundsDrawer(const Dia::Scene2D::LayerTable& layerTable,
                                     const Dia::Core::IDebugContext& manager)
    : mLayerTable(layerTable)
    , mManager(manager)
{}

Dia::Core::StringCRC LayerBoundsDrawer::GetLayerName() const
{
    return Dia::Debug::LayerNames::kScene2DLayerBounds;
}

#pragma warning(push)
#pragma warning(disable: 6262)
void LayerBoundsDrawer::Draw(Dia::Core::IDebugDraw& draw)
{
    if (!IsEnabled()) return;

    Dia::Geometry2DVisualDebugger::ShapeDrawer drawer(mManager);

    Dia::Geometry2D::AARect worldBounds(
        Dia::Maths::Vector2D(-400.0f, -300.0f),
        Dia::Maths::Vector2D( 400.0f,  300.0f));
    drawer.SubmitAARect(worldBounds, kWorldBoundsColour);

    const unsigned int layerCount = mLayerTable.GetCount();
    if (layerCount > 0)
    {
        const float totalHeight = 600.0f;
        const float bandHeight  = totalHeight / static_cast<float>(layerCount);
        const float startY      = -300.0f;

        for (unsigned int i = 0; i < layerCount; ++i)
        {
            float y0 = startY + bandHeight * static_cast<float>(i);
            float y1 = y0 + bandHeight;
            Dia::Geometry2D::AARect band(
                Dia::Maths::Vector2D(-400.0f, y0),
                Dia::Maths::Vector2D( 400.0f, y1));
            Dia::Graphics::RGBA colour = (i == 0) ? kLayerBgColour
                                       : (i == 1) ? kLayerMidColour
                                                  : kLayerFgColour;
            drawer.SubmitAARect(band, colour);
        }
    }

    drawer.Draw(draw);
}
#pragma warning(pop)

} // namespace Dia::Scene2DVisualDebugger

#endif // DIA_DEBUG
