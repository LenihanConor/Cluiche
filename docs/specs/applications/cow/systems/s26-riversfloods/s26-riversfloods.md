# System Spec: S-2.6 Rivers & Floods

**Parent:** @docs/specs/applications/cow/cow.md  
**Testbed:** required  
**Design source:** `Output Docs/GDD/10_PrototypeStrategy/S2.6_RiversFloods.md` (DesignDocs repo, Locked 2026-11-17 after a Tier 2 Challenge Pass; DECISION-135, CHANGE-050)  
**System under test:** @docs/specs/applications/cow/systems/a5-hydrology/a5-hydrology.md (A5 owns the tools and presets; this testbed hosts them). A4 is the weather source in Coupled mode only.  
**Sibling testbed (pattern source):** @docs/specs/applications/cow/systems/s25-weatherpatterns/s25-weatherpatterns.md

**Gameplay Domains:** debug

## Summary

S-2.6 is the testbed for CoW's **water layer**. It runs A5 Hydrology on S-2.5's two curated S-1 worlds, with no ecology, no succession and no dragons. It proves the six A5 success criteria before A1 and A3 read A5 water: floods travel, rivers shrink in drought but do not vanish, lakes breathe, the ice age shows then retreats, water balances, and nothing is cut off.

```
Dragons / game   S-4+         later
Ecology          S-3          A1, then A3 (reads A5 water)
Water            S-2.6        ← this testbed (A5)
Weather          S-2.5        A4
Terrain          S-1          live terrain editor
```

A5's debug domain, the eleven layers, cell inspector, `cow.hydro.*` commands, the four presets, the `ScriptedWaterInput` driver and the R-* regression tests belong to A5 (Features 5 and 6). S-2.6 adds the stage, the curated-world prechecks, ten verdict tests as numeric checks, the headless runner and report, the guided walkthrough, the baseline file and the history export.

**Checks are the verdict; overlays explain** (same rule as S-2.5). Each GDD test is a numeric predicate over A5's committed state. The same predicate runs in a **headless runner** (the verdict, reported through DiaAutomation) and as a **live check line** in the stage.

**Two modes, one stage** (GDD AD-02, AD-05 of the roster):
- **Scripted:** A5 alone on a preset's `ScriptedWaterInput`. No A4, no RNG. Deterministic and fast. Every hard verdict check runs here, so an A4 change cannot fail an A5 check.
- **Coupled:** A4 and A5 live in World Step order. A preset applies its fills only; A4 supplies the weather. Hosts Test 8, the climate-cycle scrubber and the `SLOW_` coupled run.

## Goals

1. **Prove A5 in isolation.** Tests 1–10 run as numeric checks and produce a pass/fail report. A hard fail names the A5 TechSpec section to reopen.
2. **Understand water.** Every hydrology layer can be seen on the map, inspected per cell, scrubbed back through A5's recorded history and graphed over years, on a known world.
3. **Fast, repeatable verdicts.** The Scripted headless runner finishes the plan without rendering, so it runs in automation on every A5 change.
4. **Honest thresholds.** Direction and ordering checks are hard. Magnitudes the GDD does not state are recorded on the first run and frozen (±10%). A magnitude miss goes to the user, not straight to an A5 reopen (S-2.5 AD-06 lesson, CH-S26-03).
5. **Shared guided scenarios.** Walkthroughs use the CoW-Shared scenario format S-2.5 Feature 4 builds.

## Non-Goals

- **No ecology and no succession.** A3-facing counters (`inundated_weeks`, `exposed_weeks`, `low_flow_weeks`, `glacier_weeks`, `glacier_free_weeks`) are checked as numbers only. The DRY_RIVERBED marker, Lake and Wetland conversion are S-3's job (CH-S26-02).
- **No dragons and no event-subscriber checks.** A5's events are shown through `cow.hydro.find`.
- **No tuning of A5 defaults.** `set_param` exists for understanding. S-2.6 records first-run values.
- **No water rendering.** Debug-grade layers only; look is V-1.
- **No new presets** (user decision 2026-11-17). The four A5 presets cover it.
- **Small map only for verdicts** (SD-S1-006). Standard and Large step time is report-only.
- **Equation correctness.** A5's GoogleTests own that on hand-built grids. S-2.6 checks behaviour on real S-1 worlds.
- **Designing an `s26_*` world.** If both curated worlds fail a precheck, the test reports "no valid world" and the user decides.

