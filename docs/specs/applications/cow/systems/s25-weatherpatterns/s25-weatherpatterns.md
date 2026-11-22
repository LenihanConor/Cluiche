# System Spec: S-2.5 Weather Patterns

**Parent:** @docs/specs/applications/cow/cow.md  
**Testbed:** required  
**Design source:** `Output Docs/GDD/10_PrototypeStrategy/S2.5_WeatherPatterns.md` (DesignDocs repo, Locked 2026-10-18 after a Tier 2 Challenge Pass)  
**System under test:** @docs/specs/applications/cow/systems/a4-weather/a4-weather.md (A4 owns the tools; this testbed hosts them)

**Gameplay Domains:** debug

**Amended 2026-11-18 (world-sim review, user decisions):** see "World-sim review amendment" below.

## Summary

S-2.5 is the testbed for CoW's **weather layer**. It loads an S-1 world and runs A4 alone on it: no ecology, no succession, no dragons. It proves that A4's patterns emerge (coastal rain, rain shadows, moving storms, seasons, spring floods, droughts, year-to-year variation) before A1 depends on them. S-3's Living mode isn't trusted until S-2.5 passes.

```
Dragons / game   S-4+         later
Ecology          S-3          A1, then A3
Weather          S-2.5        ← this testbed (A4)
Terrain          S-1          live terrain editor
```

A4's debug domain, seven scalar base layers plus the wind overlay (amended 2026-10-19, consistency pass; was "nine overlays"; product review), cell inspector, `cow.weather.*` commands, presets and 13-week history all belong to A4 (A4 Features 5 and 6). S-2.5 adds the stage, the curated worlds, the nine numeric checks, the headless runner and report, the guided walkthrough and the time-series graphs.

**Checks are the verdict; overlays explain** (GDD AD-07). Each of the GDD's nine tests is a numeric predicate over A4's committed state. The same predicate runs in two places: in a **headless runner** that drives A4's pure simulation directly and produces the pass/fail report through DiaAutomation, and as a **live check line** in the stage while you watch the overlays.

## Goals

1. **Prove A4 in isolation.** The GDD's Tests 1–9 run as numeric checks and produce a pass/fail report. A hard fail names the A4 TechSpec section to reopen.
2. **Understand weather.** Every weather field can be seen on the map, inspected per cell, stepped back through 13 weeks and graphed over the year, on a known world.
3. **Fast, repeatable verdicts.** The headless runner finishes a full test plan in seconds and needs no rendering, so it runs in automation and on every A4 change.
4. **Honest thresholds.** Direction and ordering checks are hard. Magnitudes the GDD doesn't state are recorded on the first run and frozen as a baseline (GDD AD-06). A magnitude miss goes to the user, not straight to an A4 reopen.
5. **Shared guided scenarios.** The walkthrough format is shared with S-3, so both testbeds teach their layer the same way.

## Non-Goals

- **Equation correctness.** A4's GoogleTests prove each pipeline step and the four AuthoringDebug §6 regressions on hand-built terrain. S-2.5 checks emergence on real S-1 worlds.
- **Tuning A4's defaults.** `set_param` exists for understanding. S-2.5 runs on A4's locked TuningParameters, and doesn't write parameters back.
- **`WeatherUpdated` and A1 integration.** No subscriber here. S-3 checks the event (GDD AD-03).
- **Extreme weather and year quality.** A4 defines neither (CHANGE-038).
- **Map sizes** (amended 2026-10-19, product review; amended 2026-10-19, consistency pass). Small (66 × 42 = 2,772 cells) is the default and baseline target for acceptance. Standard (80 × 52 = 4,160 cells) and Large (104 × 64 = 6,656 cells) are supported via a size picker (`cow.worldgen.set_size <Small|Standard|Large>`; S-1 owns world size). Performance budgets scale per size (see Performance below).
- **Player-facing weather visuals.** That's V-1. The overlays are debug-grade.

**Launch-out from S-3.5 (amended 2026-10-19, S-3.5 World Sim):** at stage start, S-2.5 reads F-3's session hint (`out/CoW/session/next_bundle.json`). If one is present, it loads that bundle instead of its configured world, so "Open in S-2.5" from S-3.5 continues with the same world.

## Architecture

### Placement

