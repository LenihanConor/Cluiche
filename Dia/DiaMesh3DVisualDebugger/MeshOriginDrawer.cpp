////////////////////////////////////////////////////////////////////////////////
// Filename: MeshOriginDrawer.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaMesh3DVisualDebugger/MeshOriginDrawer.h"

#ifdef DIA_DEBUG

#include <DiaGraphics/Frame/FrameData.h>
#include <DiaGraphics/Frame/DebugFrameData.h>
#include <DiaGraphics3D/Mesh3DFrameData.h>
#include <DiaVisualDebugger/DebugLayerNames.h>
#include <DiaVisualDebugger/DebugColourPalette.h>
#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaMaths/Vector/Vector3D.h>
#include <imgui.h>

namespace Dia { namespace Mesh3D {

static constexpr float kCrossArmLen = 0.2f;

MeshOriginDrawer::MeshOriginDrawer(const Dia::Graphics3D::Mesh3DFrameData& frameData,
                                   const Dia::Debug::DebugLayerManager& manager)
    : mFrameData(frameData)
    , mManager(manager)
{}

Dia::Core::StringCRC MeshOriginDrawer::GetLayerName() const
{
    return Dia::Debug::LayerNames::kMesh3DOrigins;
}

void MeshOriginDrawer::Draw(Dia::Graphics::FrameData& frameData)
{
    static const Dia::Graphics::RGBA kPalette[5] = {
        Dia::Debug::DebugColourPalette::kActive,
        Dia::Debug::DebugColourPalette::kGoal,
        Dia::Debug::DebugColourPalette::kWarning,
        Dia::Debug::DebugColourPalette::kCapped,
        Dia::Debug::DebugColourPalette::kHealthy
    };

    Dia::Graphics::DebugFrameData& dbg = static_cast<Dia::Graphics::DebugFrameData&>(frameData);

    const float armLen = kCrossArmLen * mManager.GetDebugScale();

    const auto& draws = mFrameData.GetMeshDraws();
    for (uint32_t i = 0; i < draws.Size(); ++i)
    {
        const Dia::Graphics3D::Mesh3DDrawCommand& cmd = draws[i];

        const Dia::Maths::Vector3D worldPos = cmd.transform.GetTranslation();

        Dia::Graphics::RGBA colour;
        if (mHighlightSkinned && cmd.skinningPaletteIndex > 0)
        {
            colour = Dia::Debug::DebugColourPalette::kPinned;
        }
        else
        {
            int idx = ((cmd.layer % 5) + 5) % 5;
            colour = kPalette[idx];
        }

        dbg.RequestDrawRay3D(worldPos, Dia::Maths::Vector3D(1.0f, 0.0f, 0.0f), armLen, colour);  // +X
        dbg.RequestDrawRay3D(worldPos, Dia::Maths::Vector3D(0.0f, 1.0f, 0.0f), armLen, colour);  // +Y
        dbg.RequestDrawRay3D(worldPos, Dia::Maths::Vector3D(0.0f, 0.0f, 1.0f), armLen, colour);  // +Z
    }
}

void MeshOriginDrawer::DrawImGui()
{
    ImGui::Checkbox("Highlight skinned", &mHighlightSkinned);
}

} } // namespace Dia::Mesh3D

#endif // DIA_DEBUG
