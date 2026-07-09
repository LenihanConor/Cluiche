////////////////////////////////////////////////////////////////////////////////
// Filename: Coord3DAxesDrawer.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaVisualDebugger/Coord3D/Coord3DAxesDrawer.h"

#ifdef DIA_DEBUG

#include <DiaCore/DebugDraw/IDebugDraw.h>
#include <DiaGraphics3D/Camera3D.h>
#include <DiaMaths/Vector/Vector3D.h>
#include <DiaMaths/Matrix/Matrix44.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <imgui.h>
#include "DiaVisualDebugger/DebugLayerManager.h"
#include "DiaVisualDebugger/DebugColourPalette.h"
#include "DiaVisualDebugger/DebugLayerNames.h"

namespace Dia::Debug
{

Coord3DAxesDrawer::Coord3DAxesDrawer(const Dia::Debug::DebugLayerManager& manager)
    : mManager(manager)
{}

Dia::Core::StringCRC Coord3DAxesDrawer::GetLayerName() const
{
    return Dia::Debug::LayerNames::kCoord3DAxes;
}

void Coord3DAxesDrawer::Draw(Dia::Core::IDebugDraw& draw)
{
    DIA_TRACE_ZONE("coord3d.axes", ::Dia::Observation::Trace::Category::kDiaGraphics);

    const Dia::Graphics3D::Camera3D& cam = mManager.GetCamera3D();

    // Camera forward in world space: third row of the view matrix (negated for OpenGL-style).
    // Row-major convention (DiaMaths SD-005): view[2] = right-handed forward.
    // Extract forward from view matrix row 2 (the Z row maps world->view Z).
    // In a right-handed camera, the camera looks down -Z in view space,
    // so world-space forward = -view row 2.
    const Dia::Maths::Vector3D forward(
        -cam.view.m[2][0],
        -cam.view.m[2][1],
        -cam.view.m[2][2]);

    const float N = mExtent;
    const Dia::Maths::Vector3D origin(0.0f, 0.0f, 0.0f);

    // X axis: red. Draw positive arm if +X faces camera, negative if -X faces camera.
    {
        const Dia::Maths::Vector3D posX(1.0f, 0.0f, 0.0f);
        const float dotPos = posX.x * forward.x + posX.y * forward.y + posX.z * forward.z;
        if (dotPos >= 0.0f)
            draw.RequestDrawLine3D(origin, Dia::Maths::Vector3D( N, 0.0f, 0.0f), Dia::Debug::DebugColourPalette::kError);
        else
            draw.RequestDrawLine3D(origin, Dia::Maths::Vector3D(-N, 0.0f, 0.0f), Dia::Debug::DebugColourPalette::kError);
    }

    // Y axis: green.
    {
        const Dia::Maths::Vector3D posY(0.0f, 1.0f, 0.0f);
        const float dotPos = posY.x * forward.x + posY.y * forward.y + posY.z * forward.z;
        if (dotPos >= 0.0f)
            draw.RequestDrawLine3D(origin, Dia::Maths::Vector3D(0.0f,  N, 0.0f), Dia::Debug::DebugColourPalette::kHealthy);
        else
            draw.RequestDrawLine3D(origin, Dia::Maths::Vector3D(0.0f, -N, 0.0f), Dia::Debug::DebugColourPalette::kHealthy);
    }

    // Z axis: blue/cyan.
    {
        const Dia::Maths::Vector3D posZ(0.0f, 0.0f, 1.0f);
        const float dotPos = posZ.x * forward.x + posZ.y * forward.y + posZ.z * forward.z;
        if (dotPos >= 0.0f)
            draw.RequestDrawLine3D(origin, Dia::Maths::Vector3D(0.0f, 0.0f,  N), Dia::Debug::DebugColourPalette::kGoal);
        else
            draw.RequestDrawLine3D(origin, Dia::Maths::Vector3D(0.0f, 0.0f, -N), Dia::Debug::DebugColourPalette::kGoal);
    }
}

void Coord3DAxesDrawer::DrawImGui()
{
    ImGui::SliderFloat("Extent (world units)", &mExtent, 1.0f, 1000.0f);
}

} // namespace Dia::Debug

#endif // DIA_DEBUG
