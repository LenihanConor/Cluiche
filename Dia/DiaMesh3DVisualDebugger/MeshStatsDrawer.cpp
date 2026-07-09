////////////////////////////////////////////////////////////////////////////////
// Filename: MeshStatsDrawer.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaMesh3DVisualDebugger/MeshStatsDrawer.h"

#ifdef DIA_DEBUG

#include <DiaCore/DebugDraw/IDebugDraw.h>
#include <DiaGraphics3D/Mesh3DFrameData.h>
#include <DiaMesh3D/Mesh3DAssetHandler.h>
#include <DiaMesh3D/Mesh3DAsset.h>
#include <DiaCore/DebugDraw/DebugLayerNames.h>
#include <DiaCore/DebugDraw/DebugColourPalette.h>
#include <DiaCore/DebugDraw/IDebugContext.h>
#include <imgui.h>

namespace Dia { namespace Mesh3D {

MeshStatsDrawer::MeshStatsDrawer(const Dia::Graphics3D::Mesh3DFrameData& frameData,
                                 const Dia::Mesh3D::Mesh3DAssetHandler&  assetHandler,
                                 const Dia::Core::IDebugContext&         manager)
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
    // Populate the cache on SimPU while mFrameData is fully built and stable.
    // DrawImGui() runs on RenderPU and must not touch mFrameData directly.
    const auto& draws = mFrameData.GetMeshDraws();
    mCachedDrawCount    = draws.Size();
    mCachedDroppedCount = mFrameData.DroppedMeshCount();
    mCachedLoadedCount  = mAssetHandler.GetLoadedCount();

    mCachedSkinnedCount = 0;
    for (uint32_t i = 0; i < mCachedDrawCount; ++i)
        if (draws[i].skinningPaletteIndex > 0) ++mCachedSkinnedCount;
    mCachedStaticCount = mCachedDrawCount - mCachedSkinnedCount;

    mCachedLayerCount = 0;
    for (uint32_t i = 0; i < mCachedDrawCount; ++i)
    {
        const int16_t layer = draws[i].layer;
        int slot = -1;
        for (int j = 0; j < mCachedLayerCount; ++j)
            if (mCachedLayers[j].layer == layer) { slot = j; break; }
        if (slot < 0 && mCachedLayerCount < kMaxTrackedLayers)
        {
            slot = mCachedLayerCount++;
            mCachedLayers[slot] = { layer, 0 };
        }
        if (slot >= 0) ++mCachedLayers[slot].count;
    }

    static constexpr uint32_t kMaxUniqueAssets = 256;
    Dia::Core::StringCRC seenIds[kMaxUniqueAssets];
    uint32_t seenCount = 0;
    mCachedStateReady    = 0;
    mCachedStatePending  = 0;
    mCachedStateFailed   = 0;
    mCachedStateNotFound = 0;
    for (uint32_t i = 0; i < mCachedDrawCount; ++i)
    {
        const Dia::Core::StringCRC id = draws[i].meshId;
        bool found = false;
        for (uint32_t j = 0; j < seenCount; ++j)
            if (seenIds[j] == id) { found = true; break; }
        if (found) continue;
        if (seenCount < kMaxUniqueAssets) seenIds[seenCount++] = id;

        const Dia::Mesh3D::Mesh3DAsset* asset = mAssetHandler.LookupMesh(id);
        if (!asset) { ++mCachedStateNotFound; continue; }
        switch (asset->GetState())
        {
            case Dia::Mesh3D::Mesh3DAsset::State::Ready:   ++mCachedStateReady;   break;
            case Dia::Mesh3D::Mesh3DAsset::State::Pending: ++mCachedStatePending; break;
            case Dia::Mesh3D::Mesh3DAsset::State::Failed:  ++mCachedStateFailed;  break;
            default: break;
        }
    }
}

void MeshStatsDrawer::DrawImGui()
{
    // Reads only the cached snapshot populated by Draw() on SimPU — no cross-PU access.
    ImGui::TextDisabled("── Draw commands ──────────────────");
    ImGui::Text("  This frame:   %u", mCachedDrawCount);
    if (mCachedDroppedCount > 0)
        ImGui::TextColored(ImVec4(220 / 255.0f, 0.0f, 0.0f, 1.0f), "  Dropped:      %u", mCachedDroppedCount);
    else
        ImGui::Text("  Dropped:      %u", mCachedDroppedCount);
    ImGui::Text("  Loaded assets: %u", mCachedLoadedCount);

    ImGui::TextDisabled("── Instance breakdown ─────────────");
    ImGui::Text("  Static:        %u", mCachedStaticCount);
    ImGui::Text("  Skinned:       %u", mCachedSkinnedCount);

    ImGui::TextDisabled("── By render layer ────────────────");
    for (int j = 0; j < mCachedLayerCount; ++j)
        if (mCachedLayers[j].count > 0)
            ImGui::Text("  Layer %3d:    %u", mCachedLayers[j].layer, mCachedLayers[j].count);

    ImGui::TextDisabled("── Asset state ────────────────────");
    ImGui::Text("  Ready:         %u", mCachedStateReady);
    ImGui::Text("  Pending:       %u", mCachedStatePending);
    ImGui::Text("  Failed:        %u", mCachedStateFailed);
    ImGui::Text("  Not found:     %u", mCachedStateNotFound);
}

} } // namespace Dia::Mesh3D

#endif // DIA_DEBUG
