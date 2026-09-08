# Feature Spec: mesh3d-stats

## Parent System
@docs/specs/applications/dia/systems/diamesh3dvisualdebugger/diamesh3dvisualdebugger.md

**Status:** `Done`

---

## Summary

Add `MeshStatsDrawer` — an ImGui-only `IVisualDebugger` that surfaces per-frame mesh pipeline statistics in the debug console shelf. `Draw()` is a no-op; all output is in `DrawImGui()`. Reads `Mesh3DFrameData` and `Mesh3DAssetHandler` to compute draw count, dropped count, loaded asset count, skinned vs static breakdown, per-layer counts, and asset state breakdown.

**Depends on:** `mesh3d-bounds-and-origins` (for vcxproj, sln, `kMesh3DStats` constant).

---

## Acceptance Criteria

1. `MeshStatsDrawer` implements `IVisualDebugger`, lives in `DiaMesh3DVisualDebugger.vcxproj`
2. `GetLayerName()` returns `LayerNames::kMesh3DStats`
3. `Draw()` is a no-op — emits zero primitives
4. `DrawImGui()` displays all stats listed below
5. Dropped mesh count displayed in `kError` colour when > 0, normal text otherwise
6. Build passes with no warnings; GoogleTests pass

---

## Draw Class

```cpp
namespace Dia::Mesh3D
{
    class MeshStatsDrawer : public Dia::Debug::IVisualDebugger
    {
    public:
        MeshStatsDrawer(const Dia::Graphics3D::Mesh3DFrameData& frameData,
                        const Dia::Mesh3D::Mesh3DAssetHandler&  assetHandler,
                        const Dia::Debug::DebugLayerManager&    manager);

        Dia::Core::StringCRC GetLayerName() const override;   // LayerNames::kMesh3DStats
        void Draw    (Dia::Graphics::FrameData& frameData) override;  // no-op
        void DrawImGui() override;

    private:
        const Dia::Graphics3D::Mesh3DFrameData& mFrameData;
        const Dia::Mesh3D::Mesh3DAssetHandler&  mAssetHandler;
        const Dia::Debug::DebugLayerManager&    mManager;
    };
}
```

---

## ImGui Shelf

Stats computed inline in `DrawImGui()` each frame from live data — no caching:

```
── Draw commands ──────────────────
  This frame:   42
  Dropped:       0        ← red if > 0
  Loaded assets: 7

── Instance breakdown ─────────────
  Static:        38
  Skinned:        4

── By render layer ────────────────
  Layer  0:      30
  Layer  1:      12

── Asset state ────────────────────
  Ready:          7
  Pending:        0
  Failed:         0
  Not found:      0
```

Implementation notes:
- Use `ImGui::TextDisabled()` for section headers
- Use `ImGui::TextColored(kError colour)` for dropped count when `DroppedMeshCount() > 0`
- Per-layer table: iterate draws, bucket by `cmd.layer` into a local fixed array (max 16 layers); render only layers with count > 0
- Asset state: iterate draws, call `LookupMesh()` per unique `meshId` (deduplicate by id to avoid double-counting shared assets)

---

## Tasks

| # | Task | Test |
|---|------|------|
| 1 | Implement `MeshStatsDrawer.h/.cpp`; add to `DiaMesh3DVisualDebugger.vcxproj` | Build passes |
| 2 | Write `Cluiche/Tests/GoogleTests/DiaMesh3D/TestMeshStatsDrawer.cpp`; add to `GoogleTests.vcxproj` | `dia run googletest --filter="DiaMesh3D_MeshStats*"` all pass |

---

## Test Plan

**File:** `Cluiche/Tests/GoogleTests/DiaMesh3D/TestMeshStatsDrawer.cpp`

Test setup: `Mesh3DFrameData` with known commands (varied layers, skinning indices, mesh IDs); stub `Mesh3DAssetHandler` with assets in each state.

| Suite | Test | What it verifies |
|-------|------|-----------------|
| MeshStatsDrawer | `LayerName_IsMesh3DStats` | `GetLayerName() == LayerNames::kMesh3DStats` |
| MeshStatsDrawer | `Draw_EmitsNoPrimitives` | `Draw()` → 0 primitives regardless of frame data |
| MeshStatsDrawer | `Draw_Disabled_EmitsNoPrimitives` | `SetEnabled(false)` → still 0 primitives |

Note: `DrawImGui()` is not unit-tested (requires ImGui context); the three tests above verify the structural contract. Stats correctness is validated via visual inspection in the running app.

---

## Inherited Binding Decisions

| ID | Decision | Compliance |
|----|----------|-----------|
| PD-001 | StringCRC for IDs | `GetLayerName()` returns `LayerNames::kMesh3DStats` |
| PD-004 | No STL in public API | Constructor uses refs; no STL in signatures |
| PD-007 | C++20 required | Compiled under `/std:c++20` |
| AD-003 | Namespace `Dia::<Module>::` | `Dia::Mesh3D::MeshStatsDrawer` |
| SD-DBG-001 | Stack of focused draw classes | One focused ImGui-only drawer |
| SD-DBG-003 | Priority-ordered draw | Registered at priority 50 (overlay tier) |
| SD-DBG-014 | Same-family classes share vcxproj | Lives in `DiaMesh3DVisualDebugger.vcxproj` |
| SD-MVD-003 | `Draw()` is a no-op | Stats surfaced only via ImGui shelf |

---

## Status

`Approved`
