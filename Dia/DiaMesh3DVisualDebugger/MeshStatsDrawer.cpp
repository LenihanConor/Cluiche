////////////////////////////////////////////////////////////////////////////////
// Filename: MeshStatsDrawer.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaMesh3DVisualDebugger/MeshStatsDrawer.h"

#ifdef DIA_DEBUG

#include <DiaCore/DebugDraw/IDebugDraw.h>
#include <DiaGraphics3D/Mesh3DFrameData.h>
#include <DiaMesh3D/Mesh3DAssetHandler.h>
#include <DiaMesh3D/Mesh3DAsset.h>
#include <DiaVisualDebugger/DebugLayerNames.h>
#include <DiaVisualDebugger/DebugColourPalette.h>
#include <DiaVisualDebugger/DebugLayerManager.h>
#include <imgui.h>

namespace Dia { namespace Mesh3D {

MeshStatsDrawer::MeshStatsDrawer(const Dia::Graphics3D::Mesh3DFrameData& frameData,
                                 const Dia::Mesh3D::Mesh3DAssetHandler&  assetHandler,
                                 const Dia::Debug::DebugLayerManager&    manager)
    : mFrameData(frameData)
    , mAssetHandler(assetHandler)
    , mManager(manager)
{}

Dia::Core::StringCRC MeshStatsDrawer::GetLayerName() const
{
    return Dia::Debug::LayerNames::kMesh3DStats;
}

void MeshStatsDrawer::Draw(Dia::Core::IDebugDraw& /*draw*/)
{
}

void MeshStatsDrawer::DrawImGui()
{
    const auto& draws      = mFrameData.GetMeshDraws();
    const uint32_t drawCount    = draws.Size();
    const uint32_t droppedCount = mFrameData.DroppedMeshCount();
    const uint32_t loadedCount  = mAssetHandler.GetLoadedCount();

    // ── Draw commands ──────────────────────────────────────────────────────
    ImGui::TextDisabled("── Draw commands ──────────────────");
    ImGui::Text("  This frame:   %u", drawCount);
    if (droppedCount > 0)
        ImGui::TextColored(ImVec4(220 / 255.0f, 0.0f, 0.0f, 1.0f), "  Dropped:      %u", droppedCount);
    else
        ImGui::Text("  Dropped:      %u", droppedCount);
    ImGui::Text("  Loaded assets: %u", loadedCount);

    // ── Instance breakdown ─────────────────────────────────────────────────
    uint32_t skinnedCount = 0;
    for (uint32_t i = 0; i < drawCount; ++i)
        if (draws[i].skinningPaletteIndex > 0) ++skinnedCount;
    const uint32_t staticCount = drawCount - skinnedCount;

    ImGui::TextDisabled("── Instance breakdown ─────────────");
    ImGui::Text("  Static:        %u", staticCount);
    ImGui::Text("  Skinned:       %u", skinnedCount);

    // ── By render layer ────────────────────────────────────────────────────
    static constexpr int kMaxTrackedLayers = 32;
    int16_t  seenLayers[kMaxTrackedLayers]  = {};
    uint32_t layerCounts[kMaxTrackedLayers] = {};
    int layerCount = 0;

    for (uint32_t i = 0; i < drawCount; ++i)
    {
        const int16_t layer = draws[i].layer;
        int slot = -1;
        for (int j = 0; j < layerCount; ++j)
            if (seenLayers[j] == layer) { slot = j; break; }
        if (slot < 0 && layerCount < kMaxTrackedLayers)
        {
            slot = layerCount++;
            seenLayers[slot]  = layer;
            layerCounts[slot] = 0;
        }
        if (slot >= 0) ++layerCounts[slot];
    }

    ImGui::TextDisabled("── By render layer ────────────────");
    for (int j = 0; j < layerCount; ++j)
        if (layerCounts[j] > 0)
            ImGui::Text("  Layer %3d:    %u", seenLayers[j], layerCounts[j]);

    // ── Asset state ────────────────────────────────────────────────────────
    static constexpr uint32_t kMaxUniqueAssets = 256;
    Dia::Core::StringCRC seenIds[kMaxUniqueAssets];
    uint32_t seenCount    = 0;
    uint32_t stateReady    = 0;
    uint32_t statePending  = 0;
    uint32_t stateFailed   = 0;
    uint32_t stateNotFound = 0;

    for (uint32_t i = 0; i < drawCount; ++i)
    {
        const Dia::Core::StringCRC id = draws[i].meshId;
        bool found = false;
        for (uint32_t j = 0; j < seenCount; ++j)
            if (seenIds[j] == id) { found = true; break; }
        if (found) continue;
        if (seenCount < kMaxUniqueAssets) seenIds[seenCount++] = id;

        const Dia::Mesh3D::Mesh3DAsset* asset = mAssetHandler.LookupMesh(id);
        if (!asset) { ++stateNotFound; continue; }
        switch (asset->GetState())
        {
            case Dia::Mesh3D::Mesh3DAsset::State::Ready:   ++stateReady;   break;
            case Dia::Mesh3D::Mesh3DAsset::State::Pending: ++statePending; break;
            case Dia::Mesh3D::Mesh3DAsset::State::Failed:  ++stateFailed;  break;
            default: break;
        }
    }

    ImGui::TextDisabled("── Asset state ────────────────────");
    ImGui::Text("  Ready:         %u", stateReady);
    ImGui::Text("  Pending:       %u", statePending);
    ImGui::Text("  Failed:        %u", stateFailed);
    ImGui::Text("  Not found:     %u", stateNotFound);
}

} } // namespace Dia::Mesh3D

#endif // DIA_DEBUG