**Feature-local** (AD-002): the stage, the checks, the runner and the graphs live with the testbed, in `Cluiche/CoW/Modules/` (with `S25` prefixes) and `Cluiche/Assets/CoW/Stages/S25WeatherPatterns/`. The **scenario format, loader and check-line panel are CoW-Shared** (`Cluiche/CoW/Shared/Scenarios/`), because S-3 uses them too. S-2.5 builds them first (Feature 4).

```
SimPU (S25WeatherPatterns stage)
  ├─ WorldGenerationModule     loads the stage's world (SD-S1-010: preset + seed)
  ├─ GameSpeedControllerModule, RandomStreamModule
  ├─ WorldStepModule           order ["weather"], synchronous by default, live Sync/Deferred switch (Feature 1, 2026-11-16)
  ├─ WeatherModule             A4 stage, tracked-layer history block, table baseline (no A1), WeatherDebugDomain
  └─ S25WeatherModule          world picker, live check lines, run controls, graphs, S25 panel card
                               └─ S25HeadlessRunner (worker job; its own WeatherSimulation)
```

No `EcologyModule`, `SuccessionModule` or `SaveLoadModule` in this stage. A4 falls back to `TableBiomeSeasonalBaseline` and reports Degraded health for the baseline. That's expected here, and the panel says so.

### Worlds

The stage ships **curated S-1 worlds as preset + seed** (SD-S1-010). They're picked by hand once S-1 runs, and each test names the world it uses:

| World | Must contain | Used by |
|-------|--------------|---------|
| `s25_coast_range` | an ocean coast, coastal lowlands, a mountain range crossing the prevailing wind, lowlands downstream of the range | Tests 1, 2, 3, 4, 5, 8, 9 |
| `s25_dry_interior` | an inland region whose farthest cells are ≥ 8 cells from any water source | Tests 6, 7 |

**World-shape prechecks.** Each world has a precheck that confirms it still has its features: for example, a cell band at distance ≥ 8 from water, or a windward/leeward cell pair across a ridge above a set elevation. A precheck runs before the tests that use the world. If it fails, the report says **invalid world** (re-pick the seed, since S-1 has changed), not an A4 fail.

The panel's world picker also lists other S-1 presets and `.wld.json` files, for looking around. Checks run on any world, but only the curated worlds give a verdict.

### Checks

A check is a named C++ predicate over A4's committed state and the world. Checks are pure (no module access) and live in `S25WeatherChecks`:

```cpp
namespace CoW::S25 {
    enum class CheckKind : uint8_t { Hard, Default };   // GDD AD-06

    struct CheckResult {
        Dia::Core::StringCRC id;
        CheckKind kind;
        bool passed;
        float measured;          // the number the check computed (NaN if not a magnitude)
        // Amended 2026-11-16 (Feature 2, weather-checks.md): `bool passed` is replaced by a CheckState
        // (Pass, Fail, MagnitudeMiss, Pending, InvalidWorld) and a reopenSection string.
        float threshold;         // the GDD default or the frozen baseline
        Dia::Core::String256 detail;   // e.g. "ocean 0.83 > coast 0.71 > inland 0.32"
    };
}
```

**Helpers** (pure, shared by the checks):
- **Seasons (amended 2026-10-19, product review):** one table, as GDD AD-08: Spring weeks 1–13, Summer 14–26, Autumn 27–39, Winter 40–52. It's the same calendar A4 reads from GameSpeedController (91-day seasons, 13 weeks per season). An integration test asserts they agree.
- **Distance fields:** hex BFS distance from water-source cells and from the map edge, over Dia's `HexTopology`.
- **Storm tracker:** links A4's `StormDetector` centres (pressure < 0.3 clusters) week to week by nearest centre, giving a track per storm.
- **Dry spells:** consecutive dry weeks per S-1 region. Without S-1 Feature 9 there's one region.
- **Aggregates:** global and per-region means of moisture, precipitation and temperature, plus snow coverage %.

**The nine tests** (GDD §5) become about 34 named checks, defined exactly in `weather-checks.md` (Feature 2; that spec is authoritative, including Test 3 timing and Test 7 thresholds). Summary:

