////////////////////////////////////////////////////////////////////////////////
// Filename: LightWidgetsDrawer.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaLighting3DVisualDebugger/LightWidgetsDrawer.h"

#ifdef DIA_DEBUG

#include <DiaCore/DebugDraw/IDebugDraw.h>
#include <DiaGraphics/Frame/DebugFrameData.h>
#include <DiaLighting3D/PointLight3D.h>
#include <DiaLighting3D/SpotLight3D.h>
#include <DiaLighting3D/DirectionalLight3D.h>
#include <DiaLighting3D/Registry/LightRegistry3D.h>
#include <DiaCore/DebugDraw/DebugLayerNames.h>
#include <DiaCore/DebugDraw/DebugColourPalette.h>

namespace Dia { namespace Lighting3D {

static constexpr float kBasePointRadius  = 0.15f;
static constexpr float kBaseSpotRadius   = 0.15f;
static constexpr float kBaseSpotArrowLen = 0.5f;
static constexpr float kBaseDirArrowLen  = 2.0f;

LightWidgetsDrawer::LightWidgetsDrawer(const LightRegistry3D& registry)
    : mRegistry(registry)
{}

Dia::Core::StringCRC LightWidgetsDrawer::GetLayerName() const
{
    return Dia::Debug::LayerNames::kLightWidgets;
}

void LightWidgetsDrawer::Draw(Dia::Core::IDebugDraw& draw)
{
    Dia::Graphics::DebugFrameData& dbg = static_cast<Dia::Graphics::DebugFrameData&>(draw);

    const float scale = mWidgetScale;

    if (mShowPointLights)
    {
        const unsigned int count = mRegistry.GetPointCount();
        for (unsigned int i = 0; i < count; ++i)
        {
            const PointLight3D& light = mRegistry.GetPointByIndex(i);
            if (!light.enabled)
                continue;

            dbg.RequestDrawSphere3D(light.position,
                                    kBasePointRadius * scale,
                                    Dia::Debug::DebugColourPalette::kWarning);
        }
    }

    if (mShowSpotLights)
    {
        const unsigned int count = mRegistry.GetSpotCount();
        for (unsigned int i = 0; i < count; ++i)
        {
            const SpotLight3D& light = mRegistry.GetSpotByIndex(i);
            if (!light.enabled)
                continue;

            dbg.RequestDrawSphere3D(light.position,
                                    kBaseSpotRadius * scale,
                                    Dia::Debug::DebugColourPalette::kWarning);

            dbg.RequestDrawRay3D(light.position,
                                 light.direction,
                                 kBaseSpotArrowLen * scale,
                                 Dia::Debug::DebugColourPalette::kWarning);
        }
    }

    if (mShowDirectionalLights)
    {
        const Dia::Maths::Vector3D anchor(0.0f, mDirLightAnchorHeight, 0.0f);

        const unsigned int count = mRegistry.GetDirectionalCount();
        for (unsigned int i = 0; i < count; ++i)
        {
            const DirectionalLight3D& light = mRegistry.GetDirectionalByIndex(i);
            if (!light.enabled)
                continue;

            dbg.RequestDrawSphere3D(anchor,
                                    kBasePointRadius * scale,
                                    Dia::Debug::DebugColourPalette::kGoal);
            dbg.RequestDrawRay3D(anchor,
                                 light.direction,
                                 kBaseDirArrowLen * scale,
                                 Dia::Debug::DebugColourPalette::kGoal);
        }
    }
}

} } // namespace Dia::Lighting3D

#endif // DIA_DEBUG
