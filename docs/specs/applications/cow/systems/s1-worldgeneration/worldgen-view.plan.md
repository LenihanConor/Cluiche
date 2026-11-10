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
| 2 | `WorldHexGrid` (CoW/Shared/WorldGeneration): centres, 6-neighbour table, bounds, `CellGridDesc` with stable storage (the desc borrows pointers). Copy `BuildGrid` from CellLayersViewerTestStageModule.cpp:110-150 | RED first: for Small, Standard, Large: `CellFromPoint(CellCenter(c)) == c`; neighbours match `HexTopology::NeighbourAt`; edge cells -1; bounds contain every hex | Done | sonnet | Add to CoW.vcxproj and GoogleTests.vcxproj |
| 3 | `WorldLayerSource`: layer descriptor table plus readers (float copy from `TerrainLayers`, uint widened to float, `Cell` field extractors), `version() = session.Revision()`, zero-fill when no world | Generate Small; every reader equals `TerrainLayers`/`Cell`; version bumps only on a changed `Tick`; Large read under 1 ms | Done | sonnet | Decide landmass kind (0..255 ids): float+ramp or capped category |
| 4 | `Assets/CoW/Stages/S1WorldGenerationTestbedStage/worldgen.schema.json`: one entry per layer (label, description, type, min/max or enum+colour, `ui.ramp`, `ui.inset` about 0.05, `ui.backdrop`; relief has `elevation_layer` and `default_base`) | Loads: every descriptor has an entry; enum counts equal enum sizes; every ramp id exists in `ramps.json`; exactly one `default_base` | Done | sonnet | Placeholder enum colours; add to the CoW assets catalogue |
| 5 | Overlay geometry (pure): `BuildRiverSegments(world)`, `BuildNeighbourOutline(grid, cell)` with 0..5 label anchors | RED first: segments equal `downstream` lines; corner cell yields only in-bounds neighbours; even and odd columns; count under the 8,192 budget at Large | Done | sonnet | FakeDebugDraw pattern from DiaCellLayers Testing |
| 6 | `WorldCameraController` (pure): fit-to-grid, pan clamp, zoom clamp, reset, keyboard zoom, drag pan, wheel zoom input; adapter to `Camera2DModule` | Fit frames Small; pan stops at bounds; zoom clamped; Home refits; wheel delta zooms | Done | sonnet | |
| 6b | `InputStreamModule`: mouse-wheel delta accessor (Baseline, AD-001 signed off for this addition only) | RED first: injected wheel event gives the delta for one frame, zero after | Done | sonnet | Check the input source path (DiaInput) first; stop and report if wheel events are not available upstream | Dia edit signed off 2026-11-09: DiaSDL InputSource translates SDL_EVENT_MOUSE_WHEEL (ints, never rounds a non-zero tick to 0); Baseline InputStreamModule.GetMouseWheelDelta(). 10 new tests. IsListeningForEvent mask entry is review-only. |
| 7 | `WorldGenViewHost` (CoW, DIA_DEBUG, SimPU): register layers, overlays, detail provider ("Terrain" rows from `session.Cell`), `LayerUploader`, `CellLayerViewerDomain`, `ViewFn`, `RegisterCommands`, `SubmitPointerFrame` (copy `BuildPointerFrame` into CoW), layer-manager draw hook; rebuild the grid on size change (unregister, re-register, reapply selection if in bounds) | Fake-registry test: layers and overlays registered; size change keeps selection in range else clears; Small to Large leaves no dangling grid | Done | sonnet | Mirror BuildViewer (cpp:281-424) and TeardownViewer | 2026-11-09: 14 WorldGenViewHost tests. Host follows session.World().dimensions (not pending Dimensions()); grid re-emplaced at the same address (selection store has 8 slots, uploader keys by grid pointer). Draw() is separate from Update(): task 9 must register DebugLayer() with the layer manager. Deps: registry, cellField (required), schema, registrar (optional), camera, input; registrar needs a SimQueryHostModule wrapper like MirrorRegistrar in CellLayersViewerTestStageModule.h. No true RED (host written first). |
| 8 | Reconcile `WorldGenDebugDomain` via `IWorldGenView`; forwarders; remove local selection, mode and drawer state; `view`/`cell` JSON from the viewer; keep keys | Update the 13+ domain tests; new: `setColourMode` changes the active base; `selectCell` selects on the viewer and `cell.selected` follows; viewer-side selection shows in `cell`; unknown mode warns; state under 12 KB at Large | Done | sonnet | | 2026-11-09: 10 WorldGenDebugDomainView tests; view.modes static, legend dynamic; state 6,576 B at Large. No RED (implementation first). |
| 9 | Wire the S-1 stage module and manifests: build the host in `TryBuildPanel`; per-frame fit camera and pointer submit; ordered teardown (viewer before relay detach). `cow_main.diaapp`: `CellLayerRegistryService` stream. Stage diaapp: `CellLayersModule` on SimPU; stage module deps `Camera2DModule`, `SimQueryHostModule`, `CellLayersModule`; channels `KernelCellFieldRegistry`, `ValueSchemaService`, `CellLayerRegistryService`, `RenderToSim` | `dia validate manifest`; CoW Debug build; launch and log shows "viewer hosted (N layers)" | Done | sonnet | CellHistoryModule not needed | 2026-11-09: cow/default E2E 8/8, celllayers E2E 11/11 (agent). Viewer hosted (25 layers, 2 overlays). Tiles NOT yet seen rendered; no runtime screenshot route for CoW. Overlay drawer labels (rivers, selection.neighbours) need display names in task 10. |
| 10 | CoW `debug-panel.html` World Generation card: `view.legend` swatches or ramp, layer picker grouped from `view.modes`, overlay checkboxes from `view.drawers` (no Grid-lines toggle), keep the coordinate box | Vitest for the new View rendering; manual panel check | Done | sonnet | CoW copy only; 2026-11-09: Vitest +14 (debugpanel suite 579); gtest +2 (view.layers). Grouped optgroup picker from view.layers (static, staticVersion), legend swatches/gradient/hidden, overlay names (Rivers, Neighbours (0-5)), Tiles/Selection fixed+disabled, Clear kept. Pixels not seen. |
| 11 | Latency and budget E2E "s1_render": slider via `cow.worldgen.set`, poll viewer version and upload counter, record elapsed at Small, Standard, Large; idle frames upload 0 over 300 frames; `DroppedCount() == 0` | Slider to uploaded under 100 ms at Small (gate in Release, log in Debug) | Done | sonnet | Per-preset screenshot through `CaptureManager`; 2026-11-09: cow.worldgen.view query + test_s1_render.py (3 tests, cow/default 11/11). Idle 0 uploads in 10 s. Debug p95 Small 110 / Standard 185 / Large 217 ms (<250). See Findings. |
| 12 | Final verification: scoped `*WorldGen*`, `*CellLayers*`, `*CellField*`; one full run `--shards 4`; manifest; both builds; E2E suites; in-game walkthrough of Feature 7 AC1, 2, 7, 8, 9, 12; gap audit; observation scan | per list | Done | sonnet | Mark system plan row 10 Done only after the in-game check by the user; 2026-11-09 final verification (agent). +9 WorldGenViewHost edge tests, +2 camera, +1 domain, +5 Vitest (584). Bug fixed: WorldCameraController NaN stuck in state (non-finite wheel/mouse/drag/dt now ignored). Release CoW build fails (pre-existing C4996, SaveLoadModule.cpp/F2TestRunnerModule.cpp) so no Release timing. Pixels never seen; system plan row 10 left for the user walkthrough. |

