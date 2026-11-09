# Plan: S-1 World Generation map view (system plan task 10)

**Spec:** @layered-debug-rendering.md (Feature 7, terrain part)
**System plan:** @worldgenerationpipeline.plan.md (task 10)
**Panel plan:** @worldgen-panel.plan.md (tasks 1-6 Done; this builds on its card and `view` JSON)
**Mockup:** @s1-editor-mockup.html (stand-in map; approved in task 8c). Not covered by it: Relief hillshade and ramps, rivers overlay, neighbour 0..5 labels, grid inset, camera, legend swatches. Verified live in task 12 (screenshots), no new mockup.
**Status:** In Progress

Scope: Tiles via the CellField renderer through the existing DiaCellLayers viewer (Relief default, Elevation, Water, terrain type, stage-result layers), Rivers overlay, always-on small grid inset, Selection + neighbours 0..5, camera, click selection. Biomes, regions, sites and validation overlays are later tasks (11, 12, 13-16).

## Decisions (2026-11-09)

- **Open Q #2 resolved by precedent.** There is no cross-PU click route to build. CluicheTest's `CellLayersViewerTestStageModule` reads input on SimPU (`InputStreamModule` via `ModuleRef`), builds a pointer frame and calls `CellLayerViewerDomain::SubmitPointerFrame(frame, GetPickerView())`; the viewer picks and selects itself. The S-1 stage module is already a SimModule with `InputStreamModule`. Typed or programmatic selection uses the sim-hosted `cells.select.cell/clear` commands.
- **Host the DiaCellLayers machinery in CoW.** No new Dia code, no CluicheTest edit. Pure CoW-Shared pieces (`WorldHexGrid`, `WorldLayerSource`, overlay geometry, `WorldCameraController`) plus a DIA_DEBUG `WorldGenViewHost` on SimPU.
- **One owner for selection and colour mode: the CellLayers viewer.** `WorldGenDebugDomain`'s `selectCell`, `clearCell`, `setColourMode`, `toggle` become thin forwarders through a small CoW interface `IWorldGenView`; its own selection, mode and drawer state is removed. The `view` and `cell` JSON are built from the viewer. Contract keys stay.
- **One card.** The viewer's relay adapter is NOT registered with `VisualDebuggerModule`; the World Generation card's View section drives the viewer, and `view.legend` is filled from the viewer's legend.
- **Grid lines: always-on small inset (about 0.05) with a dark backdrop, no runtime toggle** (user, 2026-11-09). The spec's Grid-lines drawer is dropped for now; a runtime inset override would be a Dia edit, deferred.
- **Zoom input: keyboard zoom (+/-, Q/E) and drag pan (middle/right), AND a mouse-wheel delta accessor on `InputStreamModule`** (user, 2026-11-09: "both"). The Baseline edit to `InputStreamModule` is signed off (AD-001, 2026-11-09), limited to adding a wheel-delta accessor.
- **Latency gate.** "Slider to visible under 100 ms" is gated in Release and logged in Debug. If it fails because stage re-run granularity must change, escalate to the user.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Record Q2 resolution in `layered-debug-rendering.md` (Open Q table, Selection Input section, status); amend system plan row 10 (tasks 4 and 6 Done; unblocked) | doc diff | Done | haiku | |
| 2 | `WorldHexGrid` (CoW/Shared/WorldGeneration): centres, 6-neighbour table, bounds, `CellGridDesc` with stable storage (the desc borrows pointers). Copy `BuildGrid` from CellLayersViewerTestStageModule.cpp:110-150 | RED first: for Small, Standard, Large: `CellFromPoint(CellCenter(c)) == c`; neighbours match `HexTopology::NeighbourAt`; edge cells -1; bounds contain every hex | Pending | sonnet | Add to CoW.vcxproj and GoogleTests.vcxproj |
| 3 | `WorldLayerSource`: layer descriptor table plus readers (float copy from `TerrainLayers`, uint widened to float, `Cell` field extractors), `version() = session.Revision()`, zero-fill when no world | Generate Small; every reader equals `TerrainLayers`/`Cell`; version bumps only on a changed `Tick`; Large read under 1 ms | Pending | sonnet | Decide landmass kind (0..255 ids): float+ramp or capped category |
| 4 | `Assets/CoW/Stages/S1WorldGenerationTestbedStage/worldgen.schema.json`: one entry per layer (label, description, type, min/max or enum+colour, `ui.ramp`, `ui.inset` about 0.05, `ui.backdrop`; relief has `elevation_layer` and `default_base`) | Loads: every descriptor has an entry; enum counts equal enum sizes; every ramp id exists in `ramps.json`; exactly one `default_base` | Pending | sonnet | Placeholder enum colours; add to the CoW assets catalogue |
| 5 | Overlay geometry (pure): `BuildRiverSegments(world)`, `BuildNeighbourOutline(grid, cell)` with 0..5 label anchors | RED first: segments equal `downstream` lines; corner cell yields only in-bounds neighbours; even and odd columns; count under the 8,192 budget at Large | Pending | sonnet | FakeDebugDraw pattern from DiaCellLayers Testing |
| 6 | `WorldCameraController` (pure): fit-to-grid, pan clamp, zoom clamp, reset, keyboard zoom, drag pan, wheel zoom input; adapter to `Camera2DModule` | Fit frames Small; pan stops at bounds; zoom clamped; Home refits; wheel delta zooms | Pending | sonnet | |
| 6b | `InputStreamModule`: mouse-wheel delta accessor (Baseline, AD-001 signed off for this addition only) | RED first: injected wheel event gives the delta for one frame, zero after | Pending | sonnet | Check the input source path (DiaInput) first; stop and report if wheel events are not available upstream |
| 7 | `WorldGenViewHost` (CoW, DIA_DEBUG, SimPU): register layers, overlays, detail provider ("Terrain" rows from `session.Cell`), `LayerUploader`, `CellLayerViewerDomain`, `ViewFn`, `RegisterCommands`, `SubmitPointerFrame` (copy `BuildPointerFrame` into CoW), layer-manager draw hook; rebuild the grid on size change (unregister, re-register, reapply selection if in bounds) | Fake-registry test: layers and overlays registered; size change keeps selection in range else clears; Small to Large leaves no dangling grid | Pending | sonnet | Mirror BuildViewer (cpp:281-424) and TeardownViewer |
| 8 | Reconcile `WorldGenDebugDomain` via `IWorldGenView`; forwarders; remove local selection, mode and drawer state; `view`/`cell` JSON from the viewer; keep keys | Update the 13+ domain tests; new: `setColourMode` changes the active base; `selectCell` selects on the viewer and `cell.selected` follows; viewer-side selection shows in `cell`; unknown mode warns; state under 12 KB at Large | Pending | sonnet | |
| 9 | Wire the S-1 stage module and manifests: build the host in `TryBuildPanel`; per-frame fit camera and pointer submit; ordered teardown (viewer before relay detach). `cow_main.diaapp`: `CellLayerRegistryService` stream. Stage diaapp: `CellLayersModule` on SimPU; stage module deps `Camera2DModule`, `SimQueryHostModule`, `CellLayersModule`; channels `KernelCellFieldRegistry`, `ValueSchemaService`, `CellLayerRegistryService`, `RenderToSim` | `dia validate manifest`; CoW Debug build; launch and log shows "viewer hosted (N layers)" | Pending | sonnet | CellHistoryModule not needed |
| 10 | CoW `debug-panel.html` World Generation card: `view.legend` swatches or ramp, layer picker grouped from `view.modes`, overlay checkboxes from `view.drawers` (no Grid-lines toggle), keep the coordinate box | Vitest for the new View rendering; manual panel check | Pending | sonnet | CoW copy only |
| 11 | Latency and budget E2E "s1_render": slider via `cow.worldgen.set`, poll viewer version and upload counter, record elapsed at Small, Standard, Large; idle frames upload 0 over 300 frames; `DroppedCount() == 0` | Slider to uploaded under 100 ms at Small (gate in Release, log in Debug) | Pending | sonnet | Per-preset screenshot through `CaptureManager` |
| 12 | Final verification: scoped `*WorldGen*`, `*CellLayers*`, `*CellField*`; one full run `--shards 4`; manifest; both builds; E2E suites; in-game walkthrough of Feature 7 AC1, 2, 7, 8, 9, 12; gap audit; observation scan | per list | Pending | sonnet | Mark system plan row 10 Done only after the in-game check by the user |

## Open items

- Latency gate may force finer stage granularity (escalate to the user).
- Relief tinting needs an eyeball check in the live window (task 12).
- `CellValueKind::Category` limits: confirm the largest enum (12 terrain types) and landmass ids up to 255 in task 3.