**Launch-out from S-3.5:** at stage start S-2.6 reads F-3's session hint (`out/CoW/session/next_bundle.json`) as S-2.5 does, so "Open in S-2.6" continues with the same world. Needs S-3.5 Feature 4 to list S-2.6 as a target; confirm at Feature 1.

## Architecture

### Placement

**CoW feature-local** (AD-002), user decision 2026-11-24: the stage, checks, runner and report live in `Cluiche/CoW/Modules/` (with `S26` prefixes) and `Cluiche/Assets/CoW/Stages/S26RiversFloods/`. The scenario format, `ScenarioValidator`, `CheckRegistry` and check-line panel are **reused from CoW-Shared** (built by S-2.5 Feature 4). Nothing is added to Dia or Draoi, and no CoW-Shared helpers are added (only S-2.6 needs the flood and balance checks; promote if S-3 or S-3.5 asks).

```
SimPU (S26RiversFloods stage)
  ├─ WorldGenerationModule     loads the stage's world (SD-S1-010: preset + seed)
  ├─ GameSpeedControllerModule, RandomStreamModule
  ├─ WorldStepModule           Scripted: ["hydro"]   Coupled: ["weather", "hydro"]; synchronous by default, Sync/Deferred switch
  ├─ WeatherModule             Coupled mode only
  ├─ HydrologyModule           A5 stage, debug domain, history block (ScriptedWaterInput drives it in Scripted mode)
  └─ S26RiversFloodsModule     world and mode picker, live check lines, run controls, forced years, climate scrubber, S26 card
                               └─ S26HeadlessRunner (worker job; its own HydrologySimulation, Scripted only)
```

No `EcologyModule` or `SuccessionModule`. Switching mode reloads the world and clears history (GDD AD-02).

### Worlds and prechecks

Reuses S-2.5's two curated S-1 preset + seed worlds (`s25_coast_range`, `s25_dry_interior`). Each test runs only on a world that passes its **precheck**. A failed precheck means "invalid world, re-pick the seed or change the test's world", not an A5 failure.

| Precheck | Needed by | Check |
|---|---|---|
| Long river | Tests 1, 8 | a river path ≥ 20 cells from a cell with elevation ≥ 0.6 to the sea or a basin |
| River with a crossing | Test 7 | at least one `isCrossing` river cell with a river path upstream ≥ 6 cells (`upstreamOfCrossing`) |
| Dry basin and a lake basin | Test 3 | one non-lake and one lake basin, both ≥ `MIN_BASIN_CELLS` |
| Cold high cells | Test 4 | at least 20 cells A5 can mark `glacierEligible` under the preset's cold phase |
| Corridors and protected sites | Test 6 | S-1's three guarantees hold on the unflooded world |

Default assignment: `s25_coast_range` for Tests 1, 2, 3, 4, 7, 8; `s25_dry_interior` for Tests 2 (drought), 3 (dry basin) and 6. Version 1 world files import lakes-only, so Test 3 needs a world with a basin table.

### Checks

A check is a named pure C++ predicate over A5's committed state and the world, in `S26HydrologyChecks`, using the same `CheckResult` / `CheckKind` / `CheckState` shape as S-2.5 (`CoW::S26` namespace). Kind is `Hard` or `Default` (first-run magnitude). Each failing hard check carries a `reopenSection` naming the A5 TechSpec section.