| Test | Hard checks | Default magnitudes |
|------|-------------|--------------------|
| 1 Coastal Rain | mean moisture falls monotonically ocean > coast > inland band; coastal cells have more precipitation weeks than inland | ocean ≥ 0.8, coast ≥ 0.6, inland (≥ 8 from ocean) ≤ 0.4 |
| 2 Rain Shadow | `check_rain_shadow` reports a leeward shadow; windward precipitation > leeward; same shadow cells in two runs of the same seed | — |
| 3 Storm Movement | a spawned storm's centre moves between weeks; wind around the centre points toward it; the storm dissipates or intensifies by moisture | centre moves ≥ 5 cells in 5 weeks |
| 4 Seasonal Variation | global mean temperature peaks in Summer and bottoms in Winter, smoothly; Winter precipitation > Summer; temperature crosses the melt threshold in Spring | Winter precipitation ≥ 1.5× Summer |
| 5 Spring Floods | mountain `snow_pack` rises through Winter and falls through Spring; mountain `waterToGround` (weekly rain + melt reaching the ground) pulses as the snow pack melts, peaking in Spring (aligned to locked GDD 2026-11-17, CHANGE-048: the downstream lowland flood moved to S-2.6) | lowland `waterToGround` peak lands in Spring (first run records the week) |
| 6 Droughts | `DroughtDetector` reports at least one region; inland cells are drier than coastal | ≥ 4 consecutive dry weeks in Summer or Autumn |
| 7 Variability | annual global rainfall differs across 4 years; each year keeps the seasonal shape | — |
| 8 Boundary | with `boundary.edge_inflow` on vs off, edge weather differs in the expected direction; interior cells differ by no more than a tolerance | interior tolerance |
| 9 Determinism | committed `WeatherFields` byte-identical at weeks 13, 26, 39, 52 across two runs | — |

"Smoothly" (Test 4) means the 52-week temperature curve has no more than two direction changes, after a 3-week moving mean.

**Baseline.** `Stages/S25WeatherPatterns/baseline.json` holds the frozen default magnitudes, one value per check per world. `cow.s25.baseline.record` writes it from the latest report and refuses to overwrite an existing value without `force`. Once a value is frozen, later runs compare against it (within ±10% relative) rather than the GDD default. **Re-record needed (aligned to locked GDD 2026-11-17, CHANGE-048):** A4's evaporation now reads A5's `open_water` and `surface_wetness`. With A5 absent, as in S-2.5, `open_water` falls back to the static `water_source` flag and `surface_wetness` is 0, so the old A4 magnitudes no longer match. A5 also lowers `SOIL_WETNESS_WEIGHT` to 0.25 so A4's land evaporation stays near its S-2.5 tuning, and A4 gained influences and climate cycles. Any baseline recorded before this change is stale. Re-record with `cow.s25.baseline.record force` under the new A4. With all climate cycles off, Tests 1-9 match the re-recorded baselines. A miss is reported as a **magnitude miss**, not a fail. The baseline is committed to the repo, so a change in it shows up in review.

### Headless runner

`S25HeadlessRunner` drives A4's pure `WeatherSimulation` directly, on a worker job, without `SimulationTick`, rendering or the bus. It uses the same initialise → spin-up → step-by-season sequence as `WeatherModule`, through a pure `WeatherRunner` that A4 exposes (see Amendments). So the runner can't drift from the live module. A short live-vs-runner comparison is an integration test.

- **Test plan (superseded by `headless-runner.md`'s ten-run table, 2026-11-16).** Originally: one run per test setup: a 52-week run per world, a 4-year run for Test 7, two determinism runs (Test 9, with different executor range counts, which is stronger than two identical runs), and an `edge_inflow` on/off pair (Test 8). Scenario setup (presets, forcings, wind overrides) is applied through **A4's command executor bound to the runner's own simulation** (see Amendments), so a scenario means the same thing live and headless.
- **Performance (amended 2026-11-16, Feature 3).** Report only, never a verdict input (deviation D-3.1). Headless: a 52-week run, spin-up included, at Small is compared with 5 s; step CPU with A4's gate (Small 4.2, Standard 6.3, Large 10 ms). The two exclusive performance runs (P1 Small, P2 Large) run last. The live smoke at Fastest is informational (`cow.s25.livesmoke`). Details and the full run table: `headless-runner.md`.
- **Report.** `out/CoW/s25/report_<world>_<seed>.json` (PD-009) has one `CheckResult` per check, the world precheck results, the performance numbers, and the edge-case coverage table. A hard fail carries the A4 TechSpec section named in the GDD test.
- **Edge cases.** `Stages/S25WeatherPatterns/edge_cases.json` maps each of A4's 20 edge cases (`A4_EdgeCaseMatrix.md`) either to the scenario that exercises it or to `untested` with a reason. The report lists both, so nothing is silently skipped.
- **Automation.** DiaAutomation triggers `cow.s25.run` and reads the result, following F-2's pattern: the module runs the plan, and DiaAutomation is the trigger and readout.
- It shows progress in the panel and can be cancelled.

### Live check lines

In the stage, `S25WeatherModule` evaluates the active scenario's checks after each committed week (on `WeatherUpdated`, from A4's snapshot) and shows them as pass/fail lines in the panel. Live checks that need a whole year (Tests 4, 5, 7) show "pending" until enough weeks have run. Live lines help you understand a result. The report is the verdict.

