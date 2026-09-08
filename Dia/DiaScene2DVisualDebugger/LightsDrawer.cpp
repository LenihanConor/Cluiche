////////////////////////////////////////////////////////////////////////////////
// Filename: LightsDrawer.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaScene2DVisualDebugger/LightsDrawer.h"

#ifdef DIA_DEBUG

#include <DiaLighting2D/Registry/LightRegistry2D.h>
#include <DiaLighting2D/PointLight2D.h>
#include <DiaGeometry2DVisualDebugger/ShapeDrawer.h>
#include <DiaGeometry2D/Shapes/Circle.h>
#include <DiaCore/DebugDraw/IDebugDraw.h>
#include <DiaGraphics/Misc/RGBA.h>
#include <DiaCore/DebugDraw/DebugLayerNames.h>

namespace Dia::Scene2DVisualDebugger
{

static const Dia::Graphics::RGBA kLightWarmColour(255, 180,  60, 255);
static const Dia::Graphics::RGBA kLightCoolColour( 60, 160, 255, 255);

LightsDrawer::LightsDrawer(const Dia::Lighting2D::LightRegistry2D& lightRegistry,
                           const Dia::Core::IDebugContext&         manager)
    : mLightRegistry(lightRegistry)
    , mManager(manager)
{}

Dia::Core::StringCRC LightsDrawer::GetLayerName() const
{
    return Dia::Debug::LayerNames::kScene2DLights;
}

void LightsDrawer::Draw(Dia::Core::IDebugDraw& draw)
{
    if (!IsEnabled()) return;

    Dia::Geometry2DVisualDebugger::ShapeDrawer drawer(mManager);
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

} // namespace Dia::Scene2DVisualDebugger

#endif // DIA_DEBUG
