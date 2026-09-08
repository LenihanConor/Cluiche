# Feature Spec: mesh3d-bounds-and-origins

## Parent System
@docs/specs/applications/dia/systems/diamesh3dvisualdebugger/diamesh3dvisualdebugger.md

**Status:** `Done`

---

## Summary

Add `MeshBoundsDrawer` and `MeshOriginDrawer` — the two world-space draw classes for `DiaMesh3DVisualDebugger`. Also covers all foundational setup: vcxproj, sln registration, module YAML, and the three new `DebugLayerNames` constants.

`MeshBoundsDrawer` draws a wireframe AABB box at each draw command's world position, colour-coded by asset load state. `MeshOriginDrawer` draws a small axis cross at each world position, colour-coded by render layer with an optional skinned-instance highlight.

---

## Acceptance Criteria

1. `MeshBoundsDrawer` implements `IVisualDebugger`, lives in `DiaMesh3DVisualDebugger.vcxproj`
2. `MeshOriginDrawer` implements `IVisualDebugger`, lives in `DiaMesh3DVisualDebugger.vcxproj`
3. Both store dependencies at construction — no per-call arguments
4. `MeshBoundsDrawer::GetLayerName()` returns `LayerNames::kMesh3DBounds`
5. `MeshOriginDrawer::GetLayerName()` returns `LayerNames::kMesh3DOrigins`
6. Bounds box drawn at `aabb.GetMin() + worldPos` … `aabb.GetMax() + worldPos` (translation only, per SD-MVD-002)
7. Bounds colour: `kHealthy` (Ready), `kWarning` (Pending), `kError` (Failed), `kInactive` (not found)
8. Origin cross: 3 rays along world `+X/+Y/+Z`, length `kCrossArmLen * debugScale`
9. Origin colour: `kPinned` for skinned instances (when `mHighlightSkinned`), else cycles palette by `cmd.layer % 5`
10. `MeshOriginDrawer::DrawImGui()` exposes `"Highlight skinned"` checkbox
11. `kMesh3DBounds`, `kMesh3DOrigins`, `kMesh3DStats` constants added to `DebugLayerNames.h`
12. `DiaMesh3DVisualDebugger.vcxproj` created with `DiaVisualDebugger`, `DiaMesh3D`, `DiaGraphics3D` project references; registered in `Cluiche.sln`
13. `dia.mesh3dvisualdebugger.architecture.module.md` created
14. Build passes with no warnings; GoogleTests pass

---

## Draw Classes

### `MeshBoundsDrawer`

```cpp
namespace Dia::Mesh3D
{
    class MeshBoundsDrawer : public Dia::Debug::IVisualDebugger
    {
    public:
        MeshBoundsDrawer(const Dia::Graphics3D::Mesh3DFrameData& frameData,
                         const Dia::Mesh3D::Mesh3DAssetHandler&  assetHandler,
                         const Dia::Debug::DebugLayerManager&    manager);

        Dia::Core::StringCRC GetLayerName() const override;
        void Draw(Dia::Graphics::FrameData& frameData) override;

    private:
        const Dia::Graphics3D::Mesh3DFrameData& mFrameData;
        const Dia::Mesh3D::Mesh3DAssetHandler&  mAssetHandler;
        const Dia::Debug::DebugLayerManager&    mManager;
    };
}
```

**Draw logic per draw command:**
1. `worldPos = cmd.transform.GetTranslation()`
2. `asset = mAssetHandler.LookupMesh(cmd.meshId)`
3. If `asset == nullptr`: use unit cube (`min={-0.5,-0.5,-0.5}`, `max={0.5,0.5,0.5}`), colour `kInactive`
4. Else pick colour by `asset->GetState()`: Ready→`kHealthy`, Pending→`kWarning`, Failed→`kError`
5. Draw 12 edges of the wireframe box: 8 corners = all combinations of `{min.x,max.x} × {min.y,max.y} × {min.z,max.z}` offset by `worldPos`; emit `RequestDrawLine` for each of the 12 edges

---

### `MeshOriginDrawer`

```cpp
namespace Dia::Mesh3D
{
    class MeshOriginDrawer : public Dia::Debug::IVisualDebugger
    {
    public:
        MeshOriginDrawer(const Dia::Graphics3D::Mesh3DFrameData& frameData,
                         const Dia::Debug::DebugLayerManager&    manager);

        Dia::Core::StringCRC GetLayerName() const override;
        void Draw    (Dia::Graphics::FrameData& frameData) override;
        void DrawImGui() override;

    private:
        const Dia::Graphics3D::Mesh3DFrameData& mFrameData;
        const Dia::Debug::DebugLayerManager&    mManager;

        bool mHighlightSkinned = true;
    };
}
```

**Draw logic per draw command:**
1. `worldPos = cmd.transform.GetTranslation()`
2. `scale = mManager.GetDebugScale()`
3. Colour: if `mHighlightSkinned && cmd.skinningPaletteIndex > 0` → `kPinned`; else `kLayerColours[cmd.layer % 5]` where `kLayerColours = { kActive, kGoal, kWarning, kCapped, kHealthy }`
4. Emit 3 rays: `RequestDrawRay(worldPos, {1,0,0}, kCrossArmLen * scale, colour)`, same for `{0,1,0}`, `{0,0,1}`
5. `kCrossArmLen = 0.2f` (static constexpr in .cpp)

**ImGui shelf:**
```cpp
void MeshOriginDrawer::DrawImGui()
{
    ImGui::Checkbox("Highlight skinned", &mHighlightSkinned);
}
```

---

## New Layer Name Constants

Add to `Dia/DiaVisualDebugger/DebugLayerNames.h` under a new `Mesh 3D` section:

