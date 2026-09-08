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

} } // namespace Dia::Mesh3D

#endif // DIA_DEBUG
