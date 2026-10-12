# Cluiche Backlog

Derived from spec status across `docs/specs/`. When a spec moves to Done, move it to [BACKLOG-HISTORY.md](BACKLOG-HISTORY.md).

One backlog for every project. The **Project** column says whose work it is: `Dia` (engine, including DiaCLI and editors), `CluicheTest`, `CoW`, or `Baseline` (CluicheGameBaseline). Filter with `/backlog <project>`. CoW design intent lives in the GDD (`C:\GitHub\DesignDocs\COW`); CoW specs are under [specs/applications/cow/](specs/applications/cow/cow.md). An engine capability a game needs is a `Dia` row with "CoW needs" in the notes, not a separate list.

---

## Ready to Build (Approved → implement)

These specs are `Approved` with all features `Approved`. No spec work needed — go straight to implementation.

### Systems

| Project | System | Features | Depends On |
|---------|--------|----------|------------|
| Dia | DiaAttribute: band-based-initialization | Feature 7: Schema-defined named band ranges (NONE/LOW/BASELINE/MEDIUM/HIGH/APEX or custom), `InitializeFromBands()` one-time resolution to base values, `band_schema_version` support. Spec Draft at @docs/specs/applications/dia/systems/diaattribute/band-based-initialization.md. CoW needs it for dragon stat authoring (F-2 band resolver tests). ~11 hours (1.4 days) | DiaAttribute Core (Feature 1, Done) |
| Dia | DiaMessageBus: bus-traffic-recorder | Debug-only `IBusRecorder` hook in `Bus::DispatchOne` + `FileBusRecorder` (versioned binary per-message records). Spec Approved and plan written (4 tasks, not started). CoW needs it for F-1 Test 6 and V-3; the CoW wiring (path, tick source, lifecycle) is a separate CoW task | None (DiaMessageBus is built) |
| Dia | F-1 Debug Infrastructure | 4 specs Approved (2026-10-11): (1) ModuleDebugRegistry (foundation), (2) DiaDebugConsole (in-game command console), (3) DiaDebugDataPanel (panel registry + BusTrafficViewer), (4) DiaDebugPalette (searchable discovery UI). Full APIs, mockups, 12 ACs each. 12-18 days total. Build order: Registry → Console+Panel → Palette | None (all dependencies in Dia) |
| CoW | WorldClock | System spec Approved + 4 features Approved: (1) WorldClockModule Core (season counter, time band resolution, Hz-based speed), (2) Modal Pause Nesting, (3) Save/Load Integration, (4) Speed Control UI. GDD `A2_WorldClock/` Locked 2026-10-09 | DiaSimTime (Done) |
| CoW | E1 Save/Load | System spec Approved + 5 features Approved (2026-10-11): (1) SaveLoadModule Core, (2) WorldClock Serialization, (3) RandomStream Serialization, (4) Save/Load Bus Messages, (5) TickCompleted Synchronization. CoW's integration with DiaSaveGame. GDD E1_SaveContinue Locked 2026-10-09 | DiaSaveGame (Done), WorldClock (Approved) |
| CoW | F-2 Core Runtime | System spec Approved (2026-10-11). Testbed stage with 6 tests: (1) Stream Independence, (2) Determinism Round Trip, (3) Save/Load RNG Round Trip, (4) Clock Proof, (5) Save/Load Proof, (6) Band Resolver Timing. F2CoreRuntimeStage + F2TestRunnerModule + DiaAutomation. GDD `F2_CoreRuntime.md` Locked 2026-10-09 | WorldClock (Approved), E1 Save/Load (Approved), DiaAttribute Feature 7 (Draft — Test 6 only), RandomStreamManager (Done) |
| CoW | S-1 World Generation | System spec Approved + 7 features Approved (2026-10-11): (1) Placeholder Generation Algorithm, (2) Reachability Validator, (3) Retry Mechanism, (4) JSON Export/Import, (5) Timeout and Failure Handling, (6) F-1 Debug Host Integration, (7) Layered Debug Rendering. WLD data pipeline (seeded generation, export/import, timeout/failure, reachability validator, 3 hard guarantees). **New scope:** geographic region identification (valleys, mountain ranges, coastal zones, highlands, lowlands) algorithmically identified from elevation + water proximity; each cell tagged with `geographic_region_id`. Used for D4 event narrative, C1 territory, rival spacing. A4 uses per-cell weather (not regions). GDD `S1_WorldGeneration.md` Locked 2026-10-11. **Blocker:** Feature 2 requires B2 terrain traversability data externalization (system spec Open Q #2) | F-2 (Approved), F-1 Harness (Approved for Feature 6), B2 terrain data externalization (blocking Feature 2) |

---

## Spec Work Needed (Draft or unset — review/approve before building)

### Other Spec Work

| Project | Item | Spec | What's needed |
|---------|------|------|---------------|
| ~~CoW / Dia~~ | F-1: Bus traffic recorder | — | Feature spec: versioned per-message records (tick, type id, payload), Debug-only. **Placement undecided:** new Dia capability in DiaMessageBus, or a CoW-Shared subscriber that avoids a Dia change. Depends on CoW app skeleton |
| ~~CoW~~ | ~~F-1: Debug command / inspector host~~ | — | ✅ **Approved 2026-10-11:** Moved to Ready to Build |
| ~~CoW~~ | ~~WorldClock~~ | @docs/specs/applications/cow/systems/worldclock/worldclock.md | ✅ **Approved 2026-10-11:** System + 4 features approved, moved to Ready to Build |
| ~~CoW~~ | ~~E1 Save/Load~~ | @docs/specs/applications/cow/systems/e1-saveload/e1-saveload.md | ✅ **Approved 2026-10-11:** System + 5 features approved, moved to Ready to Build |
| ~~CoW~~ | ~~F-2 Core Runtime~~ | @docs/specs/applications/cow/systems/f2-core-runtime/f2-core-runtime.md | ✅ **Approved 2026-10-11:** System spec complete, moved to Ready to Build |
| ~~CoW~~ | ~~S-1 World Generation~~ | @docs/specs/applications/cow/systems/s1-worldgeneration/worldgenerationpipeline.md | ✅ **Approved 2026-10-11:** System + 7 features approved, moved to Ready to Build |
| CoW | A1 Ecology | — | `/spec-system`: Cellular ecology simulation (biome system, seasonal baselines, population dynamics). §13 Weather Integration (subscribes to `WeatherUpdated`, accumulates rainfall over 13 weeks per season). §14 Interface Contract (exposes `get_biome_seasonal_baseline` for A4). GDD Phase 3 system. **Depends on:** WorldClock (Approved), A4 Weather (unspecced) |
| CoW | A3 (TBD) | — | `/spec-system`: [System name and description TBD from GDD]. Works with A1 and A4 in S-3 Living Ecology testbed. GDD Phase TBD. **Depends on:** TBD |
| CoW | A4 Weather System | — | `/spec-system`: Phase 8 system. Cellular automaton weather simulation (52 weeks/year, per-cell moisture/temperature/pressure/wind). Publishes `WeatherUpdated` weekly with 9 fields: `rainfall`, `temperature`, `snow_cover` (A1 integration), `cloud_cover`, `precipitation_intensity`, `precipitation_type`, `wind_direction`, `snow_pack` (V-1 rendering). 44 tunable constants, 8 debug overlays, 15 commands, 20 edge cases. GDD `A4_WeatherSystem.md` + `A4_TechnicalSpec.md` Locked 2026-10-11. **Depends on:** F-2 (Approved for RNG determinism), RandomStreamManager multi-stream support (Done), S-1 WLD data (elevation, latitude, water_source per cell). **Blocks:** A1 weather integration, V-1 weather rendering |
| CoW | S-2 Single Cell Ecology | — | `/spec-system`: A1 isolated, slider-driven unit test of tick equations (bypasses event interface). GDD `S2_SingleCellEcology.md` Locked 2026-10-11. **Depends on:** F-2 (Approved) for WorldClock time controls, A1 (unspecced) |
| CoW | S-2.5 Weather Patterns | — | `/spec-system`: A4 isolated weather CA simulation testbed. Tests 20 edge cases (coastal rain, rain shadows, storm movement, seasonal variation, spring floods, droughts, orographic lift, snow melt, etc.). 8 debug overlays (Moisture, Wind, Precipitation, Snow Pack, Cloud Cover, Pressure, Temperature, Ground Water). Proves weekly weather cycle (52 weeks/year). Fast-forward time controls. GDD TBD (testbed needs design). **Depends on:** S-1 (WLD data provider), F-2 (Approved for WorldClock), A4 (unspecced) |
| CoW | S-3 Living Ecology | — | `/spec-system`: A1+A3+A4 full world fast-forward, zero dragons, layered visualization (population density, pressure fields, event markers, succession, time-series graphs, 8 A4 weather overlays). Tests A1+A4 integration (rainfall accumulation, weather-driven biome response). GDD `S3_LivingEcology.md` Locked 2026-10-11, updated to include A4. Depends on S-1 being built (world loader), A1 (unspecced), A3 (unspecced), A4 (unspecced), S-2.5 (A4 isolated proof) |
| CoW | S-4 Dragon | — | `/spec-system`: isolated and live modes; pathfinding. **Depends on:** S-3 (unspecced), F-2 (Approved), A1 (unspecced) |
| CoW | S-5 Lineage & Hearth | — | `/spec-system`. Depends on S-4 |
| CoW | S-6 Dynasty | — | `/spec-system`: heaviest testbed; command may split off. Depends on S-5 |
| CoW | S-7 Endurance | — | `/spec-system`: stress load, long-run history and save paging. Depends on S-1 to S-6 |
| CoW | V-1 Tile Rendering | — | `/spec-system`: 5-layer rendering (base tile → seasonal shader → cloud shader → snow overlay → rain/snow particles). Weather visualization (subscribes to A4 WeatherUpdated, caches per-cell weather state). Particle systems with viewport culling (200×200 scale). Fog of war, camera. GDD `V1_TileRendering.md` Locked 2026-10-11. **Depends on:** F-1 (Approved), A4 (unspecced for weather integration) |
| CoW | V-2 Dragon & Nest Visuals | — | `/spec-system`: genome-to-visual, lifecycle stages, nest diorama. Depends on V-1 |
| CoW | V-3 UI & Interaction | — | `/spec-system`: UI kit, input, lineage view, event readability (canned data, so needs the bus recorder). Depends on F-1, bus recorder |
| CoW | A-1 Audio | — | `/spec-system`: ambient soundscape, notification sounds, time-speed-aware audio. Depends on the Dia audio module row below |
| Dia | Audio module | — | Needs a Dia spec first; Dia has no audio module. CoW needs it for A-1 |
| CoW | G-1 First Playable | — | `/spec-system`: real front end and every testbed's visuals toggled on; replaces the `FrontEnd` stub. Depends on all testbeds |
| Dia | RenderTestPlugin (CluicheEditor) | — | Needs `/spec-system` — visual debugger panel: wipe slider, region grid, expectation authoring, AI triage panel, render targets. DiaRenderTest CLI Pipeline ✅ unblocked. Mockup: [render_test_debugger_mockup.html](research/render_offline_test/render_test_debugger_mockup.html). Research: [render_offline_test/summary.md](research/render_offline_test/summary.md) |

---

## E2E Orchestration Stack

Architecture redesigned 2026-05-20. Source of truth: **[docs/research/e2e_testing/design-decisions.md](research/e2e_testing/design-decisions.md)**.

### Deferred

| # | Item | Notes |
|---|------|-------|
| 6b | Extract AutomationModuleBase into DiaAutomation | Evaluate from working code after 2+ apps use it |
| DiaRig3D system | feature spec exists (`skeleton-and-pose.md`) | Needs `/spec-system` — Bone3D, Skeleton3D, Pose3D, FK, `SkeletonComponent3D`, `Rig3DAsset`. Mirrors DiaRig2D. |
| DiaAnimation3D system | feature spec exists (`clip-and-player.md`) | Needs `/spec-system` — AnimationClip3D, ClipPlayer3D, STEP/LINEAR/CUBICSPLINE, `AnimationComponent3D`. glTF animation import is build-time only. |
| DiaStateMachineEditor system | TBD | Needs `/spec-system` — editor plugin for state machine visual debugging + design-time editing. Depends on DiaStateMachine ✅, DiaEditor |
| DiaSkinning3D | TBD — needs `/spec-system` | `skinning-palette` feature already Approved; needs own system spec. SkinningManager, per-frame Matrix34 palettes, `skinningPaletteIndex` on draw commands. | DiaAnimation3D, DiaGraphics3D |

---

---

## Loose Ends (non-spec items)

| Project | Item | Notes |
|---------|------|-------|
| Dia | RenderTechnique asset type | Layer-level rendering policy (blend mode, post-process like bloom/distortion). Layers reference a technique by name; renderer resolves at draw time. Needs `/spec-feature` under DiaGraphics or DiaBgfx once the scene system lands. |
| Dia | Camera2D controller (pan/zoom/reset) | Application-side input→Camera2D wiring for CluicheTest stages (keyboard pan, scroll zoom, home-key reset). Unblocked once coord2d-debug-overlay ships Camera2D + renderer integration. |
| Dia | ~~DiaDebugDraw.vcxproj duplicate-compile hack~~ | `DiaDebugDraw.vcxproj` still duplicate-compiles `DebugLayerManager.cpp` et al. out of `DiaVisualDebugger` via relative path (unrelated to layer violations — `dia check arch` is clean). Tried removing the duplicate entries once (see [arch_layer_violations/plan.md](refactors/arch_layer_violations/plan.md) task 9): broke the real link for `GoogleTests.exe` with 59 unresolved externals, even though `DiaDebugDraw.lib` isn't referenced anywhere in `GoogleTests.vcxproj` (not in `ProjectReference`, not in `AdditionalDependencies`) while `DiaVisualDebugger.lib` — which should contain these exact symbols — is explicitly linked. Reverted immediately, confirmed clean. Working theory, unconfirmed: a shared intermediate-object-path collision between the two projects compiling the same source files under MSBuild's default parallel build. Needs real investigation (check `IntDir` settings, try a forced serial rebuild) before attempting again. |
| CluicheEditor | Uncommitted pre-existing editor UI `dist/` bundles + untracked `External/SDL3` | Found sitting in the working tree during the 2026-10-07/08 DiaConsole run-tabs session, predating that work: modified `dist/` build output under CluicheEditor, DiaChatPlugin, DiaApplicationFlowEditor/Inspector, DiaBlackboardInspector, DiaEntityInspector, DiaPipelineEditor, DiaPythonConsoleEditor, DiaSchemaBrowser; plus an untracked `External/SDL3` directory. Left alone — unclear whether these are in-progress work, stale rebuild output, or should be committed/discarded. Needs owner triage. |
| Dia | Flaky `test_build_end_to_end_produces_a_real_exe` | `Dia/DiaCLI/tests/test_dia_console_console_cli.py` — failed once during a full-suite (incl. integration) run, passed cleanly standalone. Repro attempted 2026-10-08: (1) this test + `test_compile_code_builds_googletest` (real MSBuild compile) forced onto concurrent `pytest-xdist` workers via `-n 2`, both passed in 46s; (2) full 1024-test suite incl. all `integration`-marked tests under `-n auto` (64 workers) — note only one test in the whole suite invokes real PyInstaller, so "concurrent PyInstaller contention" was never literally possible, the real risk is PyInstaller's build racing other heavy integration tests (the MSBuild compile, and `test_test_cli_exits_0`'s nested `dia test cli` subprocess) for CPU/disk on `-n auto`'s worker count — all 1024 passed in 251s, no ordering or resource issue surfaced. Still not reproduced on this (64-core) machine; original failure's host/load conditions unknown. No code change made (anti-shotgun-debugging: no repro, no fix). Next step if it recurs: capture the actual pytest output (return code, stderr tail) from the failing run instead of retrying blind. |
| CoW | `new-cow-stage` skill trial run (spec task 2, AC4/AC5) | Run `/new-cow-stage` on the first real testbed once its design is locked in the GDD; fix the skill from what it shows. Skill and command are written. **On hold (2026-10-09):** the skill's locked check (crossed-out pieces in the roster below) fails for every testbed. F-2 is Locked in the GDD but not crossed out here and has no modes, so it cannot prove AC5; S-1 has two modes (proves AC5) but its challenge pass has not run. Decide which testbed, or reconcile F-2 first |
| CoW | Deferred: event replay from file | Until V-3 needs canned event data |
| CoW | Deferred: `reflect` pipeline stage (schema file) | Until the editor needs CoW's schema |
| CoW | Deferred: tablet support, touch input | Later; Dia has no touch input |
| CoW | Deferred: status badges for testbeds | CoW could add a `StageStatusModule` feeder later |