### Scenarios and walkthrough (CoW-Shared)

The scenario format S-3 designed becomes CoW-Shared, with one addition: walkthrough steps. Scenario files use envelope `"format": "cow.scenario", "version": 1` and extension `.scenario.json` (amended 2026-10-19, F-3 World Assets).

```json
{ "format": "cow.scenario", "version": 1,
  "id": "s25_rain_shadow", "world": "s25_coast_range",
  "setup":   ["cow.weather.load_preset name=rain_shadow"],
  "watch":   "The range blocks the prevailing wind. Rain falls on the windward slope; the leeward side stays dry.",
  "checks":  ["s25.t2.shadow_present", "s25.t2.windward_gt_leeward"],
  "walkthrough": [
      { "overlays": ["wind"],          "text": "Find the prevailing wind and the range across it." },
      { "overlays": ["precipitation"], "text": "Compare the two sides.", "check": "s25.t2.windward_gt_leeward" },
      { "overlays": ["water_to_ground"], "text": "The shadow persists in the weekly water reaching the ground." } ] }
```

- **Setup** is a list of DiaAPI commands, so a scenario does nothing the panel can't (feedback: editor actions are DiaAPI commands).
- **Checks** are names registered by the hosting testbed (S-2.5 registers its `s25.*` checks; S-3 registers its own). **Validation and vocabulary (amended 2026-10-20, T-2 Feature 5):** the loader validates a scenario with the shared `ScenarioValidator` (`CoW/Shared/WorldAssets/Core/`, the same one the T-2 scenario editor calls) against the live command registry and the registered checks and overlays. The testbed keeps `Stages/<Stage>/scenarios/vocabulary.json` (format `cow.scenario_vocabulary`: modes, checks and overlays with ids, labels and descriptions) and a GoogleTest that fails when a registered check or overlay is missing from it, or it names one that is not registered. **Ownership (decided 2026-11-16): S-2.5 Feature 4 builds `ScenarioValidator`** (its loader is the first consumer); the rule table is the contract in T-2 `scenario-editor.md`. S-3 and T-2 Feature 5 consume it; neither builds it.
- **Walkthrough** steps switch overlays on (A4's overlay toggles, through commands), show the text, and pin one check line next to the overlay. `cow.scenario.walk next|prev` steps through them.
- The loader validates on load: unknown commands, unknown checks and unknown overlays are rejected with the file and line.

S-2.5 ships nine scenarios (one per test) plus the five presets as guided setups (amended 2026-11-16, Feature 4: `scenarios-and-walkthrough.md` is authoritative; live check lines read a 65-week full-precision ring, two-run checks show `HeadlessOnly`, and the three `cow.scenario.*` names are `list`, `run` and `walk`).

### Time-series graphs

**Amended 2026-11-16 (Feature 5, `time-series-graphs.md`):** there is **no in-game graph panel** and no S-2.5 aggregate ring. The series are DiaCellHistory aggregates (every weather layer, `world` plus S-1 region groups, whole run), exported from the S25 card or by each headless run as `dia.cell_history`, and drawn in the Inspector (T-1 Feature 7, proposed). Resolved Decision 7 (graph panel starts feature-local) is moot.

### Click-to-inspect, step-back, boundary toggle

All three are A4's tools wired to the stage:
- **Click-to-inspect:** a click on the CellField view selects the cell, and A4's inspector shows its 9 weather fields (amended 2026-10-19, product review) plus biome and elevation.
- **Step-back (amended 2026-10-19, DiaCellLayers/DiaCellHistory):** the panel's button calls DiaCellHistory scrub (`cells.history.view`) and restore (`cells.snapshot.restore`). History covers 13 weeks by default. `cow.weather.step_back` no longer exists (deleted from A4 2026-11-13).
- **Boundary toggle:** a panel switch calls `cow.weather.params {"action":"set","key":"boundary.edge_inflow","value":<0|1>}` (A4 Feature 5 consolidated its commands to 7 names), effective next week. Test 8 runs both settings headless.

## Public Interface

### Stage

`Cluiche/Assets/CoW/Stages/S25WeatherPatterns/s25_weather_patterns.diastage`, launched from F-1's debug menu. It holds the default world (`s25_coast_range` as preset + seed, or may accept a `world_bundle` with terrain only; amended 2026-10-19, F-3 World Assets), the scenario list and the A4 module config (parameters file, `spinup_weeks`, tracked-layer `history` block; `history_weeks` was removed from A4, 2026-11-13). Feature 1 details: `stage-and-world.md`.

### Module

```cpp
namespace CoW::S25 {
    class S25WeatherChecks {                         // pure; one function per named check
    public:
        static void RegisterAll(CoW::Scenarios::CheckRegistry& registry);
    };

    struct S25RunParams  { Dia::Core::StringCRC world; uint32_t seed; bool recordBaseline; };
    struct S25RunReport  { DynamicArrayC<CheckResult> checks; /* prechecks, perf, edge-case coverage */ };

    class S25HeadlessRunner {
    public:
        S25RunHandle Start(const S25RunParams& params);   // worker job; progress + cancel
    };

    class S25WeatherModule : public Dia::ApplicationFlow::SimModule {
    public:
        void LoadWorld(Dia::Core::StringCRC world);
        void RunScenario(Dia::Core::StringCRC id);
        const DynamicArrayC<CheckResult>& GetLiveChecks() const;
        S25RunHandle StartRun(const S25RunParams& params);
    };
}

namespace CoW::Scenarios {                           // CoW-Shared, also used by S-3
    struct Scenario { /* id, world, setup commands, watch, checks, walkthrough steps */ };
    bool LoadScenario(const Json::Value& root, const CheckRegistry& checks, Scenario& out, ScenarioError& error);
    class CheckRegistry { /* name → predicate; registered by the hosting testbed */ };
}
```

### Commands (DiaAPI)

`cow.s25.world.load`, `cow.worldgen.set_size <Small|Standard|Large>` (amended 2026-10-19, consistency pass; S-1 owns world size), `cow.s25.run` (headless plan; `world=`, `seed=`, `size=`), `cow.s25.run.cancel`, `cow.s25.report`, `cow.s25.baseline.record [force]`, `cow.s25.livesmoke`. CoW-Shared: `cow.scenario.list`, `cow.scenario.run`, `cow.scenario.walk next|prev`. The S25 panel card calls the same commands. All weather tools are A4's `cow.weather.*`.

## Features

| # | Feature | Description | Spec | Status |
|---|---------|-------------|------|--------|
| 1 | Stage & World | `.diastage`, F-1 menu entry, `S25WeatherModule`, world load and picker (SD-S1-010), size picker via `cow.worldgen.set_size` (Small/Standard/Large; amended 2026-10-19, product review; amended 2026-10-19, consistency pass), the two curated worlds + prechecks, A4 hosting (history 13, table baseline), time controls, step-back button, click-to-inspect wiring, boundary switch, S25 panel card. HTML mockup of the S25 card + A4 tools first | @docs/specs/applications/cow/systems/s25-weatherpatterns/stage-and-world.md | Approved |
| 2 | Weather Checks | Pure helpers (season table, distance fields, storm tracker, dry spells, aggregates), the nine tests as named `Hard`/`Default` checks, baseline file record/compare. TDD over hand-built hex terrain with known answers | @docs/specs/applications/cow/systems/s25-weatherpatterns/weather-checks.md | Approved |
| 3 | Headless Runner & Report | `S25HeadlessRunner` on A4's `WeatherRunner` + command executor, the test plan, worker job with progress and cancel, report JSON, edge-case coverage table, headless and live-smoke performance, DiaAutomation trigger and readout, live-vs-runner integration test | @docs/specs/applications/cow/systems/s25-weatherpatterns/headless-runner.md | Approved |
| 4 | Scenarios & Walkthrough (CoW-Shared) | Scenario format + loader + validator, `CheckRegistry`, check-line panel, walkthrough steps and `cow.scenario.*`; the nine S-2.5 scenarios and the five preset setups. S-3 Feature 3 consumes it | @docs/specs/applications/cow/systems/s25-weatherpatterns/scenarios-and-walkthrough.md | Approved |
| 5 | Time-Series Graphs | Weekly aggregate ring (global + per region, 4 years), panel line graphs with season bands, step-back trim | @docs/specs/applications/cow/systems/s25-weatherpatterns/time-series-graphs.md | Approved |

**Build order:** 2 can start once A4 Feature 1 exists (it needs only `WeatherFields` and hand-built terrain). 3 follows A4 Features 2, 5 and 6 plus the A4 amendments. 1 needs A4 Feature 3 and S-1 Features 1 and 8. 4 and 5 follow 1. **First full verdict:** 2 + 3 on the curated worlds, which needs no rendering.

## Inherited Binding Decisions

| ID | Decision | How it applies |
|----|----------|----------------|
| PD-002 | ProcessingUnit/Phase/Module architecture | `S25WeatherModule` is a SimModule on SimPU, and game logic stays on SimPU. It reaches A4 and S-1 through `ModuleRef`. The runner owns its own `WeatherSimulation`, not the module's |
| PD-004 | No STL containers in public APIs | Check results, reports and aggregates use `DynamicArrayC` |
| PD-009 | Non-binary output under `Cluiche/out/<AppName>/` | Reports go to `out/CoW/s25/` |
| AD-002 | Four-tier dependency model | Stage, checks, runner and graphs are feature-local. The scenario format is CoW-Shared, since S-3 is a second consumer |
| AD-003 | GDD owns design intent | Deviations from the GDD doc are listed below for the GDD session |
| AD-005 | One stage per testbed | One stage. Worlds and scenarios are chosen from its panel |

## Amendments to Sibling Specs (applied 2026-10-18)

**A4 Weather** (CoW-owned, no AD-001 sign-off needed):
1. **Pure `WeatherRunner`** in `CoW/Shared/Weather/`: initialise from a `WeatherTerrain`, spin up, then step N weeks with the season from the week. `WeatherModule` uses it, so the S-2.5 runner can't drift from the module.
2. **The command executor binds to a `WeatherSimulation&`,** not to `WeatherModule`. The module binds it to its own simulation, and the runner binds a second instance to its own. This follows the typed command model rule (UI and scripts call the same model).
3. **Preset worlds:** A4 Feature 6's "fixed preset world" is S-2.5's `s25_coast_range` (preset + seed). A4's regression GoogleTests stay on hand-built terrain.
4. Update the Testbed host section: S-2.5 is specced, the walkthrough is numeric-backed (not visual), and the deviation-7 `year_quality` note is closed by CHANGE-038/042.

**S-3 Ecology:** Feature 3 uses the CoW-Shared scenario format from S-2.5 Feature 4 rather than defining its own.

## Deviations from GDD S-2.5

For the GDD session (`cow-29`) to fold back into `S2.5_WeatherPatterns.md`:

1. **The 52-week ≤ 5 s budget is measured headless at Small (amended 2026-10-19, product review).** GameSpeedController caps Fastest at one day per sim frame (30 days/s), so a live 52-week run takes at least 12.1 s. The live run at Fastest becomes a smoke test: about 12.1 s at Small, with no weekly tick above 4.2 ms.
2. **Checks run headless for the verdict** and live as check lines, using the same predicates. This makes AD-07 concrete.
3. **Test worlds are curated S-1 preset + seed worlds with shape prechecks.** A precheck failure means "invalid world", not an A4 fail. May also accept a `world_bundle` with terrain only (amended 2026-10-19, F-3 World Assets).
4. **Command names follow the A4 spec (amended 2026-10-19, product review):** `cow.weather.reload` (the GDD says `reload_parameters`), plus A4's `load_preset`, `step_back` and `profile`.
5. **"Smooth curve" (Test 4) is defined:** at most two direction changes in the 52-week temperature curve after a 3-week moving mean.
6. **Test 9 uses two different executor range counts** for its two runs, which also proves A4's parallel bit-identity on a real world.
7. **The walkthrough format is shared with S-3** (CoW-Shared scenarios). Scenario files use envelope `"format": "cow.scenario", "version": 1` and extension `.scenario.json` (amended 2026-10-19, F-3 World Assets).

## Dependencies

**Requires:**
- **A4 Weather**, Features 1–3, 5 and 6, plus the amendments above.
- **S-1 World Generation**, Features 1 (Terrain) and 8 (Biome Classification) for the worlds. Feature 9 (regions) is soft: without it, droughts and per-region graphs use one region.
- F-1 (debug menu, debug host), GameSpeedController, `RandomStreamModule`, DiaAutomation. All Done.
- Dia CellField (dia-needs #14), through A4's overlays and for click-to-inspect.

**Required by:** S-3 Ecology's Living mode, whose results aren't trusted until S-2.5 passes.

## Test Strategy

- **Unit (Feature 2):** each helper against hand-built hex terrain with known answers (BFS distances, a hand-moved storm track, a scripted dry spell). Every check gets a pass case and a fail case. The season table agrees with GameSpeedController's calendar.
- **Unit (Feature 4):** scenario files load and validate. Bad commands, unknown checks and unknown overlays are rejected with the file and line.
- **Integration (Features 1, 3):** the runner and a live 13-week run on the same world give byte-identical committed fields. Cancel stops cleanly. The baseline refuses to overwrite without `force`.
- **Proof:** the full plan on both curated worlds. First run records the baseline. The live smoke at Fastest. Results go in the plan when they run.

## Resolved Design Decisions

1. **Checks run headless for the verdict and live for understanding** (2026-10-18).
   - Rejected: live stage only. Every check costs real frames, and a year is at least 12.1 s.
   - Rejected: GoogleTests only. That drifts from the GDD's DiaAutomation rule (AD-07).
2. **Test worlds are curated S-1 preset + seed** (2026-10-18).
   - Rejected: exported `.wld.json` files. They go stale against S-1's format.
   - Rejected: any world with adaptive checks. Harder to write, and "no range found" is ambiguous.
3. **Test 7 stays hard, and a fail goes to the user** (2026-10-18). A4 has no RNG, so after spin-up its year may settle into a repeating cycle and Test 7 would fail. If it does, that's an A4 finding for the user and the GDD session (add a forcing, accept periodic weather, or drop the test). S-2.5 doesn't invent variability.
4. **The scenario format is promoted to CoW-Shared** (2026-10-18). S-3 is a second consumer, which is the AD-002 trigger.
   - Rejected: separate formats per testbed, which would drift.
5. **The 52-week budget is measured headless, with a live smoke at Fastest** (2026-10-18).
   - Rejected: a live budget raised to about 13 s. That mostly measures the speed cap, not A4.
   - Rejected: a faster stage-only speed tier. It breaks GameSpeedController's one-day-per-frame rule.

6. **Baseline tolerance is ±10% relative** (2026-10-18). A4 is deterministic, so a frozen value reproduces exactly until A4's code or parameters change, and then every value moves a little. Frozen values compare within ±10%. After an intended A4 change, re-record deliberately with `cow.s25.baseline.record force`.
7. **The graph panel starts feature-local** (2026-10-18). A1 Feature 9 has its own rolling graph panel, recording per band and per cell, while S-2.5 records weekly per region. Whether to promote one CoW-Shared time-series widget is decided when A1 Feature 9 is specced.

## Open Design Questions

None at system level. Feature-level questions go in each feature spec.

## Gap-pass amendments (2026-11-16)

Found by the plan-writing pass; each item below **supersedes** any contradicting text above. Numbering is stable.

1. Command names are Feature 3's (`cow.s25.run` with an `action`, `cow.s25.baseline`); the dotted forms `cow.s25.run.cancel` and `cow.s25.baseline.record` are superseded.
2. `CheckResult.measured` and `threshold` are `double`, and the result has a `state` and `reopenSection` (Feature 2).
3. Mockups live in `s25-weatherpatterns/mockups/`.
4. The check-engine inputs, `WorldInfo` and the headless-only flag are defined in Feature 2's amendments.

## World-sim review amendment (2026-11-18)

User decisions from the world-sim review. Each item **supersedes** any contradicting text in this spec. Sources: [summary](../../../../../research/worldsim_data_driven/summary.md) §0, §0.1; [parameter-files](../../../dia/systems/diaattribute/parameter-files.md); [biome-registry](../s1-worldgeneration/biome-registry.md); [worldsim-kit](../worldstep/worldsim-kit.md); [grid-search-helpers](../../../dia/systems/diapathfinding/grid-search-helpers.md); [blackboard-scopes](../../../dia/systems/diablackboard/blackboard-scopes.md).

Applies A4's system amendment ([a4-weather](../a4-weather/a4-weather.md), World-sim review amendment) to this spec's A4 references.
1. Command names: `cow.weather.reload` is `cow.weather.params {action: reload}`; the boundary switch and every parameter edit use `cow.weather.params` (`set` reply `{queued, key, value, effectiveDay, visibleDay}`). Supersedes the Binding Decision listing `cow.weather.reload`.
2. Health: "Unhealthy" reads Failing.
3. Schema keys for A4 layers are `cow.weather.layers/<field>`; layer ids unchanged.
4. Stage id is `weather`; World Step order `["weather", "hydro", "ecology"]`.

## Status

**Status:** `Approved` (2026-10-18). **Aligned to locked GDD 2026-11-17 (CHANGE-048):** the GDD S-2.5 re-locked with A5 (A5 owns ground water; Test 5 split with S-2.6, now in the GDD roster as testbed 17). **Every first-run baseline must be re-recorded once, deliberately, under the new A4** (see Baseline and Open Design Questions below). **Amended 2026-11-17 (A5 Hydrology):** A4 no longer has `groundWater`; it outputs `waterToGround`, and ground water moves to A5 (`hydro.ground`). Test 5's measure becomes mountain `waterToGround`; the true flood moves to S-2.6 Rivers & Floods (A4 + A5, own spec, not written yet). S-2.5 stays A4-only (World Step `["weather"]`). **Open (aligned to locked GDD 2026-11-17, CHANGE-048):** the GDD S-2.5 Test 5 now measures the **mountain** `waterToGround` melt pulse, matching the GDD (resolved 2026-11-17). 

**Amended 2026-11-18 (world-sim review).** Status stays `Approved`. Amended 2026-11-18 (world-sim review decisions; user-approved designs in the specs referenced below).

## Backlog notes (moved 2026-11-22)

Moved verbatim from the BACKLOG.md row "**#9** S-2.5 Weather Patterns" so the backlog row holds only what is open. Links below are relative to `docs/`.

**Deliverable cell:** **Re-planned 2026-11-18 (world-sim review):** the headless-runner, stage-and-world and scenarios plans are rebuilt (`cow.weather.params`, Failing health, the sidecar path, `CellSelectorSet`). **Spec Approved 2026-10-18; no plan or code yet:** [s25-weatherpatterns.md](specs/applications/cow/systems/s25-weatherpatterns/s25-weatherpatterns.md). The weather layer's tool: A4 alone on two curated S-1 worlds (`s25_coast_range`, `s25_dry_interior`, preset + seed with shape prechecks). The GDD's 9 tests are numeric Hard/Default checks, run headless for the verdict (report JSON via DiaAutomation) and live as check lines; default magnitudes freeze into `baseline.json` (±10%). Hosts A4's overlays, inspector, `cow.weather.*`, presets, 13-week step-back and the `edge_inflow` toggle. 5 features: (1) Stage & World, (2) Weather Checks, (3) Headless Runner & Report, (4) Scenarios & Walkthrough (CoW-Shared, S-3 reuses it), (5) Time-Series Graphs. Perf: weekly tick ≤ 16 ms on Small; 52 weeks ≤ 5 s headless (live Fastest is ≥ 12.1 s by the speed cap, so it's a smoke test). **Feature 2 can start once A4 Feature 1 exists.** **Updated 2026-10-20 (world-sim review):** three sizes via `cow.worldgen.set_size` (Small default, one Large headless run); hosts the shared DiaCellLayers picker + timeline (hotkeys from the layer schema, Snow = K); step-back and aggregate rings move to DiaCellHistory (A4 Feature 5 removes the interim `WeatherHistory`); scenarios become `.scenario.json` with the `cow.scenario` envelope; `S25` log channel; honours the S-3.5 launch-out session hint. Amended A4 (pure `WeatherRunner`, command executor bound to a simulation, preset world) and S-3 Feature 3. 7 GDD deviations folded back and re-locked 2026-10-18 (CHANGE-043)

**Spec / plan cell:** `S2.5_WeatherPatterns.md` Locked 2026-10-18

**Depends On cell:** A4 Features 1–3, 5, 6 + amendments; S-1 Features 1 + 8 (9 soft); CellField (#14); F-1, GameSpeedController, DiaAutomation (Done) **World source (2026-11-12):** gets its world through the S-1 hand-off (SD-S1-010): set `preset` (+ `seed`, `size`) or `world_file` in the stage's `WorldGenerationModule` config, then use `IsReady(1)` or `WorldGenerationComplete`; `world_bundle` waits for F-3.
