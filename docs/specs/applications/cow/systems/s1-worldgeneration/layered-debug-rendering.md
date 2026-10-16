# Feature Spec: Layered Debug Rendering

**Parent:** @docs/specs/applications/cow/systems/s1-worldgeneration/worldgenerationpipeline.md

**Gameplay Domains:** simulation

## Summary

Layered Debug Rendering is S-1's in-game visual debugger: a `WorldGenDebugDomain` (`IDebugDomain`, DiaDebugDomain) whose world-space drawers show **what the generated data is and why it passed or failed**. It goes beyond "does this placement look broken?":

- The tile layer can be coloured by any per-cell field: Biome, Terrain, Region, Elevation, Latitude or Water Source.
- A selected-cell highlight pairs with Feature 6's Cell Inspector, which lists every field of that cell.
- Sites and dynasty starts are drawn, with spacing rings that turn red when a pair is too close.
- A Guarantee Evidence layer draws the proof paths the validator found and marks every violation.

It is debug-grade, not V-1's tile art: flat-coloured flat-top hexes on the world's odd-q grid, and simple shapes.

GDD decision 6 chose to include visual rendering despite S-1 being a "data pipeline testbed" because fast iteration requires seeing what you're iterating on. *Amended 2026-10-15:* the aim widened from "eyeball placements" to "understand the data", and rendering moved from a bespoke `S1DebugRenderer` to the engine's `IDebugDomain` overlay system (tool tiers: an in-game overlay is a Visual Debugger domain).

## Problem

Without debug rendering:
- **Blind iteration:** designers cannot see biome distribution, site placements or region boundaries without exporting JSON and reading it.
- **Invisible terrain fields:** `elevation`, `latitude` and `waterSource` feed A4 Weather but have no picture. A bad elevation field shows up only later, as wrong weather in S-2.5.
- **Opaque guarantees:** a world passes or fails with no visible reason. "Which Hearth was unreachable, and where did the path stop?" needs the validator's evidence on the map.
- **Features 1–3 opaque:** generation, validation and retry all work but produce invisible output (only logs and JSON files).

## Goals