---

## CoW: Features to Be Designed (GDD)

The testbeds and the pieces each absorbs, from `TestbedRoster.md`. Cross a piece out (`~~like this~~`) once its testbed design is locked in the GDD, and make sure its `/spec-system` row exists under Spec Work Needed. Strike a testbed only when all its pieces are.

| # | Testbed | Absorbs | Notes |
|---|---------|---------|-------|
| F-1 | Harness | ~~Boot & Debug Menu~~ · ~~Event Bus & Inspector~~ · Record & Replay | `F1_Harness.md` Locked; 4 debug infrastructure specs Approved 2026-10-11 |
| F-2 | Core Runtime | ~~Clock & Determinism~~ · ~~RNG~~ · ~~Data Loading & Band Resolver~~ · ~~save/load round-trip~~ | `F2_CoreRuntime.md` Locked 2026-10-09; system spec Approved 2026-10-11 |
| S-1 | World Generation | ~~World Generation~~ | `S1_WorldGeneration.md` Locked 2026-10-11; system + 7 features Approved 2026-10-11. **New:** geographic region identification |
| S-2 | Single Cell Ecology | Single Cell (isolated A1 unit test) | `S2_SingleCellEcology.md` Locked 2026-10-11; needs `/spec-system` |
| S-2.5 | Weather Patterns | Weather CA (isolated A4 test, 8 overlays, 20 edge cases) | **NEW 2026-10-11:** A4 isolated testbed; needs GDD design pass |
| S-3 | Living Ecology | Living Ecology (A1+A3+A4 full world, zero dragons) | `S3_LivingEcology.md` Locked 2026-10-11, updated to include A4; needs `/spec-system` |
| S-4 | Dragon | Dragon Sandbox (isolated mode) · Dragon in the World (live mode) · Pathfinding | Needs GDD design pass |
| S-5 | Lineage & Hearth | Single Hearth · Hearth Economy & Lifecycle · Breeding & Drift | Needs GDD design pass |
| S-6 | Dynasty | Command Chain · Clans & Politics · Apotheosis (late-game preset) | Needs GDD design pass |
| S-7 | Endurance | Swarm & Scale · History & Save | Needs GDD design pass |
| V-1 | Tile Rendering | ~~Grid~~ · ~~overlays~~ · ~~fog of war~~ · ~~camera~~ | `V1_TileRendering.md` Locked 2026-10-11; 5-layer rendering + A4 weather integration; needs `/spec-system` |
| V-2 | Dragon & Nest Visuals | Dragon Visuals · Nest Diorama | Needs GDD design pass |
| V-3 | UI & Interaction | UI kit · Input · Lineage View · Event Readability | Needs GDD design pass |
| A-1 | Audio | none listed | Dia has no audio module |
| G-1 | First Playable | Front End · every testbed's real-visuals toggle | Final integration testbed |
