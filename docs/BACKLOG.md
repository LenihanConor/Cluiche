# Cluiche Backlog

Derived from spec status across `docs/specs/`. When a spec moves to Done, move it to [BACKLOG-HISTORY.md](BACKLOG-HISTORY.md).

One backlog for every project. The **Project** column says whose work it is: `Dia` (engine, including DiaCLI and editors), `CluicheTest`, `CoW`, or `Baseline` (CluicheGameBaseline). Filter with `/backlog <project>`. An engine capability a game needs is a `Dia` row with "CoW needs" in the notes, not a separate list (cross-boundary detail: [dia-needs.md](specs/applications/cow/dia-needs.md)).

**CoW rows.** CoW design intent lives in the GDD (`C:\GitHub\DesignDocs\COW`): systems in `Output Docs/GDD/08_SystemsDeepDive/`, testbeds in `Output Docs/GDD/10_PrototypeStrategy/TestbedRoster.md`; specs are under [specs/applications/cow/](specs/applications/cow/cow.md). Every CoW row is one of two kinds, each with one clear deliverable:

- **System**: one GDD system (A1, A4, ...) built as CoW modules. Deliverable: the system spec, its features, and the module(s) with unit tests.
- **Testbed**: one stage (AD-005) that proves one or more systems. Deliverable: the stage, its modes and presets as panels, and its pass/fail tests.

Sub-tasks (individual features, CLI changes, skills) live in the item's spec and plan, not here.

---

## Ready to Build (Approved → implement)

These specs are `Approved` with all features `Approved`. No spec work needed — go straight to implementation.

### Systems

| Project | System | Features | Depends On |
|---------|--------|----------|------------|
| Dia | Debug Console and Command Lifecycle | Specs Approved 2026-10-13: (1) [command-unregister](specs/applications/dia/systems/diaapi/command-unregister.md) / [plan](specs/applications/dia/systems/diaapi/command-unregister.plan.md) (7 tasks: `UnregisterCommand`, thread-safe registry, uncapped list), (2) [panel-console](specs/applications/dia/systems/diadebugdomain/panel-console.md) / [plan](specs/applications/dia/systems/diadebugdomain/panel-console.plan.md) (10 tasks: extends the `~` panel command strip with output, parser, history, autocomplete, log view, 240-char input). Replaces ModuleDebugRegistry and DiaDebugDataPanel (both dropped). Build order: (1) then (2). Baseline/CluicheTest tasks need sign-off | None |
| Dia | DiaDebugPalette | Spec Approved 2026-10-13: [panel-palette](specs/applications/dia/systems/diadebugdomain/panel-palette.md) / [plan](specs/applications/dia/systems/diadebugdomain/panel-palette.plan.md) (7 tasks). Ctrl+P overlay in the debug panel over commands and domains; fuzzy search | Debug Console and Command Lifecycle |

### CoW