1. **Tile layer with colour-by mode:** one filled flat-top hex per cell, drawn as a single **CellField** layer (dia-needs #14: GPU-retained hex geometry + a per-cell value texture), not as immediate primitives. The colour mode is one of Biome (default), Terrain, Region, Elevation, Latitude or Water Source. Categorical fields use a distinct palette; `elevation` and `latitude` use a sequential ramp over [0, 1]. Feature 6's panel shows a legend for the current mode.
2. **Grid lines and region edges:** optional hex outlines (CellField's inset, so the gaps are the lines and cost nothing), and region boundaries drawn along every shared edge whose two cells differ in region.
3. **Sites:** markers for Hearth Sites (circles), Nexus Nodes (triangles) and world-thrones (stars), each with a distinct colour and shape.
4. **Dynasty Starts:** a player marker, rival markers, and a minimum-spacing ring around each start. A ring is red when its start belongs to a pair in DynastySpacing's `violatingPairs`.
5. **Guarantee Evidence:** each `proofPath` from Feature 2 drawn as a polyline (one colour per guarantee), and each `violatingCells` cell outlined in red with a cross. Drawn only when the evidence belongs to the world on screen (see Evidence Freshness).
6. **Selection:** the selected cell is outlined, and its six neighbours are outlined and labelled with their direction index `0..5` (the order A4 depends on). Feature 6's Cell Inspector shows the fields.
7. **Fits the debug-draw budget:** a Small world (66 × 42) with every drawer on drops nothing (`DroppedCount() == 0`) and keeps the frame under 16 ms. Needs dia-needs #13 (budget 2,048 → 8,192) for the line/marker overlays and #14 (CellField) for the tiles.
8. **Independent toggles:** every drawer toggles through the domain's `toggle` command. Toggle state and colour mode survive regeneration and import.

## Non-Goals

1. **V-1 real tile art:** debug-grade only (solid-colour hexes, simple shapes). V-1 owns sprite tiles, the real camera and fog of war.
2. **Animation:** static rendering only.
3. **Lighting / shaders:** flat, unlit rendering.
4. **3D terrain:** elevation is a colour ramp on the 2D grid, not a heightmap mesh.
5. **Live ecology state:** world structure only. A1 runtime state belongs to S-2/S-3.
6. **Camera persistence:** pan/zoom resets on regeneration.
7. **Drawing a discarded world:** a failed generation's world is thrown away (Feature 5), so its evidence isn't drawn. Feature 6 lists failed attempts as text.
8. **Standard / Large performance:** S-1 measures Small only (parent spec, Map Sizes). Larger maps may exceed the budget; that's for whichever testbed first needs them.

## Architecture

### Domain and Ownership

`WorldGenDebugDomain` is created by `WorldGenerationModule` under `DIA_DEBUG` and registered with the visual debugger, following CoW's `GameSpeedDebugDomain` (`Cluiche/CoW/Modules/GameSpeedDebugDomain.h`).

- It lives on **SimPU** with the module, so drawers read `GetCurrentWorld()` and `GetLastReport()` directly with no cross-thread copy. This replaces the old "renderer on MainPU reads the world via event or shared state", which had no safe answer.
- `IVisualDebugger::SetEnabled` is atomic, so the MainPU panel can toggle SimPU drawers.
- The same domain provides Feature 6's panel via `GetJSONState` / `OnCommand`. One domain, one panel card, so the controls and the drawers stay in sync.
- Drawers re-read the world each frame. When `GetWorldRevision()` changes they rebuild any cached per-cell geometry and colours. Selection is kept if the coordinate is still in bounds, and cleared otherwise.

### Drawers

| Drawer | Default | What it draws | Primitives at Small (66 × 42) |
|--------|---------|---------------|-------------------------------|
| `Tiles` | On | CellField layer: one value per cell (category id or float, per colour mode) + palette/ramp | 1 CellField request (0 debug primitives) |
| `GridLines` | Off | CellField inset on the tile layer (gaps over a dark background) | 0 |
| `RegionEdges` | Off | Edges whose two cells differ in `region`, 2 px, in region colour | ≤ ~2,000 lines (typically far fewer) |
| `Sites` | On | Hearth circles, Nexus triangles, throne stars | < 100 |
| `DynastyStarts` | Off | Start markers + spacing rings (red when violating) | < 50 |
| `GuaranteeEvidence` | Off | Proof-path polylines; red outline + cross on violating cells | < 1,500 lines |
| `Selection` | On | Selected-cell outline, 6 neighbour outlines, 6 direction labels | ~20 + 6 text |
| **Total, all on** | | | **~3,600 geometry, ~6 text** (budget 8,192 / 256) |

Region-edge counts come from the flat-top odd-q rectangle (about `3 × W × H` edges in total, of which only boundaries are drawn). The budget headroom covers rounding in these estimates; AC10 measures the real numbers. *Amended 2026-10-15:* tiles and grid lines moved from ~19,500 immediate primitives to CellField.

### Colour Modes

| Mode | Field | Palette |
|------|-------|---------|
| Biome (default) | `cell.biome` | Categorical, HSV-spaced placeholder colours until Phase 2 biome names land |
| Terrain | `cell.terrain` | Categorical |
| Region | `cell.region` | Categorical (same colours as `RegionEdges`) |
| Elevation | `cell.elevation` | Sequential ramp, 0 = dark blue → 1 = white |
| Latitude | `cell.latitude` | Sequential ramp, 0 (equator) = warm → 1 (pole) = cold |
| Water Source | `cell.waterSource` | None = neutral grey; Ocean, Lake and River each distinct |

On a world or colour-mode change the domain writes the per-cell values into its CellField layer (category ids for categorical modes, floats for Elevation/Latitude) and picks the palette or ramp; nothing is uploaded on frames where neither changed. The colour mode is set by the `setColourMode` command (`args: { "mode": "<name>" }`). `GetJSONState` writes the active mode and its legend (category → colour, or ramp endpoints with min/max values seen in this world) for Feature 6 to show.

### Evidence Freshness

Feature 3's `GenerationReport` holds `finalValidation` (with Feature 2's evidence) and the `worldRevision` it produced. `GuaranteeEvidence` draws only when `report.worldRevision == GetWorldRevision()`. After a failed generation the previous world stays on screen, the evidence layer draws nothing, and the panel shows "Evidence hidden: last generation failed (see Validation)". An imported world has no report, so there's nothing to draw. A **Validate** command (Feature 6) runs Feature 2 on the current world, and its result is used here as fresh evidence.

### Hex Geometry

All cell geometry comes from Dia's `HexTopology`, not renderer-local maths. With `size` = centre-to-corner distance:
- A hex is `2 × size` wide and `√3 × size` tall, and columns are `1.5 × size` apart.
- `CellCenter(col, row) = (1.5 × size × col, √3 × size × (row + 0.5 × (col & 1)))` in y-down world space.
- The six corner vertices sit at on-screen angles 0°, 60°, ..., 300° from the centre.

World → screen goes through the camera. Screen → cell uses the camera inverse, then `CellFromPoint` (cube rounding), and returns "none" outside the grid. Corner positions are cached per world revision.

### Selection Input

A left-click on the map selects a cell:
1. The stage's input handler (MainPU) turns the click into a world point (camera inverse).
2. `HexTopology::CellFromPoint` turns the point into a cell.
3. The handler sends `selectCell { "x": col, "y": row }` to the domain through the same command path the panel uses.

The panel can also type a coordinate. Esc or clicking outside the grid clears the selection (`selectCell` with no args).

### Camera Controls

Pan (WASD / arrow keys, clamped to world bounds), zoom (mouse wheel), reset (Home key, frames the whole world). Camera state is position + zoom; it resets on regeneration.

## Public Interfaces

### APIs

```cpp
// WorldGenDebugDomain.h (CoW/Shared/WorldGeneration/) — DIA_DEBUG only
namespace CoW::WorldGeneration {

enum class ColourMode : uint8_t { Biome, Terrain, Region, Elevation, Latitude, WaterSource };

class WorldGenDebugDomain : public Dia::VisualDebugger::IDebugDomain {
public:
    explicit WorldGenDebugDomain(WorldGenerationModule& module);

    // Identity: id "WorldGen", display "World Generation", group "Systems"
    Dia::Core::StringCRC GetDomainId()     const override;
    const char*          GetDisplayName()  const override;
    const char*          GetDescription()  const override;
    Dia::Core::StringCRC GetGroup()        const override;
    Dia::Core::RGBA      GetAccentColour() const override;

    void Register(Dia::Debug::IDebugLayerRegistry& mgr) override;
    void Unregister(Dia::Debug::IDebugLayerRegistry& mgr) override;
    int  GetDrawerCount() const override;                       // 7
    Dia::Debug::IVisualDebugger* GetDrawer(int index) override;

    // Panel bridge: drawers, colour mode + legend, Feature 6's controls/inspector/validation
    void GetJSONState(Json::Value& out) override;
    // Commands: toggle, setScale (IDebugDomain); setColourMode, selectCell (this feature);
    //           regenerate, export, import, cancel, validate (Feature 6)
    void OnCommand(Dia::Core::StringCRC cmd, const Json::Value& args) override;

    ColourMode GetColourMode() const;
    bool       HasSelection() const;
    GridCoord  GetSelection() const;
};

} // namespace CoW::WorldGeneration
```

**No new events:** drawing is passive. Selection and colour mode are domain state, not bus traffic.

## Dependencies

### Upstream Dependencies

| System/Feature | Why needed | Status |
|----------------|-----------|---------|
| **Feature 1 (Placeholder Generation Algorithm)** | Produces `WorldStructure` to render | Approved |
| **Feature 2 (Reachability Validator)** | Evidence (`proofPaths`, `violatingCells`, `violatingPairs`) for `GuaranteeEvidence` and `DynastyStarts` | Approved (amended 2026-10-15) |
| **Feature 3 (Retry Mechanism)** | `GenerationReport` with `finalValidation` and `worldRevision` (Evidence Freshness) | Approved (amended 2026-10-15) |
| **Feature 4 (JSON Export/Import)** | Imported worlds also rendered | Approved |
| **Feature 6 (F-1 Debug Host Integration)** | Panel controls, Cell Inspector, legend display | Approved |
| **DiaDebugDomain** | `IDebugDomain`, `IVisualDebugger`, DiaDebugPanel | Done |
| **dia-needs #13** | Debug-draw geometry budget 2,048 → 8,192 | Approved 2026-10-15, not built — **blocks this feature** |
| **dia-needs #14 (CellField)** | Tile layer, colour modes, grid inset | Approved 2026-10-15, spec not written — **blocks this feature** |
| **Phase 2 data design** | Biome, Terrain, Region and ApotheosisType enums for palettes and markers | GDD in progress (placeholder colours/shapes) |

### Dia Engine Dependencies

- **DiaVisualDebugger** (`IDebugDomain`, `IVisualDebugger`, `IDebugLayerRegistry`)
- **DiaCore** (`IDebugDraw`: lines, circles, triangles, text for the overlays)
- **DiaGraphics / DiaBgfx** (CellField layer, dia-needs #14)
- **DiaPathfinding** (`HexTopology`: `CellCenter`, `CellFromPoint`, `NeighbourAt`)
- **DiaInput** (camera pan/zoom, click to select)

## Acceptance Criteria

All at the Small preset (66 × 42) unless stated.

### AC1 — Tile Layer
Generate a world with a known seed. **Pass:**
- 66 × 42 filled hexes appear, with odd columns offset half a cell down.
- In Biome mode, colours match biome types: a Forest-heavy `GenParams` gives a mostly-Forest-coloured map.

### AC2 — Colour Modes *(amended 2026-10-15)*
Switch through all six modes with `setColourMode`. **Pass:**
- Each mode recolours every hex from its field.
- Elevation and Latitude show a continuous ramp, and the panel legend reports the min/max seen.
- Water Source shows at least one non-None cell (Test 1c guarantees one).
- A test world with hand-set values (e.g. elevation 0, 0.5 and 1 in three cells) shows the three ramp colours at those cells.

### AC3 — Sites Toggle
Toggle `Sites` on. **Pass:** Hearth/Nexus/throne markers sit at their site cells. Toggle off. **Pass:** markers disappear, tiles remain.

### AC4 — Regions
Toggle `RegionEdges` on. **Pass:**
- An edge is drawn exactly where `NeighbourAt` crosses into a different region, and nowhere else (checked on a hand-built two-region world).
- Region colour mode and edge colours agree.

### AC5 — Dynasty Starts and Spacing
Toggle `DynastyStarts` on, on a passing world. **Pass:** markers + rings, none red. Then import a world (hand-edited JSON) with two starts too close, and run Validate. **Pass:** exactly that pair's rings are red.

### AC6 — Guarantee Evidence *(added 2026-10-15)*
Generate a passing world and toggle `GuaranteeEvidence` on. **Pass:** one polyline per `proofPath`, each running Hearth → Nexus or throne → Ancient Interiors through neighbouring cells.

Import a world with one Hearth on an island and run Validate. **Pass:** that Hearth's cell is outlined red with a cross, and it has no polyline.

Force a failed generation (Feature 5). **Pass:**
- The previous world stays on screen and its evidence is hidden.
- The panel says why the evidence is hidden.

### AC7 — Selection and Neighbours *(added 2026-10-15)*
Click a cell on an even column and one on an odd column. **Pass:**
- The clicked cell is outlined, and its in-bounds neighbours are outlined and labelled `0..5`, matching `HexTopology::NeighbourAt(cell, d)`.
- Feature 6's inspector shows the same cell.

Click a corner cell. **Pass:** only the in-bounds neighbours are drawn.

Click outside the grid or press Esc. **Pass:** selection clears.

### AC8 — Grid Lines
Toggle `GridLines` on. **Pass:** every hex is outlined and each shared edge is drawn once (no doubled lines in a draw-count check). Toggle off. **Pass:** outlines disappear.

### AC9 — Camera
- Pan with WASD or the arrow keys: the view moves and stops at the world bounds.
- Wheel: zooms in and out.
- Home: resets to frame the whole world.

### AC10 — Budget and Performance *(amended 2026-10-15)*
With every drawer on and Grid Lines on, on a Small world. **Pass:**
- The tile layer is one CellField request per frame, and its value texture is uploaded only on world or colour-mode change (upload counter unchanged across 300 idle frames).
- `DebugFrameData::DroppedCount() == 0` and `DroppedTextCount() == 0` for 300 consecutive frames.
- Frame time stays under 16 ms.
- The recorded primitive count is logged, to check the drawer table.

Needs dia-needs #13 and #14.

### AC11 — State Persistence
Turn Sites off, Regions on and Dynasty Starts on, set Elevation mode, and select a cell. Then regenerate, and then import. **Pass:**
- Toggles and colour mode are unchanged after each.
- The selection is kept if still in bounds.

### AC12 — Hex Picking
Click at cell centres on even and odd columns, just inside hex corners, and just outside the grid. **Pass:**
- A centre returns that cell.
- A point just inside a corner returns the cell that contains it, never an off-by-one neighbour.
- A point outside the grid returns "none".
- `CellFromPoint(CellCenter(c)) == c` for every cell.

## Inherited Binding Decisions

### From Platform (Cluiche)

| ID | Decision | How it applies |
|----|----------|----------------|
| PD-002 | ProcessingUnit/Phase/Module architecture | The domain lives on SimPU with `WorldGenerationModule`; only input (click, camera) and the panel are on MainPU, and they reach the domain through its commands |
| PD-004 | No STL containers in public APIs | The domain API uses enums, `GridCoord` and Json; cached geometry uses `DynamicArrayC` internally |

### From Application (CoW)

| ID | Decision | How it applies |
|----|----------|----------------|
| AD-001 | Dia edits need sign-off | dia-needs #13 (budget raise) and #14 (CellField) signed off 2026-10-15. No other Dia change |
| AD-002 | Classify cross-cutting needs | `WorldGenDebugDomain` is **CoW-Shared**, next to `WorldGenerationModule` (S-2.5 and S-3 load S-1 worlds and get the same view for free). Not Dia: it draws CoW's biomes, sites and guarantees. The Dia hex pathfinding debugger is square-grid only; making it hex-aware would be a separate Dia need, not done here |

## Open Design Questions

| # | Question | Impact | Resolution |
|---|----------|--------|------------|
| 1 | Exact biome/terrain/region debug colours? | Clarity only | HSV-spaced placeholders; swap for thematic colours when Phase 2 names land |
| 2 | How does a world click reach a SimPU domain command? | Selection (AC7) | Check at plan time how DiaDebugPanel commands are routed to SimPU domains, and send the click the same way. If no route exists for non-panel input, use the baseline render-to-sim request pattern (`RenderToSimNavRequest`) |
| 3 | Should a failed generation keep its last attempt's world, so violations can be drawn? | Debugging unsatisfiable seeds visually | Not for S-1: text diagnostics in Feature 6 plus "export what you have" is enough. Revisit if the S-1 challenge pass shows designers need it |
| 4 | ~~Is ~23,000 primitives too slow through the immediate debug path?~~ | AC10 | **Resolved 2026-10-15:** tiles and grid moved to CellField (dia-needs #14); the immediate path keeps ~3,600 overlay primitives |

## Status

`Approved` — 2026-10-11. Amended 2026-10-14 (hex world): hex tiles, hex picking, outline-based region edges. Amended 2026-10-15: rebuilt as `WorldGenDebugDomain` (`IDebugDomain`); colour-by modes for every per-cell field; Selection + neighbour labels; Guarantee Evidence with freshness rule; Design Reference moved into Feature 6's inspector; budget and performance measured at Small. Tiles and grid lines drawn through CellField (dia-needs #14); overlays through the debug-draw path with the budget raised to 8,192 (dia-needs #13). Blocked on both.

---

## Implementation Notes

Create `layered-debug-rendering.plan.md` when implementation starts. Its **Implementation Patterns** section must cover:

- The domain skeleton and registration (copy the `GameSpeedDebugDomain` / `GameSpeedControllerModule` pattern)
- The per-revision geometry cache (hex corners, shared-edge list, region-edge list)
- Palette and ramp tables per colour mode, and the legend JSON shape
- Evidence freshness check and the hidden-evidence message
- The click → `selectCell` route (Open Q #2)
- The measured primitive counts against the drawer table (AC10)

Build order: dia-needs #13 and #14 first, then this feature.
