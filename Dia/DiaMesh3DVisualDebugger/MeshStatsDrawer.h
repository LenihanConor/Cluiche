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

#include <DiaCore/DebugDraw/IVisualDebugger.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia { namespace Graphics3D { class Mesh3DFrameData; } }
namespace Dia { namespace Mesh3D     { class Mesh3DAssetHandler; } }
namespace Dia { namespace Core       { class IDebugContext; } }

namespace Dia { namespace Mesh3D {

////////////////////////////////////////////////////////////////////////////////
// MeshStatsDrawer
//
// Registered under layer name LayerNames::kMesh3DStats ("mesh3d.stats").
// Draw() is a no-op — stats display was removed with the ImGui console.
// Cached stats are still populated for potential future use.
////////////////////////////////////////////////////////////////////////////////
class MeshStatsDrawer : public Dia::Debug::IVisualDebugger
{
public:
    static constexpr int kMaxTrackedLayers = 32;
    struct LayerBucket { int16_t layer; uint32_t count; };

    MeshStatsDrawer(const Dia::Graphics3D::Mesh3DFrameData& frameData,
                    const Dia::Mesh3D::Mesh3DAssetHandler&  assetHandler,
                    const Dia::Core::IDebugContext&         manager);

    Dia::Core::StringCRC GetLayerName() const override;
    void Draw    (Dia::Core::IDebugDraw& draw) override;

    // Public read surface for DiaDebugPanel stats — written by Draw() on SimPU.
    uint32_t         GetCachedDrawCount()     const { return mCachedDrawCount; }
    uint32_t         GetCachedDroppedCount()  const { return mCachedDroppedCount; }
    uint32_t         GetCachedLoadedCount()   const { return mCachedLoadedCount; }
    uint32_t         GetCachedStaticCount()   const { return mCachedStaticCount; }
    uint32_t         GetCachedSkinnedCount()  const { return mCachedSkinnedCount; }
    int              GetCachedLayerCount()    const { return mCachedLayerCount; }
    LayerBucket      GetCachedLayer(int i)    const { return (i >= 0 && i < mCachedLayerCount) ? mCachedLayers[i] : LayerBucket{}; }
    uint32_t         GetCachedStateReady()    const { return mCachedStateReady; }
    uint32_t         GetCachedStatePending()  const { return mCachedStatePending; }
    uint32_t         GetCachedStateFailed()   const { return mCachedStateFailed; }
    uint32_t         GetCachedStateNotFound() const { return mCachedStateNotFound; }

private:
    const Dia::Graphics3D::Mesh3DFrameData& mFrameData;
    const Dia::Mesh3D::Mesh3DAssetHandler&  mAssetHandler;
    const Dia::Core::IDebugContext&         mManager;

    // Cached per-frame stats — written by Draw() on SimPU.
    LayerBucket mCachedLayers[kMaxTrackedLayers] = {};
    uint32_t    mCachedDrawCount     = 0;
    uint32_t    mCachedDroppedCount  = 0;
    uint32_t    mCachedLoadedCount   = 0;
    uint32_t    mCachedStaticCount   = 0;
    uint32_t    mCachedSkinnedCount  = 0;
    int         mCachedLayerCount    = 0;
    uint32_t    mCachedStateReady    = 0;
    uint32_t    mCachedStatePending  = 0;
    uint32_t    mCachedStateFailed   = 0;
    uint32_t    mCachedStateNotFound = 0;
};

} } // namespace Dia::Mesh3D

#endif // DIA_DEBUG