| Test | Scope | Hard checks (summary) | Default magnitudes |
|---|---|---|---|
| 1 Floods Travel | Scripted, `melt_pulse` | `Wm ≥ Wh`; mouth peak < head peak; flood flag appears and clears `recovery_clear_weeks` after the last melt week; cumulative sea outflow equals input within tolerance | `Wm − Wh` 3–10 weeks (a `k` under 3 goes to the user, CH-S26-03) |
| 2 Rivers Shrink, Not Vanish | Scripted, dry year and `wet_then_dry` decade | mean flow falls in the dry year and recovers with rain; `low_flow_weeks` climbs in the dry decade; frozen weeks neither count nor reset it (CH-A5-02) | ≥ 80% of river cells keep flow > 0 |
| 3 Lakes Breathe | Scripted, `wet_then_dry` | dry basin level rises in the wet year; `inundated_weeks` reaches 104 (read from A3's `lake_inundation_weeks`); every basin lower at the end of the dry decade; some cell has `exposed_weeks > 0`; wet mean flow > dry mean | wet/dry flow ratio; wet years to a Lake |
| 4 Ice Age Shows | Scripted, `ice_age`; Coupled variant pins `ice_age_40y` | rivers and lakes `Frozen` in cold; glacier mass non-decreasing in cold and > 0 after onset + 1 years; a lake lower at cold end; glacier mass falls every warming year; first warming year's flow > last cold year's (Great Thaw) | years to clear (about 10); melt-flow ratio |
| 5 Water Balances | Scripted, every preset, 100 years | weekly `\|error\| ≤ 1e-4 × max(in, out, 1)`; cumulative `\|error\| < 1e-3 ×` throughput | worst weekly error (recorded) |
| 6 Nothing Is Cut Off | Scripted, R-Reach cycle, per world; one `SLOW_` Coupled run | no `isCrossing` or corridor cell `Deep` any week; S-1 validator findings unchanged every year; lakes-only worlds have no `Deep` protected cell | run time (reported) |
| 7 Flash Flood and Fords | Scripted, `flash_flood_fords` | `floodedCells > 0` in an extreme week; crossing never `Deep`; crossing not `Dry` at least one week (non-vacuous); all flood flags clear `recovery_clear_weeks` after | — |
| 8 Weather Drives Water | Coupled, `oscillation_4y`, 8 years, plus S-2.5 `spring_melt` | wettest year's mouth flow > driest's; mouth peak after the mountain melt peak; A5's `water_to_ground` input equals A4's published value each week | — |
| 9 A3 Counters | Scripted, runs of Tests 2, 3, 4 | the five counters rise by exactly one per qualifying week and reset on the documented condition; A3 thresholds (104 / 52 / 26 / 260) shown as "would trigger" lines | — |
| 10 Determinism | Scripted, `melt_pulse` + `flash_flood_fords`; Coupled 104 weeks | bit-identical layers and totals at ranges 1, 2, 7, 16 and serial; Coupled deferred vs synchronous identical; layer goldens match | — |

The exact check ids, the state machine (`Pass`, `Fail`, `MagnitudeMiss`, `Pending`, `InvalidWorld`) and the live-vs-headless split are defined in Feature 2, which is authoritative once written.

### Baseline

`Stages/S26RiversFloods/baseline.json`, one value per world × preset. Frozen after the first run: `k` and head-to-mouth attenuation (Test 1), peak flooded cells for `melt_pulse` and `flash_flood_fords`, dry-year surviving-river fraction (Test 2), wet/dry flow ratio and wet years to a Lake (Test 3), peak glacier mass, years to clear and Great Thaw ratio (Test 4), worst weekly balance error (Test 5). Later runs compare within **±10% relative** (cell counts: ±10% or ±1 cell, whichever is larger). `cow.s26.baseline` records, and refuses to overwrite a frozen value without `force`. `step_ms` and the largest river-tree size are recorded for information and never compared. The file is committed so a change shows in review.

### Headless runner

`S26HeadlessRunner` drives A5's pure `HydrologySimulation` directly through A5's pure `HydrologyRunner` (the same initialise → spin-up → step sequence `HydrologyModule` uses, so the runner cannot drift), on a worker job with progress and cancel. Presets and forced years are applied through **A5's command executor bound to the runner's own simulation**, so a scenario means the same live and headless.

- **Scripted only.** The verdict never depends on A4. Coupled checks (Test 8, the `SLOW_` coupled Test 6, the Coupled Test 4 variant) are live-stage or `SLOW_` GoogleTest runs, and the report lists them as `CoupledOnly`.
- **Performance is report-only** (DECISION-130, A5 §9): `cow.hydro.step_ms` per map size and the largest river-tree size go in the report and baseline file. The panel warns, but does not fail, when A5's step plus A4's step exceeds the World Step slack.
- **Report:** `out/CoW/s26/report_<world>_<seed>.json` (PD-009): one `CheckResult` per check, prechecks, performance, edge-case coverage.
- **Edge cases:** `Stages/S26RiversFloods/edge_cases.json` maps each A5 edge case (`A5_EdgeCaseMatrix.md`) to the scenario that exercises it or to `untested` with a reason, so nothing is silently skipped.
- **Automation:** DiaAutomation triggers `cow.s26.run` and reads the result (F-2 pattern).

### Panel and tools

All hosted tools are A5's or the stage's, and the S26 card calls the same DiaAPI commands (editor-actions rule).

- **Time controls:** F-2's Normal/Fast/Fastest, pause, single-step at weekly cadence, plus a **year-step** (52 weeks).
- **Presets:** `melt_pulse`, `wet_then_dry`, `ice_age`, `flash_flood_fords` via `cow.hydro.preset`.
- **Forced wet year / forced dry year** (52 weeks): Scripted scales the scripted rain; Coupled uses A4's influence API, so A4's weekly per-field caps apply. Tools for understanding, never verdicts (CH-S26-04).
- **Climate-cycle scrubber (Coupled):** pins A4's `ice_age_40y` and `oscillation_4y` to a phase `u` (or lets them run); re-runs from spin-up.
- **Layers** (shared layer picker, flow arrows on top): the 8 `hydro.*` cell layers plus `hydro.river_state`, `hydro.basins`, `hydro.flow_dir`.
- **Click-to-inspect:** every `HydroCell` field, basin, counters and this week's per-pass deltas (A5 debug).
- **Readouts:** water totals by store, the balance-error gauge, `cow.hydro.step_ms` per size, largest river tree, A3-facing counters with "would trigger" lines, live check lines.
- **Step-back and events:** DiaCellHistory scrub (`cells.history.view`) and restore (`cells.snapshot.restore`) over A5's recorded history. S-2.6 requires only that a flood can be scrubbed from peak back to onset. Events (flood start/end, river runs dry, lake level record, freeze-up) are markers in the history export.
- **Graphs:** as S-2.5 Feature 5, there is **no in-game graph panel**. Series (water totals by store, balance error, mouth and head flow of the longest river, selected basin levels, flooded cell count, glacier mass) are DiaCellHistory aggregates exported as `dia.cell_history` from the S26 card and by each headless run, and drawn in the T-1 Inspector.

### Scenarios and walkthrough

Reuses the CoW-Shared `cow.scenario` format (`.scenario.json`, setup as DiaAPI commands, checks registered by the host, `cow.scenario.list|run|walk`). S-2.6 registers its `s26.*` checks and overlays, keeps `Stages/S26RiversFloods/scenarios/vocabulary.json` with the matching GoogleTest, and ships one scenario per test plus the four presets as guided setups. Coupled scenarios set up A4 with lines such as `cow.weather.load_preset name=spring_melt`.

## Public Interface

### Stage

`Cluiche/Assets/CoW/Stages/S26RiversFloods/s26_rivers_floods.diastage`, launched from F-1's debug menu (AD-005, one stage; modes and presets are panel state). Holds the default world (`s25_coast_range` as preset + seed, or a terrain `world_bundle`), the scenario list, the `HydrologyModule` config (parameter file, spin-up weeks, history block) and the World Step order per mode. Stage id for A5 is `hydro`.

### Module

```cpp
namespace CoW::S26 {
    class S26HydrologyChecks {                       // pure; one function per named check
    public:
        static void RegisterAll(CoW::Scenarios::CheckRegistry& registry);
    };

    enum class S26Mode : uint8_t { Scripted, Coupled };
    struct S26RunParams { Dia::Core::StringCRC world; uint32_t seed; bool recordBaseline; };
    struct S26RunReport { DynamicArrayC<CheckResult> checks; /* prechecks, perf, edge-case coverage */ };

    class S26HeadlessRunner {
    public:
        S26RunHandle Start(const S26RunParams& params);   // worker job; progress + cancel
    };

    class S26RiversFloodsModule : public Dia::ApplicationFlow::SimModule {
    public:
        void SetMode(S26Mode mode);                       // reloads the world, clears history
        void LoadWorld(Dia::Core::StringCRC world);
        void RunScenario(Dia::Core::StringCRC id);
        const DynamicArrayC<CheckResult>& GetLiveChecks() const;
        S26RunHandle StartRun(const S26RunParams& params);
    };
}
```

### Commands (DiaAPI)

`cow.s26.world`, `cow.s26.mode`, `cow.s26.run` (`action`: start/cancel/status; `world=`, `seed=`, `size=`), `cow.s26.report`, `cow.s26.baseline` (`record [force]`), `cow.s26.forced_year` (`wet|dry`), `cow.s26.year_step`, `cow.s26.cycle` (Coupled; pin phase), `cow.s26.livesmoke`. Reused: `cow.worldgen.set_size`, `cow.scenario.*`, all `cow.hydro.*` (`state`, `basins`, `find`, `set_param`, `reload`, `preset`, `balance`, `set`) and `cow.weather.*` (Coupled). Final names are fixed in Feature 1 and Feature 3.

## Features

Mirrors S-2.5. Feature specs to be written (`/spec-feature`), all Draft.

| # | Feature | Description | Spec | Status |
|---|---------|-------------|------|--------|
| 1 | Stage & World | `.diastage`, F-1 menu entry, `S26RiversFloodsModule`, mode switch, world load and size picker, the prechecks, A5 (and in Coupled mode A4) hosting, time controls and year-step, forced years, climate-cycle scrubber, layers, click-to-inspect, step-back, S26 card. **HTML mockup of the S26 card first** (extend S-2.5's) | `stage-and-world.md` (to write) | Draft |
| 2 | Hydrology Checks | Pure helpers (river-tree and head/mouth lookup, flood-lag and attenuation, basin level series, counter tracking, balance accumulator), the ten tests as `Hard`/`Default` checks with `reopenSection`, baseline record/compare. TDD over hand-built hydrology grids with known answers | `hydrology-checks.md` (to write) | Draft |
| 3 | Headless Runner & Report | `S26HeadlessRunner` on `HydrologyRunner` + the A5 command executor, test plan and run table, worker job with progress and cancel, report JSON, edge-case coverage table, performance reporting, DiaAutomation trigger, live-vs-runner integration test | `headless-runner.md` (to write) | Draft |
| 4 | Scenarios & Walkthrough | S-2.6 scenarios (one per test plus presets), vocabulary file and its GoogleTest, check and overlay registration over the CoW-Shared format | `scenarios-and-walkthrough.md` (to write) | Draft |
| 5 | History & Graphs | DiaCellHistory block for A5 layers and events, `dia.cell_history` export from the card and each run, flood scrub-from-peak-to-onset | `history-and-graphs.md` (to write) | Draft |

**Build order:** 2 can start once A5 Feature 1 exists (it needs only A5's data types and hand-built grids). 3 follows A5 Features 2, 5 and 6. 1 needs A5 Feature 3 and S-1 Features 1 and 8. 4 needs S-2.5 Feature 4 (the shared format and validator). 5 follows 1. **Scripted first (user decision 2026-11-24):** Features 2, 3, 4 and 5 deliver the Scripted verdict with no A4. The Coupled parts (Test 8, the Coupled Test 4 variant, the cycle scrubber, the `SLOW_` Coupled Test 6, Coupled determinism) are tasks inside Features 1, 2 and 3 marked **blocked on A4 Features 1–3 and 8**, and do not gate marking a feature Done.

## Inherited Binding Decisions

| ID | Decision | How it applies |
|----|----------|----------------|
| PD-002 | ProcessingUnit/Phase/Module architecture | `S26RiversFloodsModule` is a SimModule on SimPU; game logic stays on SimPU. It reaches A5, A4 and S-1 through `ModuleRef`. The runner owns its own `HydrologySimulation` |
| PD-004 | No STL containers in public APIs | Check results, reports, series and baselines use `DynamicArrayC` and `String64`/`String256` |
| PD-009 | Non-binary output under `Cluiche/out/<AppName>/` | Reports and history exports go to `out/CoW/s26/` |
| AD-002 | Four-tier dependency model | Stage, checks, runner feature-local. The scenario format is reused from CoW-Shared and S-2.6 adds nothing there |
| AD-003 | GDD owns design intent | Deviations from the GDD doc are listed below for the GDD session |
| AD-005 | One stage per testbed | One stage; modes and presets are panel state |

## Deviations from GDD S-2.6

For the GDD session to fold back into `S2.6_RiversFloods.md` (never edit the GDD in parallel; send exact text to the CoW/GDD session):

1. **Graphs are a DiaCellHistory export drawn in the T-1 Inspector, not an in-game graph panel.** The GDD §2 "Time-series graphs and history scrubber" bullet and §5a imply an in-game graph; this follows S-2.5 Feature 5.
2. **Coupled-only checks are not part of the headless verdict.** Test 8, the Coupled Test 4 variant, the Coupled determinism run and the `SLOW_` Coupled Test 6 run live in the stage or as `SLOW_` GoogleTests (GDD AD-03 allows one Coupled smoke test; this lists exactly which).
3. **Stage module and command names** (`S26RiversFloodsModule`, `cow.s26.*`) are fixed here; the GDD does not name them.
4. **Roster text is stale:** `TestbedRoster.md` S-2.6 row says "spec not yet written (needs its own interview)". The GDD doc itself is locked (2026-11-17).

## Dependencies

**Requires:**
- **A5 Hydrology**, Features 1–3, 5 and 6 (data, pipeline, module, debug, presets and regression with `ScriptedWaterInput`).
- **A4 Weather**, Features 1–3 and 8, for Coupled mode only.
- **S-1 World Generation**, Features 1 and 8, plus amendment 8 (basin data) for real worlds.
- **S-2.5**, Feature 4 (CoW-Shared scenario format, `ScenarioValidator`, `CheckRegistry`) and its two curated worlds.
- F-1, F-2, GameSpeedController, `RandomStreamModule`, DiaAutomation, DiaCellHistory, DiaCellLayers (Done).
- F-3 for `world_bundle` and the launch-out hint.

**Required by:** S-3 Ecology Living mode reading A5 water (A1 §13.4–13.5 constants tuned here), S-3.5 Run mode with hydrology, A3's five A5-driven rules (thresholds are first-run defaults, tuned here).

## Test Strategy

- **Unit (Feature 2):** each helper against hand-built hydrology grids with known answers; every check gets a pass and a fail case; the precheck predicates are tested on grids that satisfy and violate them.
- **Unit (Feature 4):** scenarios load and validate; the vocabulary test fails when a registered check or overlay is missing from `vocabulary.json`.
- **Integration (Features 1, 3):** the runner and a live run on the same world give byte-identical committed fields; cancel stops cleanly; the baseline refuses to overwrite without `force`.
- **Proof:** the full Scripted plan on both curated worlds; first run records the baseline and each default magnitude; results go in the plan.

## Resolved Design Decisions

1. **Feature split mirrors S-2.5, five features** (2026-11-24). Rejected: folding graphs into Stage & World (S-2.5 F5 became an export only, but keeping the slot matches the sibling and keeps the history block separately testable); a sixth Coupled feature.
2. **CoW feature-local placement** (2026-11-24). Rejected: pre-emptive CoW-Shared helpers, with no second consumer yet.
3. **Scripted first, Coupled later** (2026-11-24). Scripted carries every hard verdict check, so S-2.6 is useful before A4 exists.
4. **Verdict runs Scripted** (GDD AD-03), so an A4 change cannot fail an A5 check.
5. **Baseline tolerance ±10% relative** (GDD §6, same as S-2.5).
6. **No new presets** (GDD AD-08, user decision 2026-11-17).

## Open Design Questions

1. **R-Reach run time.** Test 6 plays 5,200 weeks per world and is expected to be the longest default check. Cluiche A5 Feature 6 Open Question 3 decides default vs `SLOW_`; S-2.6 follows that answer (CH-S26-06). Revisit when the first measurement exists.
2. **Test 1 lag on Small.** The GDD's 3–10 week lag may be unreachable on a 66 × 42 map (CH-S26-03). If the first run records `k < 3`, it is a tuning question (`RELEASE_MIN` / `RELEASE_SLOPE`, or an A5 Feature 2 design change) for the user, not an A5 reopen.
3. **Shared session hint.** Whether S-3.5's launch-out lists S-2.6 as a target needs a check against S-3.5 Feature 4 at Feature 1 spec time.

## Status

**Status:** `Draft` (2026-11-24). System spec written from the locked GDD; feature specs not yet written, no plan, no code. Cannot be built until A5 and S-2.5 Feature 4 exist (backlog order #4 → #9 → #11). Approve this spec, then write the five feature specs.