| Project | Item | Kind | Deliverable | Spec / plan | Depends On |
|---------|------|------|-------------|-------------|------------|
| CoW | F-1 Harness (remaining) | Testbed | Boot, skeleton and tooling are Done (plan tasks 1 to 9, see history). Still open: debug command / inspector host (CoW wiring only: add `UIModule`, `VisualDebuggerModule`, `DebugPanelPageModule`, `DebugServerHostModule`, a `MessageBusModule` plus one message type to `cow_main.diaapp`, and copy `debug-panel.html` into `Assets/CoW`; the copy can drift from CluicheTest's, so share it when the console work changes the file), bus traffic recorder CoW wiring (path, tick source, lifecycle; F-1 Test 6; being done in another agent), plan tasks 11 and 12. (Bus live verification is Done via CluicheTest `MessageBusStressTestStage`; plan task 10 is N/A, since the module doc already shows `status: active`.) Record & Replay still needs GDD design | [f1-harness.md](specs/applications/cow/systems/f1-harness/f1-harness.md) / [plan](specs/applications/cow/systems/f1-harness/f1-harness.plan.md) | Dia: bus-traffic-recorder (Approved); the wiring itself needs no new Dia code. Console, command lifecycle and Palette are separate Dia items (Spec Work Needed). Recorder placement is settled: Dia `IBusRecorder` in DiaMessageBus |
| CoW | GameSpeedController (A2) | System | Approved 2026-10-13 (re-scoped from WorldClock). EU4/Civ-style speed control: Pause + 4 presets (`.diaapp` module config) via DiaSimTime time scale (no Dia change). Day is the base tick: `SimulationTick{day, band}` with Day/Week/Season/Year/Decade/Century bands. Modal-safe pause rules. Top speed capped at 1 day per sim frame (30 days/s). 2 features: (1) Core logic (TDD), (2) SimModule wiring. Serialization → E1, buttons → F-2. Spec lists 9 GDD A2 deviations to fold back | [gamespeedcontroller.md](specs/applications/cow/systems/gamespeedcontroller/gamespeedcontroller.md) | DiaSimTime (Done), DiaMessageBus (built) |
| CoW | E1 Save/Load | System | CoW's integration with DiaSaveGame: (1) SaveLoadModule Core, (2) GameSpeedController Serialization, (3) RandomStream Serialization, (4) Save/Load Bus Messages, (5) Frame-Boundary Save (replaces TickCompleted). GDD `E1_SaveContinue` Locked 2026-10-09 | [e1-saveload.md](specs/applications/cow/systems/e1-saveload/e1-saveload.md) | DiaSaveGame (Done), GameSpeedController |
| CoW | F-2 Core Runtime | Testbed | `F2CoreRuntimeStage` + `F2TestRunnerModule` (DiaAutomation), 6 tests: stream independence, determinism round trip, save/load RNG round trip, clock proof, save/load proof, band resolver timing. GDD `F2_CoreRuntime.md` Locked 2026-10-09 | [f2-core-runtime.md](specs/applications/cow/systems/f2-core-runtime/f2-core-runtime.md) | GameSpeedController, E1, RandomStreamManager (Done), Dia: DiaAttribute Feature 7 (Test 6 only) |
| CoW | S-1 World Generation | Testbed | WLD pipeline: (1) placeholder generation algorithm, (2) reachability validator, (3) retry, (4) JSON export/import, (5) timeout/failure, (6) F-1 debug host integration, (7) layered debug rendering; 3 hard guarantees as pluggable `IWorldGuarantee`s, reachability via injected `INavigationProfile`s (DiaPathfinding `IPathCostProvider`; placeholder terrain-table profile now, B2 body plans injected later by S-4). GDD `S1_WorldGeneration.md` Locked 2026-10-11. Also tags each cell with `geographic_region_id` (valleys, mountain ranges, coastal zones, highlands, lowlands, from elevation + water proximity) for D4 narrative, C1 territory and rival spacing; A4 uses per-cell weather, not regions | [worldgenerationpipeline.md](specs/applications/cow/systems/s1-worldgeneration/worldgenerationpipeline.md) | F-2, F-1 (Feature 6). No B2 dependency |

---

## Spec Work Needed (Draft or unset — review/approve before building)

### Other Spec Work

| Project | Item | Spec | What's needed |
|---------|------|------|---------------|
| Dia | Audio module | — | Needs a Dia spec first; Dia has no audio module. CoW needs it for its A-1 Audio testbed |
| Dia | RenderTestPlugin (CluicheEditor) | — | Needs `/spec-system` — visual debugger panel: wipe slider, region grid, expectation authoring, AI triage panel, render targets. DiaRenderTest CLI Pipeline ✅ unblocked. Mockup: [render_test_debugger_mockup.html](research/render_offline_test/render_test_debugger_mockup.html). Research: [render_offline_test/summary.md](research/render_offline_test/summary.md) |

### CoW

Systems first, then the testbeds that prove them.

| Project | Item | Kind | Deliverable | GDD | Depends On |
|---------|------|------|-------------|-----|------------|
| CoW | A4 Weather | System | `/spec-system`: weekly cellular-automaton weather (52 weeks/year; per-cell moisture, temperature, pressure, wind). Publishes `WeatherUpdated` weekly: `rainfall`, `temperature`, `snow_cover` (for A1); `cloud_cover`, `precipitation_intensity`, `precipitation_type`, `wind_direction`, `snow_pack` (for V-1). 44 constants, 8 debug overlays, 15 commands, 20 edge cases. **Owns the A1 ↔ A4 contract:** defines `WeatherUpdated` and the `get_biome_seasonal_baseline` query A1 must provide. Unblocks A1 weather integration and V-1 weather rendering | `A4_WeatherSystem.md` + `A4_TechnicalSpec.md` Locked 2026-10-11 | S-1 (WLD elevation, latitude, water source per cell), F-2 (RNG determinism), RandomStreamManager multi-stream (Done) |
| CoW | A3 Biome Succession | System | `/spec-system`: multi-cell succession FSM (forward succession, staged degradation, asymmetric recovery, cell visual states) | `A3_BiomeSuccession/` Locked | A1 (cell schema) |
| CoW | A1 Ecology | System | `/spec-system`: cell schema, tick pipeline (producer/consumer, pressure fields, succession counter, event thresholds), species roster, biomes and seasonal baselines. §13 Weather Integration: consumes A4's `WeatherUpdated`, accumulates rainfall over the 13 weeks of each season. §14 Interface Contract: provides `get_biome_seasonal_baseline` to A4 | `A1_Ecology/` Locked 2026-10-12 (re-locked after the DECISION-122 species/consumer-model redesign and Tier 2 Challenge Pass) | GameSpeedController, A4 contract |
| CoW | S-2 Single Cell Ecology | Testbed | `/spec-system`: one isolated cell driven by sliders; proves A1's tick equations. No A4: extreme weather is deferred to S-3 | `S2_SingleCellEcology.md` Locked 2026-10-11 | A1, F-2 (time controls) |
| CoW | S-3 Living Ecology | Testbed | `/spec-system`: full S-1 world fast-forwarded, no dragons; proves A1+A3+A4 emergence (booms, crashes, drought, succession, weather-driven biome response, rainfall accumulation). Layered visualization: population density, pressure fields, event markers, succession, time-series graphs, the 8 A4 overlays | `S3_LivingEcology.md` Locked 2026-10-11, updated to include A4 | S-1 (built, world loader), A1, A3, A4. S-2.5 was listed as a prerequisite; it waits on the GDD decision below |
| CoW | V-1 Tile Rendering | Testbed | `/spec-system`: 5-layer tile rendering (base tile, seasonal shader, cloud shader, snow overlay, rain/snow particles), all biome/ecology/season/weather tile variants, continuous zoom, particle viewport culling at 200×200. Subscribes to `WeatherUpdated` and caches per-cell weather state. The old backlog also had fog of war and camera here; the roster now puts fog of war in V-2 | `V1_TileRendering.md` Locked 2026-10-11 | F-1, A1, A3, A4 (`WeatherUpdated`) |

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
| Dia | ~~`dia check sanitizer` fails to link~~ | **Fixed 2026-10-13.** Three separate problems: (1) LNK1319/LNK2038 `annotate_string` mismatch from prebuilt third-party libs: `_DISABLE_STRING_ANNOTATION`/`_DISABLE_VECTOR_ANNOTATION` for Debug-Asan (`45cdd688`); (2) the Asan exe could not start (`STATUS_DLL_NOT_FOUND`, ASan runtime not on PATH) and the command reported a crash as "No sanitizer findings": runtime dir now on PATH and the result is honest (`8f7b66c6`); (3) real bugs found by ASan and fixed: `StringCRC::CopyString` over-read (`183765df`), `String<N>` converting constructor over-read (`ed70501d`), `Application` cached metric pointers freed by `MetricRegistry::Reset()` (`5a19ca23`). Whole suite now runs under ASan: 9183 tests pass, 0 AddressSanitizer findings, including the registry stress test and the console log ring tests |
| Dia | 3 tests fail only in the whole-suite ASan run | `SessionContextTest.SaveAndLoadRoundTrip`, `FileDiscovererScanTest.DiscoversFindsPngAndConfigFiles`, `FileDiscovererScanTest.ExcludesAlreadyRegistered` (all `Cluiche/Tests/GoogleTests/DiaAssetCatalogueEditor/TestDiaAssetCatalogueEditorExhaustive.cpp`). They pass in the normal Debug suite (single process too) and pass when run alone under Debug-Asan, so it is an interaction with something earlier in the long ASan run, not ASan itself and not the working directory (the runner and the sanitizer command use the same cwd). They use relative `Cluiche/out/...` paths named with `GetTickCount()` and `system("rmdir ...")`. Until fixed, `dia check sanitizer` exits non-zero ("Test run FAILED ... NOT a clean pass") even with 0 ASan findings |
| Dia | `--shards` runs under Debug-Asan still cannot find the ASan runtime DLL | `dia_cli/commands/test/shard_runner.py` was not changed when `asan_runtime.with_asan_runtime_on_path` was added to `launch.py` and `googletest_runner.py` |
| Dia | E2E `geometry2d` scenario fails on first stage entry | Found 2026-10-13: `dia test e2e --scenario="*geometry2d*"` (`Cluiche/Tests/E2E/scenarios/cluichetest/geometry2d_stage/smoke.py`) fails at the FIRST `navigate_to("Geometry2DTestStage")` with `Geometry2D test failed: pending` (checkpoint `test.geometry2d.passed` not reached in 5 s). **Blocks:** the stage re-entry check for command-unregister task 5 (Boot, Geometry2D, Boot, Geometry2D, no "already registered (duplicate)" in the log), which has therefore never run; task 5 is verified for compile only. Unknown whether it predates task 5 (that change only touches `OnStop` of `Geometry2DTestStageModule` and `EntityInspectorModule`). **Update 2026-10-13:** the manual re-entry check (Boot, Geometry2D, Boot, Geometry2D) was run by the user and works, so task 5 is verified; only the E2E scenario itself is still failing. Options: (a) build the commit before `075a02de` in a separate worktree and run the same scenario, (b) do the manual re-entry check with `dia launch cluichetest`, (c) raise the 5 s timeout if the Debug build is simply slow. Decision pending |
| Dia | `dia validate manifest` misses ServiceStream orphan providers | `ManifestValidatorV2` raises `SERVICE_STREAM_ORPHAN_PROVIDER` (a provider with no `consumes` module) as an error at launch, and `LoadManifest` prints every warning as "Validation error" when any error exists, but the Python `dia validate manifest` and `dia check deps` pass. Found wiring CoW's debug host (f1-harness task 13). Align the CLI validator with the C++ one, and label warnings as warnings in `LoadManifest` |
| Dia | ~~`dia launch cluichetest` / `--automation` fail: "Failed to compose manifest"~~ | **Fixed 2026-10-13** (`ab3eb4b1`): `MessageBusStressTestStage` (added Oct 9, `f717ac07`) was missing from CluicheTest's `assets.catalogue.json`, so it was never deployed while `cluichetest.diagame` imports it; entries added. Both launch modes boot again and `dia test e2e` now runs. `websockets` was pip-installed into the Python env. The geometry2d E2E failure found afterwards is its own row below |
| Dia | RenderTechnique asset type | Layer-level rendering policy (blend mode, post-process like bloom/distortion). Layers reference a technique by name; renderer resolves at draw time. Needs `/spec-feature` under DiaGraphics or DiaBgfx once the scene system lands. |
| Dia | Camera2D controller (pan/zoom/reset) | Application-side input→Camera2D wiring for CluicheTest stages (keyboard pan, scroll zoom, home-key reset). Unblocked once coord2d-debug-overlay ships Camera2D + renderer integration. |
| Dia | ~~DiaDebugDraw.vcxproj duplicate-compile hack~~ | `DiaDebugDraw.vcxproj` still duplicate-compiles `DebugLayerManager.cpp` et al. out of `DiaVisualDebugger` via relative path (unrelated to layer violations — `dia check arch` is clean). Tried removing the duplicate entries once (see [arch_layer_violations/plan.md](refactors/arch_layer_violations/plan.md) task 9): broke the real link for `GoogleTests.exe` with 59 unresolved externals, even though `DiaDebugDraw.lib` isn't referenced anywhere in `GoogleTests.vcxproj` (not in `ProjectReference`, not in `AdditionalDependencies`) while `DiaVisualDebugger.lib` — which should contain these exact symbols — is explicitly linked. Reverted immediately, confirmed clean. Working theory, unconfirmed: a shared intermediate-object-path collision between the two projects compiling the same source files under MSBuild's default parallel build. Needs real investigation (check `IntDir` settings, try a forced serial rebuild) before attempting again. |
| CluicheEditor | Uncommitted pre-existing editor UI `dist/` bundles + untracked `External/SDL3` | Found sitting in the working tree during the 2026-10-07/08 DiaConsole run-tabs session, predating that work: modified `dist/` build output under CluicheEditor, DiaChatPlugin, DiaApplicationFlowEditor/Inspector, DiaBlackboardInspector, DiaEntityInspector, DiaPipelineEditor, DiaPythonConsoleEditor, DiaSchemaBrowser; plus an untracked `External/SDL3` directory. Left alone — unclear whether these are in-progress work, stale rebuild output, or should be committed/discarded. Needs owner triage. |
| Dia | Flaky `test_build_end_to_end_produces_a_real_exe` | `Dia/DiaCLI/tests/test_dia_console_console_cli.py` — failed once during a full-suite (incl. integration) run, passed cleanly standalone. Repro attempted 2026-10-08: (1) this test + `test_compile_code_builds_googletest` (real MSBuild compile) forced onto concurrent `pytest-xdist` workers via `-n 2`, both passed in 46s; (2) full 1024-test suite incl. all `integration`-marked tests under `-n auto` (64 workers) — note only one test in the whole suite invokes real PyInstaller, so "concurrent PyInstaller contention" was never literally possible, the real risk is PyInstaller's build racing other heavy integration tests (the MSBuild compile, and `test_test_cli_exits_0`'s nested `dia test cli` subprocess) for CPU/disk on `-n auto`'s worker count — all 1024 passed in 251s, no ordering or resource issue surfaced. Still not reproduced on this (64-core) machine; original failure's host/load conditions unknown. No code change made (anti-shotgun-debugging: no repro, no fix). Next step if it recurs: capture the actual pytest output (return code, stderr tail) from the failing run instead of retrying blind. |
| CoW | `new-cow-stage` skill trial run | [new-cow-stage-skill.md](specs/applications/cow/systems/f1-harness/new-cow-stage-skill.md) AC4/AC5: run `/new-cow-stage` on the first locked testbed and fix the skill from what it shows. Skill and command are written (AC1 to AC3). **On hold since 2026-10-09:** pick the testbed. F-2 is locked but has no modes, so it cannot prove AC5; S-1 has two modes (proves AC5), but its challenge pass had not run then. The skill's locked check uses this file's CoW layout |
| CoW | Shared application-bootstrap helper | CluicheTest's `Main.cpp` has generic path-alias and base-dir code CoW copies. Not yet logged in dia-needs.md; log it if wanted |
| CoW | Deferred: Event replay from file | Deferred until V-4 needs canned event data |
| CoW | Deferred: `reflect` pipeline stage for CoW (schema file) | Omitted until the editor needs CoW's schema |
| CoW | Deferred: Tablet support, touch input | Later; Dia has no touch input |
| CoW | Deferred: Status badges for CoW testbeds | None for now; CoW could add a `StageStatusModule` feeder later |

