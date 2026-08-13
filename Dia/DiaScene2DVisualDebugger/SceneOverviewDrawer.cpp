////////////////////////////////////////////////////////////////////////////////
// Filename: SceneOverviewDrawer.cpp
// Feature spec: docs/specs/features/dia/diavisualdebugger/drawer-boundary-consistency.md
////////////////////////////////////////////////////////////////////////////////
#include "DiaScene2DVisualDebugger/SceneOverviewDrawer.h"

#ifdef DIA_DEBUG

#include <DiaCamera2D/Registry/CameraRegistry2D.h>
#include <DiaCamera2D/Camera2D.h>
#include <DiaLighting2D/Registry/LightRegistry2D.h>
#include <DiaLighting2D/PointLight2D.h>
#include <DiaScene2D/LayerTable.h>
#include <DiaGeometry2DVisualDebugger/ShapeDrawer.h>
#include <DiaGeometry2D/Shapes/Circle.h>
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
static const Dia::Graphics::RGBA kCameraColour     (130, 130, 240, 255);
static const Dia::Graphics::RGBA kLightWarmColour  (255, 180,  60, 255);
static const Dia::Graphics::RGBA kLightCoolColour  ( 60, 160, 255, 255);

SceneOverviewDrawer::SceneOverviewDrawer(
    const Dia::Camera2D::CameraRegistry2D&  cameraRegistry,
    const Dia::Lighting2D::LightRegistry2D& lightRegistry,
    const Dia::Scene2D::LayerTable&         layerTable,
    const Dia::Core::IDebugContext&         manager)
    : mCameraRegistry(cameraRegistry)
    , mLightRegistry(lightRegistry)
    , mLayerTable(layerTable)
    , mManager(manager)
{}

Dia::Core::StringCRC SceneOverviewDrawer::GetLayerName() const
{
    return Dia::Debug::LayerNames::kScene2DOverview;
}

#pragma warning(push)
#pragma warning(disable: 6262)
void SceneOverviewDrawer::Draw(Dia::Core::IDebugDraw& draw)
{
    if (!IsEnabled()) return;

    Dia::Geometry2DVisualDebugger::ShapeDrawer drawer(mManager);

    // World bounds outline
    Dia::Geometry2D::AARect worldBounds(
        Dia::Maths::Vector2D(-400.0f, -300.0f),
        Dia::Maths::Vector2D( 400.0f,  300.0f));
    drawer.SubmitAARect(worldBounds, kWorldBoundsColour);

    // Layer bands — horizontal strips coloured by depth
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

    // Active camera as a circle
    if (mCameraRegistry.GetCount() > 0)
    {
        const auto& cam = mCameraRegistry.GetActive();
        Dia::Geometry2D::Circle camCircle(20.0f, cam.GetPosition());
        drawer.SubmitCircle(camCircle, kCameraColour);
    }

    // Lights — circle at position + translucent radius ring
    for (unsigned int i = 0; i < mLightRegistry.GetCount(); ++i)
    {
        const auto& light = mLightRegistry.GetByIndex(i);
        if (!light.enabled) continue;

        Dia::Graphics::RGBA colour = (i == 0) ? kLightWarmColour : kLightCoolColour;
        Dia::Geometry2D::Circle lightCircle(12.0f, light.position);
        drawer.SubmitCircle(lightCircle, colour);

        Dia::Geometry2D::Circle radiusCircle(
            light.radius > 0.0f ? light.radius : 50.0f, light.position);
        drawer.SubmitCircle(radiusCircle, Dia::Graphics::RGBA(colour.R(), colour.G(), colour.B(), 40));
    }

    drawer.Draw(draw);
}
#pragma warning(pop)

} // namespace Dia::Scene2DVisualDebugger

#endif // DIA_DEBUG
