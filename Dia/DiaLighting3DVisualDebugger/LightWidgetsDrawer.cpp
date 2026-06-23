////////////////////////////////////////////////////////////////////////////////
// Filename: LightWidgetsDrawer.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaLighting3DVisualDebugger/LightWidgetsDrawer.h"

#ifdef DIA_DEBUG

#include <DiaGraphics/Frame/FrameData.h>
#include <DiaGraphics/Frame/DebugFrameData.h>
#include <DiaLighting3D/PointLight3D.h>
#include <DiaLighting3D/SpotLight3D.h>
#include <DiaLighting3D/DirectionalLight3D.h>
#include <DiaLighting3D/Registry/LightRegistry3D.h>
#include <DiaVisualDebugger/DebugLayerNames.h>
#include <DiaVisualDebugger/DebugColourPalette.h>
#include <DiaVisualDebugger/DebugLayerManager.h>

#include <imgui.h>

namespace Dia { namespace Lighting3D {

static constexpr float kBasePointRadius  = 0.15f;
static constexpr float kBaseSpotRadius   = 0.15f;
static constexpr float kBaseSpotArrowLen = 0.5f;
static constexpr float kBaseDirArrowLen  = 2.0f;

LightWidgetsDrawer::LightWidgetsDrawer(const LightRegistry3D&               registry,
                                       const Dia::Debug::DebugLayerManager& manager)
    : mRegistry(registry)
    , mManager(manager)
{}

Dia::Core::StringCRC LightWidgetsDrawer::GetLayerName() const
{
    return Dia::Debug::LayerNames::kLightWidgets;
}

void LightWidgetsDrawer::Draw(Dia::Graphics::FrameData& frameData)
{
    Dia::Graphics::DebugFrameData& dbg = static_cast<Dia::Graphics::DebugFrameData&>(frameData);

    const float scale = mManager.GetDebugScale() * mWidgetScale;

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
        static const Dia::Maths::Vector3D kWorldOrigin(0.0f, 0.0f, 0.0f);

        const unsigned int count = mRegistry.GetDirectionalCount();
        for (unsigned int i = 0; i < count; ++i)
        {
            const DirectionalLight3D& light = mRegistry.GetDirectionalByIndex(i);
            if (!light.enabled)
                continue;

            dbg.RequestDrawRay3D(kWorldOrigin,
                                 light.direction,
                                 kBaseDirArrowLen * scale,
                                 Dia::Debug::DebugColourPalette::kGoal);
        }
    }
}

void LightWidgetsDrawer::DrawImGui()
{
    ImGui::Checkbox("Point lights",       &mShowPointLights);
    ImGui::Checkbox("Spot lights",        &mShowSpotLights);
    ImGui::Checkbox("Directional lights", &mShowDirectionalLights);
    ImGui::SliderFloat("Widget scale",    &mWidgetScale, 0.1f, 5.0f);
}

} } // namespace Dia::Lighting3D

#endif // DIA_DEBUG
