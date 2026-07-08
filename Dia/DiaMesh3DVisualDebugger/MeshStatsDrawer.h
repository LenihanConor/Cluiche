////////////////////////////////////////////////////////////////////////////////
// Filename: MeshStatsDrawer.h
// Description: IVisualDebugger that displays per-frame mesh draw statistics in
//              an ImGui panel: draw counts, dropped draws, instance breakdown,
//              per-layer counts, and asset load-state summary.
// Feature spec: docs/specs/applications/dia/systems/diamesh3dvisualdebugger/
//               mesh3d-render-system-stage.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/IVisualDebugger.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia { namespace Graphics3D { class Mesh3DFrameData; } }
namespace Dia { namespace Mesh3D     { class Mesh3DAssetHandler; } }
namespace Dia { namespace Debug      { class DebugLayerManager; } }

namespace Dia { namespace Mesh3D {

////////////////////////////////////////////////////////////////////////////////
// MeshStatsDrawer
//
// Registered under layer name LayerNames::kMesh3DStats ("mesh3d.stats").
// Draw() is a strict no-op — all output is via DrawImGui().
// Stats are computed inline from live refs each frame (no caching).
////////////////////////////////////////////////////////////////////////////////
class MeshStatsDrawer : public Dia::Debug::IVisualDebugger
{
public:
    MeshStatsDrawer(const Dia::Graphics3D::Mesh3DFrameData& frameData,
                    const Dia::Mesh3D::Mesh3DAssetHandler&  assetHandler,
                    const Dia::Debug::DebugLayerManager&    manager);

    Dia::Core::StringCRC GetLayerName() const override;
    void Draw    (Dia::Core::IDebugDraw& draw) override;
    void DrawImGui() override;

private:
    const Dia::Graphics3D::Mesh3DFrameData& mFrameData;
    const Dia::Mesh3D::Mesh3DAssetHandler&  mAssetHandler;
    const Dia::Debug::DebugLayerManager&    mManager;
};

} } // namespace Dia::Mesh3D

#endif // DIA_DEBUG
