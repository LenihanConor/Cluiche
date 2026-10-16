**Spec:** @docs/specs/applications/cow/systems/s1-worldgeneration/worldgenerationpipeline.md
**Status:** In Progress

System plan: each feature row gets its own `<feature>.plan.md` when it starts (created by `/implement`). Small map (66 × 42) only.

## Milestones

1. **Live terrain tool (tasks 1–10, incl. 8b, 8c).** Sliders and presets shape a rule-based world in near real time, seen in Relief and terrain layers. The visible, feel-it milestone.
2. **Biomes (task 11).** Biomes fall out of the terrain by rule (Biome Classification, a new feature).
3. **Playable world data (tasks 12–17).** Regions and sites, guarantees with evidence, retry, timeout/cancel, JSON save/load, and the rest of the panel and overlays.
4. **Proof (tasks 18–20).** System acceptance, review, observation pass, close-out.

**Parallel work:** Dia tasks (2, 4, 5, 6) and CoW tasks (1, 3, 7) touch different projects and can run side by side. Within CoW, tasks are sequential: they share `CoW.vcxproj`, `GoogleTests.vcxproj` and the `WorldStructure` types.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| **M1** | **Live terrain tool** | | | | |
| 1 | Spec fix-up: bus messages trivially copyable (fixed `char` paths/messages; `GenParams` arrays, no `std::map`, PD-004). Fill the existing `WorldGenerationComplete` stub (`CoW/Modules/GameSpeedMessages.h:40`, already subscribed by GameSpeedController), moved to `WorldGenerationMessages.h` with the same `kTypeId`. Record the per-frame state machine in Features 3 and 5. Amend Feature 3 for **SD-S1-008**: retries re-roll Regions and Sites only, terrain/biomes stay fixed, terrain-level failures report their cause | n/a (docs) | Pending | sonnet | Bus recorder asserts `is_trivially_copyable` (`DiaMessageBus/Bus.h:461`); a blocking `GenerateWorld()` on SimPU can't receive Cancel |
| 2 | dia-needs #15: seeded 2D noise in DiaMaths (small feature spec, then build) | Determinism (same seed + coords → identical bits), range/distribution tests; full `dia run googletest` | Pending | sonnet | Blocks task 7. User approves the small spec first |
| 3 | Re-spec Feature 1 as the **Terrain Generator** (rename from Placeholder Generation Algorithm), per the physical rules in GDD `02_WorldLore/GeographyAndRegions.md`: continent, coast, islands, peninsulas, mountain spine, second barrier, rivers that carve valleys, lakes, depth zones. Inputs: `WorldShapeParams` sliders + presets (Continental, Archipelago, Highlands). Staged pipeline with cached stage results; **sliders are thresholds/weights over seed-fixed fields** (more lakes adds lakes, nothing reshuffles); live preview runs terrain only. Rules fixed in code, documented step by step, constants in a tuning file; every stage result viewable as a layer. Decide where Regions & Sites live (task 12). Log the deviation from S-1 GDD decision 1 ("placeholder is naive"). **Also specify:** presets (SD-S1-011: versioned JSON format, `Cluiche/Assets/CoW/` location, deploy path, `preset.load` / `preset.save` / `presets`); locked imports + provenance (SD-S1-009); terrain types derived from elevation + slope for the navigation table; rivers through cells (matches A4's per-cell `waterSource`); a latitude-span slider (e.g. 20°–60°) instead of pole-to-pole; ocean border on every map; live preview byte-identical to Regenerate for the same seed + params; per-stage timings shown in the panel; **shape-check ACs** (land fraction tracks its slider; every river reaches the sea; lake count never drops as the lakes slider rises; mountain spine connected; depth zones ordered) | n/a (spec; user approval gate) | Pending | sonnet | Decided 2026-10-15: look and feel like a world; near-real-time shaping; transparent, not tweakable rules. Opus fits this design: ask first |
| 4 | dia-needs #13: `DebugFrameData::kGeometryCapacity` 2,048 → 8,192 | New googletest: 8,192 accepted, `DroppedCount() == 0`; full `dia run googletest` | Pending | sonnet | Check where `FrameData` lives (heap/stack) and copies in flight first |
| 5 | CellField feature spec (`/spec-feature`): `docs/specs/applications/dia/systems/diabgfx/cell-field-renderer.md`, parent `render-backend.md` | n/a (spec; user approval gate) | Pending | sonnet | dia-needs #14. Interview covers S-1 now (flat, Relief/hillshade, grid inset, live updates every slider move) and designs in, but defers building, weather needs (smooth float shading, layer stacking, weekly blend, partial updates, wind glyphs). Opus fits: ask first |
| 6 | Build CellField (its own plan) | Per its spec; render check | Blocked | sonnet | Blocked on task 5 approval |
| 7 | Build Feature 1 — Terrain Generator | Feature 1 ACs (re-spec) incl. the shape checks over many seeds and every preset; slider-stability tests (raising lakes/rivers only adds); live-preview world == Regenerate world (byte-identical); terrain stages < ~20 ms at Small; system Test 1c | Blocked | sonnet | Blocked on tasks 2 and 3 |
| 8 | `WorldGenerationModule` (SimPU) with **live preview** (re-run from the changed stage, same frame) and the per-frame generation state machine; accessors + revisions; metrics + health; `cow_main.diaapp`. Scaffold the S-1 stage with `/new-cow-stage` | Module integration tests in a real `Application`; `dia validate manifest`; `dia launch cow` boots into S-1 | Pending | sonnet | Also the `new-cow-stage` skill trial run (BACKLOG row) |
| 8b | **WorldGen command model + DiaAPI** (SD-S1-002): spec the `cow.worldgen.*` command list in Feature 6 first, then a typed command model in CoW-Shared; register JSON commands for every action (`set`, `preset.load`, `preset.save`, `regenerate`, `cancel`, `export`, `import`, `validate`, `select`, `view`) and query (`params`, `status`, `cell`, `report`, `presets`); thread-safe request queue drained in `DoUpdate` with last-value-wins per frame; per-revision snapshot for queries; register/unregister with the module lifecycle (SaveLoadModule pattern) | Tests: each command via `ExecuteCommandJson` from a non-SimPU thread; queued action applies next frame; query matches the module; unknown args return `success:false`; a test asserting every panel action maps to a registered command | Pending | sonnet | Before task 9: the panel is built on these commands |
| 8c | **HTML mockup** of the S-1 editor panel and view (sliders, presets, View section, Cell Inspector, Validation, stage timings, locked-import state) as the visual gate, per the editor-mockup rule; user signs off before task 9. At the same time, check `Cluiche/Assets/CoW/Global/Presentation/UI/debug-panel.html` supports the needed widgets (sliders that update while dragging, dropdowns, text/file inputs). New widgets are a CoW edit; copying them back to CluicheTest's panel is a CluicheTest edit and needs sign-off (AD-001) | User sign-off on the mockup; widget gap list | Pending | sonnet | Mockup next to the spec, e.g. `s1-editor-mockup.html` |
| 9 | Feature 6, terrain part: `WorldGenDebugDomain` + panel on top of the 8b commands (seed, `WorldShapeParams` sliders, presets load/save, View section, Cell Inspector incl. stage values, map size) | Feature 6 ACs for these sections; slider drag → preview update | Pending | sonnet | Needs 8c sign-off. Build to the mockup. Panel `OnCommand` calls the DiaAPI commands, nothing else (SD-S1-002) |
| 10 | Feature 7, terrain part: Tiles via CellField with Relief (default), Elevation, Water, stage-result layers; Rivers overlay; grid inset; Selection + neighbours; camera; click → `selectCell` | Feature 7 ACs for these drawers; live slider → visible change in < 100 ms | Blocked | sonnet | Needs tasks 4, 6, 9. Resolve Feature 7 Open Q #2 (click → SimPU route) first. **M1 done here** |
| **M2** | **Biomes** | | | | |
| 11 | Spec + build **Biome Classification** (new feature): temperature from latitude + elevation, moisture from distance to water + rain shadow; map to the GDD's 8 biomes + Ocean/Lake as shared `StringCRC` ids; zone rules; forbidden-neighbour fix-up. Biome, Temperature and Moisture layers; live preview | Spec ACs; zone/adjacency rule tests on many seeds | Pending | sonnet | First-guess biomes that A3/A4 evolve later. A4 needs a biome per cell before it can run |
| **M3** | **Playable world data** | | | | |
| 12 | Spec + build Feature 9 — Regions (10–16, contiguous, grown within depth zones, primary biome) + site placement (Hearths, Nexus, thrones, dynasty starts on the coast per the GDD). Sites take their own sub-seed so retries re-roll only this stage (SD-S1-008) | Spec ACs (home decided in task 3) | Pending | sonnet | Needs task 11 (regions carry a primary biome) |
| 13 | Feature 2 — Reachability Validator with evidence | Feature 2 AC1–AC10 | Pending | sonnet | @reachability-validator.md |
| 14 | Feature 3 — Retry + `GenerationReport` | Feature 3 AC1–AC7; system Tests 1a, 1b, 2a, 5a | Pending | sonnet | @retry-mechanism.md. Retry runs on Regenerate, never during live preview |
| 15 | Feature 5 — Timeout and Failure Handling | Feature 5 ACs; system Tests 4, 5 | Pending | sonnet | @timeout-failure-handling.md |
| 16 | Feature 4 — JSON Export/Import, schema v1 (terrain, biomes, regions, sites) **+ provenance** (seed, `WorldShapeParams`, preset, generator version) and locked imports (SD-S1-009) | Feature 4 ACs; system Test 3; regenerate-from-provenance reproduces the un-edited world; slider on a locked import asks before discarding | Pending | sonnet | @json-export-import.md. Schema written once the cell fields are final |
| 16b | **World hand-off** (SD-S1-010): a stage names its world in its `.diastage` config (`.wld.json` asset or preset + seed); `WorldGenerationModule` imports or generates it at stage start. Document it as the contract for S-2.5, S-3 and A4 | Test stage config with each form loads the expected world (revision > 0, matches the file / regenerated world); bad reference fails with a clear log + health error | Pending | sonnet | Then update A4's spec and the S-2.5/S-3 backlog rows to point at it |
| 17 | Features 6/7, the rest: Validation section, Validate button, export/import/cancel/progress; RegionEdges, Sites, DynastyStarts, GuaranteeEvidence | Remaining Feature 6/7 ACs; system Test 2b | Pending | sonnet | |
| **M4** | **Proof** | | | | |
| 18 | System acceptance pass at Small + `/review`. Save a screenshot per preset (Relief view) through DiaObservation's `CaptureManager` / DiaBgfx frame capture (Done) for visual review | Fresh `dia run googletest --filter=*WorldGen*` + in-game walkthrough + preset screenshots reviewed by the user | Pending | sonnet | |
| 19 | Observation pass over all touched files | Review of touched files | Pending | sonnet | Suggestions only |
| 20 | Close-out: `dia docs spec-done`, BACKLOG row, dia-needs #13/#14/#15 → Done; confirm the GDD fold-back (BACKLOG loose end) is done | `dia docs precommit` | Pending | haiku | |

## Open choices

- ~~**Undo/redo**~~ — decided 2026-10-15: out (system spec Open Q #5).

## Implementation Patterns

These are drafts from the specs and existing CoW code; check them against the real code before marking tasks Done.

**Code layout.**
- Pure logic goes in `Cluiche/CoW/Shared/WorldGeneration/`: types, `PlaceholderGenerator`, `ReachabilityValidator`, the attempt loop, `WorldJson`.
- The module, debug domain and messages go in `Cluiche/CoW/Modules/`. This is the `GameSpeedController` split, and the specs' header paths are indicative.
- GoogleTests compiles CoW `.cpp` files directly (`GoogleTests.vcxproj:885` pattern), so the code doesn't need to be header-only. Add files with `dia docs vcxproj-add`.

**Live shaping (task 3 specifies this).**
- The terrain pipeline is a list of stages, each caching its output fields (shape → elevation → flow → rivers/valleys → lakes → depth zones; biomes later).
- Each slider declares the first stage it affects. On change, re-run from that stage on, in the same SimPU frame, and publish a preview revision.
- Stability: the seed fixes every random field once. Sliders only threshold or weight those fields, so raising "lakes" fills more of the already-ranked basins, and raising "rivers" lowers the flow threshold so tributaries appear.
- Live preview never places sites, validates or retries. Those run on Regenerate, or once sliders are idle.

**Generation state machine (task 1 records this in the specs).**
- `WorldGenerationModule` runs **one attempt per SimPU frame**: `Idle → Attempting(n) → Idle`.
- Each `DoUpdate`:
  1. drain requests;
  2. if cancelled, finish as MANUAL_CANCEL;
  3. if past the timeout, finish as TIMEOUT;
  4. otherwise run attempt `n` (generate + validate). Pass → accept the world. Fail → append an `AttemptRecord`, publish `WorldGenerationRetry`, and either continue or finish as UNSATISFIABLE.
- Why: a single blocking call would stall SimPU for up to 5 s, and the Cancel message couldn't be handled until it returned. At Small an attempt should take tens of milliseconds, so this costs little.
- The blocking `GenerateWorld(seed, params)` stays for tests and headless use. It just loops the same step.

**Attempt loop is pure.** `GenerationRun` holds the seed, params, attempt index, report, injected clock and cancel flag, and `Step()` advances one attempt. Tests drive it directly: a fake clock for TIMEOUT, a set flag for cancel, and a failing guarantee for UNSATISFIABLE.

**RNG.** Each attempt reseeds from `DeriveSubSeed(outer, n)`. F-2 enforces registered streams in debug builds, so use the `WLD` stream from `RandomStreamModule` (reseeded per attempt), not a private RNG. Check the `RandomStreamManager` reseed API in task 2.

**Messages.**
- All trivially copyable; `kTypeId` is `StringCRC("CoW.<Name>")`.
- Requests: `WorldRegenerationRequested`, `WorldExportRequested`, `WorldImportRequested`, `WorldGenerationCancelled`.
- Notifications: `WorldGenerationComplete`, `WorldGenerationFailed`, `WorldGenerationRetry`, `WorldExportComplete`, `WorldImportComplete`.
- The world and report are read through the module accessors, never through the bus.

**Module conventions** (copy `GameSpeedControllerModule`):
- `DoStart` subscriptions with a handle check that increments health errors on failure.
- A health reporter, plus `cow.worldgen.*` metrics: attempts, generations, failures by reason, last duration.
- Under `DIA_DEBUG`, create the debug domain in `DoStart` and register it lazily.

**DiaAPI commands (SD-S1-002).** Copy `SaveLoadModule::RegisterCommands` (`CoW/Modules/SaveLoadModule.cpp:172`) but use `RegisterCommandJson`. Callbacks run on the caller's thread, so they never touch module state: actions push to a mutex-guarded queue, and queries read the latest snapshot. Unregister in `DoStop`.

**Debug domain.** Follow the `GameSpeedDebugDomain` shape. The domain lives on SimPU with the module. Panel commands turn into the same request messages, and the domain never edits module state directly.

**JSON.** jsoncpp, schema v1 per Feature 4; cells as a flat array in index order `y * width + x`.

## Progress notes

(Added as tasks complete.)