---

## CoW: Features to Be Designed (GDD)

Testbeds whose design is not yet locked, from `TestbedRoster.md`. When a piece is locked, cross it out (`~~like this~~`); when every piece is, add the testbed's Testbed row to **Spec Work Needed → CoW**, remove it here, and log it in BACKLOG-HISTORY.md under "CoW: Designed in the GDD". Testbeds already locked (F-2, S-1, S-2, S-3, V-1) are not listed.

| # | Testbed | Absorbs (cross out when designed) | Notes |
|---|---------|-----------------------------------|-------|
| F-1 | Harness | ~~Boot & Debug Menu~~ · ~~Event Bus & Inspector~~ · Record & Replay | Debug host wiring is Ready to Build; console, command lifecycle and Palette are Dia spec work |
| S-2.5 | Weather Patterns | Weather CA (isolated A4 proof) | **Not in `TestbedRoster.md` yet** and has no design doc. A4 has no `PrototypeTestbed` file; this would be it. Decide in the GDD whether it is a separate testbed or a mode of S-3. Proposed scope: 20 edge cases (coastal rain, rain shadows, storm movement, seasonal variation, spring floods, droughts, orographic lift, snow melt, ...); 8 overlays (moisture, wind, precipitation, snow pack, cloud cover, pressure, temperature, ground water); weekly cycle over 52 weeks; fast-forward. Depends on S-1, F-2, A4 |
| S-4 | Dragon | Dragon Sandbox (isolated mode) · Dragon in the World (live mode) · Pathfinding | Live mode runs in S-3's world. Also supplies B2's body-plan `INavigationProfile`s, injected into S-1's validator. Depends on S-3, F-2, A1 |
| S-5 | Lineage & Hearth | Single Hearth · Hearth Economy & Lifecycle · Breeding & Drift | The dependency graph's B4/B6/C6 bucket. Depends on S-4 |
| S-6 | Dynasty | Command Chain · Clans & Politics · Apotheosis (late-game preset) | Heaviest testbed; command may split off. Depends on S-5 |
| S-7 | Endurance | Swarm & Scale · History & Save | Stress load, long-run history and save paging. Depends on S-1 to S-6 |
| V-2 | Overlays & Effects | Fog of war · Territory overlays · D3 consequence overlays | |
| V-3 | Dragon & Nest Visuals | Dragon Visuals · Nest Diorama | Genome-to-visual, lifecycle stages. Was "V-2" in the old backlog. Depends on V-1 |
| V-4 | UI & Interaction | UI kit · Input · Lineage View · Event Readability (canned events first, then live) | Was "V-3" in the old backlog. Depends on F-1 and the bus recorder (canned data) |
| A-1 | Audio | none listed | Ambient soundscape, notification sounds, time-speed-aware audio. Dia has no audio module, so the Dia spec comes first (main backlog, [dia-needs.md](specs/applications/cow/dia-needs.md) #5) |
| G-1 | First Playable | Front End · every testbed's real-visuals toggle switched on | Replaces the `FrontEnd` stub; needs all testbeds |
