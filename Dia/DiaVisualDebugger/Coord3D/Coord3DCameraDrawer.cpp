////////////////////////////////////////////////////////////////////////////////
// Filename: Coord3DCameraDrawer.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaVisualDebugger/Coord3D/Coord3DCameraDrawer.h"

#ifdef DIA_DEBUG

#include <DiaCore/DebugDraw/IDebugDraw.h>
#include <DiaGraphics3D/Camera3D.h>
#include <imgui.h>
#include "DiaVisualDebugger/DebugLayerManager.h"
#include "DiaVisualDebugger/DebugLayerNames.h"

namespace Dia::Debug
{

Coord3DCameraDrawer::Coord3DCameraDrawer(const Dia::Debug::DebugLayerManager& manager)
    : mManager(manager)
{}

Dia::Core::StringCRC Coord3DCameraDrawer::GetLayerName() const
{
    return Dia::Debug::LayerNames::kCoord3DCamera;
}

void Coord3DCameraDrawer::Draw(Dia::Core::IDebugDraw& /*draw*/)
{
    // No geometry — all output is in DrawImGui()
}

void Coord3DCameraDrawer::DrawImGui()
{
    const Dia::Graphics3D::Camera3D& cam = mManager.GetCamera3D();
    const auto& v = cam.view;

    // Eye position: for a row-major view matrix V=R*T, eye = -R^T * t
    // where t = (v.m[3][0], v.m[3][1], v.m[3][2]).
    // R^T applied to t: multiply each row of R by corresponding t component.
    const float eyeX = -(v.m[0][0]*v.m[3][0] + v.m[1][0]*v.m[3][1] + v.m[2][0]*v.m[3][2]);
    const float eyeY = -(v.m[0][1]*v.m[3][0] + v.m[1][1]*v.m[3][1] + v.m[2][1]*v.m[3][2]);
    const float eyeZ = -(v.m[0][2]*v.m[3][0] + v.m[1][2]*v.m[3][1] + v.m[2][2]*v.m[3][2]);

    // Forward (camera looks down -Z in view space; world-space forward = -row2 of view)
    const float fwdX = -v.m[2][0];
    const float fwdY = -v.m[2][1];
    const float fwdZ = -v.m[2][2];

    ImGui::Text("Eye:     (%.2f, %.2f, %.2f)", eyeX, eyeY, eyeZ);
    ImGui::Text("Forward: (%.2f, %.2f, %.2f)", fwdX, fwdY, fwdZ);
}

} // namespace Dia::Debug

#endif // DIA_DEBUG