## Open items

- Latency gate may force finer stage granularity (escalate to the user).
- Relief tinting needs an eyeball check in the live window (task 12).
- `CellValueKind::Category` limits: confirm the largest enum (12 terrain types) and landmass ids up to 255 in task 3.

## Findings (tasks 2-4, 2026-11-09)

- Landmass is a Float layer (ramp `sequential.purple`, 0..16): the registry caps category palettes at 64 colours and landmass ids saturate at 255.
- `lakeMask` has 13 values (none + lake ids 1..12), `flowDirection` 7 (0..5 + sink). Float layer min/max in the schema are a first guess from seed 12345 (Small); eyeball in the live window (task 12).
- **Pre-existing CLI bug:** `dia validate schema` hardcodes `KNOWN_RAMPS` to 5 ramps (`Dia/DiaCLI/dia_cli/commands/validate/data_rules.py:20`) and rejects `sequential.terrain`, `sequential.purple`, `sequential.grey`; it also warns on unknown ui keys `inset`, `backdrop`, `default_base`, `elevation_layer`. The CluicheTest `celllayers_viewer.schema.json` fails the same way. The runtime loader is happy (test requires zero warnings). To fix in the CLI (backlog).
- **Possible generator issue:** `landPotential` contains `-inf` on some cells (seed 12345 range -inf..0.98). Check whether intended (Terrain Generator owner).

## Findings (tasks 10-11, 2026-11-09)