```cpp
// ----------------------------------------------------------------
// Mesh 3D (priority tier 10–19)
// ----------------------------------------------------------------
inline const Dia::Core::StringCRC kMesh3DBounds  { "mesh3d.bounds"  };
inline const Dia::Core::StringCRC kMesh3DOrigins { "mesh3d.origins" };
inline const Dia::Core::StringCRC kMesh3DStats   { "mesh3d.stats"   };
```

All three constants added here (not split across features) — `DebugLayerNames.h` is the single source of truth.

---

## Tasks

| # | Task | Test |
|---|------|------|
| 1 | Add `kMesh3DBounds`, `kMesh3DOrigins`, `kMesh3DStats` to `DebugLayerNames.h` | Build passes |
| 2 | Create `Dia/DiaMesh3DVisualDebugger/` directory; create `DiaMesh3DVisualDebugger.vcxproj` and `.vcxproj.filters`; add `DiaVisualDebugger`, `DiaMesh3D`, `DiaGraphics3D` project references; register in `Cluiche.sln` | Build passes |
| 3 | Create `dia.mesh3dvisualdebugger.architecture.module.md` YAML module doc | `dia docs registry` passes |
| 4 | Implement `MeshBoundsDrawer.h/.cpp`; add to vcxproj | Build passes |
| 5 | Implement `MeshOriginDrawer.h/.cpp`; add to vcxproj | Build passes |
| 6 | Write `Cluiche/Tests/GoogleTests/DiaMesh3D/TestMeshBoundsDrawer.cpp` and `TestMeshOriginDrawer.cpp`; add to `GoogleTests.vcxproj` | `dia run googletest --filter="DiaMesh3D_Mesh*"` all pass |

---

## Test Plan

**Files:** `Cluiche/Tests/GoogleTests/DiaMesh3D/TestMeshBoundsDrawer.cpp`, `TestMeshOriginDrawer.cpp`

Test setup: a `Mesh3DFrameData` with known draw commands (identity transform, known mesh IDs), a stub `Mesh3DAssetHandler` with pre-registered assets of each state.

| Suite | Test | What it verifies |
|-------|------|-----------------|
| MeshBoundsDrawer | `LayerName_IsMesh3DBounds` | `GetLayerName() == LayerNames::kMesh3DBounds` |
| MeshBoundsDrawer | `Draw_ReadyAsset_Emits12Lines` | Ready asset → 12 line primitives (wireframe box) |
| MeshBoundsDrawer | `Draw_ReadyAsset_Colour_IsHealthy` | Line colour == `kHealthy` |
| MeshBoundsDrawer | `Draw_PendingAsset_Colour_IsWarning` | Pending asset → `kWarning` |
| MeshBoundsDrawer | `Draw_FailedAsset_Colour_IsError` | Failed asset → `kError` |
| MeshBoundsDrawer | `Draw_UnknownMeshId_Colour_IsInactive` | No asset found → `kInactive`, unit cube drawn |
| MeshBoundsDrawer | `Draw_BoxOffset_MatchesWorldPosition` | Box corners offset by transform translation |
| MeshBoundsDrawer | `Draw_Disabled_NoPrimitives` | `SetEnabled(false)` → 0 primitives |
| MeshBoundsDrawer | `Draw_EmptyFrameData_NoPrimitives` | No draw commands → 0 primitives |
| MeshOriginDrawer | `LayerName_IsMesh3DOrigins` | `GetLayerName() == LayerNames::kMesh3DOrigins` |
| MeshOriginDrawer | `Draw_OneCommand_Emits3Rays` | 1 draw command → 3 ray primitives |
| MeshOriginDrawer | `Draw_SkinnedHighlight_IsKPinned` | `skinningPaletteIndex > 0`, `mHighlightSkinned=true` → `kPinned` |
| MeshOriginDrawer | `Draw_SkinnedHighlightOff_CyclesPalette` | `mHighlightSkinned=false` → cycles palette by layer |
| MeshOriginDrawer | `Draw_Layer0_IsKActive` | `cmd.layer=0` → `kActive` |
| MeshOriginDrawer | `Draw_DebugScale_AffectsArmLength` | `debugScale=2.0f` → ray length doubles |
| MeshOriginDrawer | `Draw_Disabled_NoPrimitives` | `SetEnabled(false)` → 0 primitives |

---

## Inherited Binding Decisions

| ID | Decision | Compliance |
|----|----------|-----------|
| PD-001 | StringCRC for IDs | Layer names use `LayerNames::kMesh3D*` constants |
| PD-004 | No STL in public API | Constructors and `Draw()` use refs and engine types only |
| PD-007 | C++20 required | Compiled under `/std:c++20` |
| AD-001 | Module YAML frontmatter | Created in Task 3 |
| AD-003 | Namespace `Dia::<Module>::` | `Dia::Mesh3D::MeshBoundsDrawer`, `Dia::Mesh3D::MeshOriginDrawer` |
| SD-DBG-001 | Stack of focused draw classes | Two focused draw classes; `MeshStatsDrawer` is the third |
| SD-DBG-003 | Priority-ordered draw | Both registered at priority 10 |
| SD-DBG-005 | Global `debugScale` | Cross arm length multiplied by `mManager.GetDebugScale()` |
| SD-DBG-010 | `DebugColourPalette` colours | All colours use palette constants |
| SD-DBG-014 | Same-family classes share vcxproj | Both in `DiaMesh3DVisualDebugger.vcxproj` |
| SD-MVD-001 | Separate static library | `DiaMesh3DVisualDebugger.vcxproj` created in Task 2 |
| SD-MVD-002 | Translation-only AABB placement | Only `GetTranslation()` used from transform — no rotation/scale applied |

---

## Status

`Approved`
