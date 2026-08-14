////////////////////////////////////////////////////////////////////////////////
// Filename: LightRangesDrawer.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaLighting3DVisualDebugger/LightRangesDrawer.h"

#ifdef DIA_DEBUG

#include <DiaCore/DebugDraw/IDebugDraw.h>
#include <DiaGraphics/Frame/DebugFrameData.h>
#include <DiaLighting3D/PointLight3D.h>
#include <DiaLighting3D/SpotLight3D.h>
#include <DiaLighting3D/Registry/LightRegistry3D.h>
#include <DiaCore/DebugDraw/DebugLayerNames.h>
#include <DiaGraphics/Misc/RGBA.h>

namespace Dia { namespace Lighting3D {

static const Dia::Graphics::RGBA kPointRangeColour(255, 220,  60,  40);
static const Dia::Graphics::RGBA kSpotRangeColour (180, 220, 255,  40);

LightRangesDrawer::LightRangesDrawer(const LightRegistry3D& registry)
    : mRegistry(registry)
{}

Dia::Core::StringCRC LightRangesDrawer::GetLayerName() const
{
    return Dia::Debug::LayerNames::kLightRanges;
}

void LightRangesDrawer::Draw(Dia::Core::IDebugDraw& draw)
{
    Dia::Graphics::DebugFrameData& dbg = static_cast<Dia::Graphics::DebugFrameData&>(draw);

    for (unsigned int i = 0; i < mRegistry.GetPointCount(); ++i)
    {
        const PointLight3D& light = mRegistry.GetPointByIndex(i);
        if (!light.enabled || light.radius <= 0.0f) continue;
        dbg.RequestDrawSphere3D(light.position, light.radius, kPointRangeColour);
    }

    for (unsigned int i = 0; i < mRegistry.GetSpotCount(); ++i)
    {
        const SpotLight3D& light = mRegistry.GetSpotByIndex(i);
        if (!light.enabled || light.range <= 0.0f) continue;
        dbg.RequestDrawSphere3D(light.position, light.range, kSpotRangeColour);
    }
}

} } // namespace Dia::Lighting3D

#endif // DIA_DEBUG
