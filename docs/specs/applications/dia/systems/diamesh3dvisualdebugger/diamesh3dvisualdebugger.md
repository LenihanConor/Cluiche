# System Spec: DiaMesh3DVisualDebugger

## Parent Application
@docs/specs/applications/dia/dia.md

**Status:** `Approved`

---

## Purpose

DiaMesh3DVisualDebugger is the visual debug rendering module for 3D mesh instances. It reads from `Mesh3DFrameData` (the per-frame draw command list) and `Mesh3DAssetHandler` (for per-asset bounds and state), and writes debug geometry into DiaVisualDebugger. It follows the same sibling pattern as `DiaLighting3DVisualDebugger` and `DiaRigidBody2DVisualDebugger` — a separate static library with zero footprint in the main pipeline libraries.

Three focused `IVisualDebugger` draw classes cover all information naturally available from the data:
- `MeshBoundsDrawer` — model-space AABB translated to world position per draw command
- `MeshOriginDrawer` — axis cross at world position per draw command, colour-coded by layer and skinning state
- `MeshStatsDrawer` — ImGui-only stats panel (draw count, dropped count, per-layer breakdown, skinned vs static)

**Dependency chain:**
`DiaMesh3DVisualDebugger → DiaMesh3D + DiaGraphics3D + DiaVisualDebugger → DiaMaths → DiaCore`

No dependency on DiaBgfx3D or DiaGraphics — frame data and asset data are sufficient.

---

## Responsibilities

- Read `Mesh3DFrameData::GetMeshDraws()` each frame and submit debug geometry to DiaVisualDebugger
- Translate model-space AABBs to world position using `Matrix44::GetTranslation()`
- Colour-code bounds by asset load state: Ready (`kHealthy`), Pending (`kWarning`), Failed (`kError`), not found (`kInactive`)
- Draw axis cross at world origin of each draw command, colour by render layer, tinted for skinned instances
- Surface per-frame stats (draw count, dropped count, per-layer counts, skinned count) via ImGui shelf
- Use `DebugColourPalette` constants — no ad-hoc colour literals
- Multiply all visual sizes by `mManager.GetDebugScale()`
- Use `DebugLayerNames::kMesh3D*` canonical layer name constants
- Provide `DiaMesh3DVisualDebugger.vcxproj` static library project registered in `Cluiche.sln`
- Provide `dia.mesh3dvisualdebugger.architecture.module.md` YAML module documentation

## Non-Responsibilities

- Per-vertex normals, tangents, or wireframe drawing — requires vertex buffer access, out of scope
- Skinning palette visualisation — belongs in a future DiaSkinning3DVisualDebugger
- Shadow frustum or light space debug — belongs in DiaBgfx3DVisualDebugger
- Any mutation of draw commands or assets — strictly read-only

---

## Public Interfaces

### `MeshBoundsDrawer`

```cpp
namespace Dia::Mesh3D
{
    class MeshBoundsDrawer : public Dia::Debug::IVisualDebugger
    {
    public:
        MeshBoundsDrawer(const Dia::Graphics3D::Mesh3DFrameData&  frameData,
                         const Dia::Mesh3D::Mesh3DAssetHandler&   assetHandler,
                         const Dia::Debug::DebugLayerManager&     manager);

        Dia::Core::StringCRC GetLayerName() const override;   // LayerNames::kMesh3DBounds
        void Draw    (Dia::Graphics::FrameData& frameData) override;
    };
}
```

**Draw logic:**
- Iterate `mFrameData.GetMeshDraws()`
- Per command: `worldPos = cmd.transform.GetTranslation()`
- Look up asset: `asset = mAssetHandler.LookupMesh(cmd.meshId)`
- If `asset == nullptr`: draw unit cube wireframe at `worldPos`, colour `kInactive`
- Else colour by `asset->GetState()`:
  - `Ready` → `kHealthy` (green)
  - `Pending` → `kWarning` (yellow)
  - `Failed` → `kError` (red)