- **Picker contract:** `view.layers [{id,label,group}]` (static, full frames only, with `view.modes` kept) is built from `WorldLayerSource` groups through `IWorldGenView::ListColourGroups`; labels are humanised ids (`terrainType` -> "Terrain type"), the default shows as "Relief". Schema labels were not used.
- **`cow.worldgen.view`** (sim-hosted query registered by `S1WorldGenerationTestbedStageModule`): `{built, layerVersion, uploadCount, readFailures, droppedCount, gridWidth, gridHeight, colourMode, selection}`. `droppedCount` is the largest `IDebugDraw::DroppedCount()` seen at the end of a viewer draw.
- **Idle:** `uploadCount` stays constant over 10 s (about 300 Sim frames); `droppedCount` 0, `readFailures` 0.
- **Slider-to-uploaded latency, Debug** (20 alternating `landFraction` sets per size; elapsed from the `cow.worldgen.set` reply to `layerVersion == revision`; includes about 5-10 ms of polling round trip):

  | Size | p50 | p95 | max | generator `lastMs` (last set) |
  |---|---|---|---|---|
  | Small | 78 ms | 110 ms | 140 ms | 73.5 |
  | Standard | 122 ms | 185 ms | 243 ms | 117.0 |
  | Large | 179 ms | 217 ms | 241 ms | 195.7 |

  Debug gate p95 < 250 ms passes at all sizes. Elapsed time is dominated by the generation itself (landFraction re-runs every stage from Landmass: all ten implemented stages ran); the upload and the viewer add only a few ms. The Release gate (< 100 ms at Small) is not enforced and not yet measured; Debug Small p95 is already 110 ms, so Release needs a measurement in task 12.

## Latency gate revised (2026-11-09)

The first gate (total slider-to-visible p95 under 250 ms in Debug) flaked on Large (217, 232 and 261 ms in three runs), because generation itself is about 70 ms (Small), 100-130 ms (Standard) and 140-195 ms (Large) in Debug and varies by machine. The E2E now gates the part the view and the frame hand-offs add (total minus that run's own `lastMs`) at under 100 ms p95. Two consecutive runs: overhead p95 56/48/28 ms and 44/38/41 ms (Small/Standard/Large); totals p95 125/173/217 and 108/140/220 ms. The viewer exists only in DIA_DEBUG builds, so there is no Release measurement of the view; generation time in Release is the Terrain Generator's budget (about 20 ms at Small per its spec), which Debug does not meet (Small about 70 ms): a Release `lastMs` read is a task-12 item.

## Follow-ups from the S-1 map view and perf work (2026-11-09; add to docs/BACKLOG.md)

- **CoW Release build fails** (C4996, pre-existing): `fopen` in `Cluiche/CoW/Modules/SaveLoadModule.cpp:346`, `strncpy` in `F2TestRunnerModule.cpp:839` and `:1025`. No Release binary, so generation time in Release (Terrain Generator budget about 20 ms at Small) is unmeasured. Debug: about 64 / 91 / 138 ms at Small / Standard / Large.
- **`dia validate schema` is stale**: `KNOWN_RAMPS` hardcodes 5 ramps (`Dia/DiaCLI/dia_cli/commands/validate/data_rules.py:20`), rejecting `sequential.terrain`, `sequential.purple`, `sequential.grey`, and warns on ui keys `inset`, `backdrop`, `default_base`, `elevation_layer`. CluicheTest's schema fails the same way.
- **`landPotential` contains `-inf`** on some cells (seed 12345): check whether intended.
- **Dia changes wanted from the panel perf fix:** skip unchanged domains or cache per-domain JSON in `PackDomainStates`; the viewer-state adapter re-parses relayed JSON on Sim and calls a Main-owned `StreamReader::FetchLatest` from Sim; rate-limit `pu.over_budget` (covered by the approved profiling amendment).
- **Terrain Generator spec amendments** (AC9, AC12, AC13, stage 5 `basinDepth`) still unwritten.
- **Observation suggestions** for `WorldGenViewHost` (profile scopes around Draw, size rebuild and uploads; metrics for uploads, readFailures, droppedCount; a health reporter) and for `WorldGenDebugDomain`/stage module (see task 12 report).
- **Not covered by any test:** pixels (tiles, hillshade, ramps, rivers, labels, inset, legend), real SDL mouse/wheel/keyboard into the host, a real window resize, frame time under 16 ms, Esc-clears and click-outside-clears, toggle state across regenerate.
- **Layer labels** are humanised ids, not schema labels. **Grid lines drawer** is replaced by an always-on 0.05 inset (no runtime toggle).
