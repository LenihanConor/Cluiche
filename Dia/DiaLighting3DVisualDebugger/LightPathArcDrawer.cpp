////////////////////////////////////////////////////////////////////////////////
// Filename: LightPathArcDrawer.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaLighting3DVisualDebugger/LightPathArcDrawer.h"

#ifdef DIA_DEBUG

#include <DiaGraphics/Frame/FrameData.h>
#include <DiaGraphics/Frame/DebugFrameData.h>
#include <DiaLighting3D/Registry/LightRegistry3D.h>
#include <DiaLighting3D/Behaviours/LightPathBehaviour3D.h>
#include <DiaGeometry3D/Shapes/Spline3D.h>
#include <DiaVisualDebugger/DebugLayerNames.h>
#include <DiaVisualDebugger/DebugColourPalette.h>
#include <DiaVisualDebugger/DebugLayerManager.h>
#include <imgui.h>

namespace Dia { namespace Lighting3D {

LightPathArcDrawer::LightPathArcDrawer(const LightRegistry3D&               registry,
                                       const Dia::Debug::DebugLayerManager& manager)
    : mRegistry(registry)
    , mManager(manager)
{}

Dia::Core::StringCRC LightPathArcDrawer::GetLayerName() const
{
    return Dia::Debug::LayerNames::kLightPathArc;
}

void LightPathArcDrawer::Draw(Dia::Graphics::FrameData& frameData)
{
    (void)frameData;
    // Implemented in path-arc-preview feature
}

void LightPathArcDrawer::DrawImGui()
{
    ImGui::SliderInt("Arc samples", &mArcSamples, 8, 64);
}

} } // namespace Dia::Lighting3D

#endif // DIA_DEBUG