- Draw 12-edge wireframe box: `aabb.GetMin() + worldPos` … `aabb.GetMax() + worldPos` (translation only — no rotation/scale applied, matching what is actually stored)

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

        Dia::Core::StringCRC GetLayerName() const override;   // LayerNames::kMesh3DOrigins
        void Draw    (Dia::Graphics::FrameData& frameData) override;
        void DrawImGui() override;

    private:
        const Dia::Graphics3D::Mesh3DFrameData& mFrameData;
        const Dia::Debug::DebugLayerManager&    mManager;

        bool mHighlightSkinned = true;
    };
}
```

**Draw logic:**
- Iterate `mFrameData.GetMeshDraws()`
- Per command: `worldPos = cmd.transform.GetTranslation()`
- Colour: if `mHighlightSkinned && cmd.skinningPaletteIndex > 0` → `kPinned` (magenta); else colour by `cmd.layer` cycling through `{kActive, kGoal, kWarning, kCapped, kHealthy}` (layer index mod 5)
- Draw axis cross: 3 rays of length `kCrossArmLen * debugScale` along `+X`, `+Y`, `+Z` world axes from `worldPos`
- `kCrossArmLen = 0.2f` (static constexpr in .cpp)

**ImGui shelf:**
```cpp
void MeshOriginDrawer::DrawImGui()
{
    ImGui::Checkbox("Highlight skinned", &mHighlightSkinned);
}
```

---

### `MeshStatsDrawer`

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

**`Draw()` is a no-op** — this drawer exists only for its shelf.

**ImGui shelf — stats surfaced:**
- Total draw commands this frame (`GetMeshDraws().Size()`)
- Dropped mesh count (`DroppedMeshCount()`) — coloured red if > 0
- Loaded asset count (`assetHandler.GetLoadedCount()`)
- Skinned instance count (draw commands where `skinningPaletteIndex > 0`)
- Per-layer draw count breakdown (iterate draws, bucket by `cmd.layer`)
- Asset state breakdown: Ready / Pending / Failed counts (iterate draws, lookup each asset)

---

## New Layer Name Constants (add to `DebugLayerNames.h`)

```cpp
// ----------------------------------------------------------------
// Mesh 3D (priority tier 10–19)
// ----------------------------------------------------------------
inline const Dia::Core::StringCRC kMesh3DBounds  { "mesh3d.bounds"  };
inline const Dia::Core::StringCRC kMesh3DOrigins { "mesh3d.origins" };
inline const Dia::Core::StringCRC kMesh3DStats   { "mesh3d.stats"   };
```

---

## Registration Example

```cpp
static Dia::Mesh3D::MeshBoundsDrawer  boundsDrawer (mesh3dFrameData, assetHandler, debugManager);
static Dia::Mesh3D::MeshOriginDrawer  originDrawer (mesh3dFrameData, debugManager);
static Dia::Mesh3D::MeshStatsDrawer   statsDrawer  (mesh3dFrameData, assetHandler, debugManager);

debugManager.Register(&boundsDrawer,  10);
debugManager.Register(&originDrawer,  10);
debugManager.Register(&statsDrawer,   50);   // overlay tier — stats panel on top

// Each frame, after Mesh3DFrameData is populated and before it is cleared:
debugManager.Draw(frameData);
```

---

## Features

| Feature | Description | Spec |
|---|---|---|
| `mesh3d-bounds-and-origins` | `MeshBoundsDrawer` + `MeshOriginDrawer` + layer name constants + vcxproj + module YAML | [mesh3d-bounds-and-origins.md](mesh3d-bounds-and-origins.md) |
| `mesh3d-stats` | `MeshStatsDrawer` ImGui stats panel | [mesh3d-stats.md](mesh3d-stats.md) |

---

## Dependencies

| Dependency | What's used |
|---|---|
| DiaMesh3D | `Mesh3DAsset`, `Mesh3DAssetHandler::LookupMesh()`, `GetLoadedCount()`, `GetBounds()`, `GetState()` |
| DiaGraphics3D | `Mesh3DFrameData::GetMeshDraws()`, `DroppedMeshCount()`, `Mesh3DDrawCommand` fields |
| DiaVisualDebugger | `IVisualDebugger`, `DebugLayerManager`, `DebugColourPalette`, `DebugLayerNames` |
| DiaMaths | `Matrix44::GetTranslation()`, `Vector3D` |
| DiaCore | `StringCRC` |

### Does NOT depend on
- DiaBgfx3D or DiaBgfx (no GPU resource access)
- DiaGraphics (DiaVisualDebugger abstracts drawing)
- DiaApplicationFlow

---

## System Decisions

| ID | Decision | Rationale | Binding |
|----|----------|-----------|---------|
| SD-MVD-001 | Separate static library | DiaMesh3D and DiaGraphics3D must have zero DiaVisualDebugger dependency | Yes |
| SD-MVD-002 | Translation-only AABB placement | `GetBounds()` is model-space; only translation is extracted from the transform. Rotation/scale are not applied — this matches what is actually stored and avoids false precision | Yes |
| SD-MVD-003 | `MeshStatsDrawer::Draw()` is a no-op | Stats are frame-data reads surfaced only in ImGui; no world-space geometry to draw | No |
| SD-MVD-004 | Layer colour cycles over 5 palette entries | `cmd.layer` is an int16 with no semantic colour mapping; cycling the palette is more informative than a single colour | No |

---

## Inherited Binding Decisions

| ID | Decision | How it applies |
|---|---|---|
| PD-001 | StringCRC for IDs | Layer names are `LayerNames::kMesh3D*` constants |
| PD-004 | No STL in public API | Constructors and `Draw()` use refs and engine types only |
| PD-005 | x64 only | Single build target |
| PD-007 | C++20 required | Compiled under `/std:c++20` |
| PD-008 | Directory.Build.props owns build paths | No per-project OutDir/IntDir overrides |
| AD-001 | Module YAML frontmatter | `dia.mesh3dvisualdebugger.architecture.module.md` required |
| AD-003 | Namespace `Dia::<Module>::` | `Dia::Mesh3D::` (companion to DiaMesh3D, same domain namespace) |
| SD-DBG-001 | Stack of focused draw classes | Three focused draw classes |
| SD-DBG-003 | Priority-ordered draw | bounds/origins at 10, stats at 50 (overlay tier) |
| SD-DBG-005 | Global `debugScale` | Cross arm length multiplied by `mManager.GetDebugScale()` |
| SD-DBG-010 | `DebugColourPalette` colours | All colours use palette constants; no ad-hoc literals |
| SD-DBG-014 | Same-family classes share vcxproj | All three draw classes in `DiaMesh3DVisualDebugger.vcxproj` |

---

## Status

`Approved`
