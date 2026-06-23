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
    Dia::Graphics::DebugFrameData& dbg = static_cast<Dia::Graphics::DebugFrameData&>(frameData);

    // Sample buffer — max 64 samples + 1 endpoint = 65 points, safe on the stack
    Dia::Maths::Vector3D points[65];
    const float invSamples = 1.0f / static_cast<float>(mArcSamples);

    // Point lights (colour: kHealthy / green)
    {
        const unsigned int count = mRegistry.GetPointCount();
        for (unsigned int i = 0; i < count; ++i)
        {
            const Dia::Core::StringCRC id = mRegistry.GetPointIdByIndex(i);
            LightPathBehaviour3D* behaviour = mRegistry.GetPathBehaviour(id);
            if (behaviour == nullptr)
                continue;

            const Dia::Geometry3D::Spline3D& spline = behaviour->GetSpline();
            for (int k = 0; k <= mArcSamples; ++k)
                points[k] = spline.Evaluate(k * invSamples);

            for (int k = 0; k < mArcSamples; ++k)
                dbg.RequestDrawLine3D(points[k], points[k + 1], Dia::Debug::DebugColourPalette::kHealthy);
        }
    }

    // Spot lights (colour: kHealthy / green)
    {
        const unsigned int count = mRegistry.GetSpotCount();
        for (unsigned int i = 0; i < count; ++i)
        {
            const Dia::Core::StringCRC id = mRegistry.GetSpotIdByIndex(i);
            LightPathBehaviour3D* behaviour = mRegistry.GetPathBehaviour(id);
            if (behaviour == nullptr)
                continue;

            const Dia::Geometry3D::Spline3D& spline = behaviour->GetSpline();
            for (int k = 0; k <= mArcSamples; ++k)
                points[k] = spline.Evaluate(k * invSamples);

            for (int k = 0; k < mArcSamples; ++k)
                dbg.RequestDrawLine3D(points[k], points[k + 1], Dia::Debug::DebugColourPalette::kHealthy);
        }
    }

    // Directional lights (colour: kGoal / cyan)
    {
        const unsigned int count = mRegistry.GetDirectionalCount();
        for (unsigned int i = 0; i < count; ++i)
        {
            const Dia::Core::StringCRC id = mRegistry.GetDirectionalIdByIndex(i);
            LightPathBehaviour3D* behaviour = mRegistry.GetPathBehaviour(id);
            if (behaviour == nullptr)
                continue;

            const Dia::Geometry3D::Spline3D& spline = behaviour->GetSpline();
            for (int k = 0; k <= mArcSamples; ++k)
                points[k] = spline.Evaluate(k * invSamples);

            for (int k = 0; k < mArcSamples; ++k)
                dbg.RequestDrawLine3D(points[k], points[k + 1], Dia::Debug::DebugColourPalette::kGoal);
        }
    }
}

void LightPathArcDrawer::DrawImGui()
{
    ImGui::SliderInt("Arc samples", &mArcSamples, 8, 64);
}

} } // namespace Dia::Lighting3D

#endif // DIA_DEBUG
