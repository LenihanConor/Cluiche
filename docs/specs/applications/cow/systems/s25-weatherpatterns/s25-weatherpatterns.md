# System Spec: S-2.5 Weather Patterns

**Parent:** @docs/specs/applications/cow/cow.md  
**Design source:** `Output Docs/GDD/10_PrototypeStrategy/S2.5_WeatherPatterns.md` (DesignDocs repo, Locked 2026-10-18 after a Tier 2 Challenge Pass)  
**System under test:** @docs/specs/applications/cow/systems/a4-weather/a4-weather.md (A4 owns the tools; this testbed hosts them)

**Gameplay Domains:** debug

## Summary

S-2.5 is the testbed for CoW's **weather layer**. It loads an S-1 world and runs A4 alone on it: no ecology, no succession, no dragons. It proves that A4's patterns emerge (coastal rain, rain shadows, moving storms, seasons, spring floods, droughts, year-to-year variation) before A1 depends on them. S-3's Living mode isn't trusted until S-2.5 passes.

```
Dragons / game   S-4+         later
Ecology          S-3          A1, then A3
Weather          S-2.5        ← this testbed (A4)
Terrain          S-1          live terrain editor
```

A4's debug domain, nine overlays (eight scalar fields plus wind; amended 2026-10-19, product review), cell inspector, `cow.weather.*` commands, presets and 13-week history all belong to A4 (A4 Features 5 and 6). S-2.5 adds the stage, the curated worlds, the nine numeric checks, the headless runner and report, the guided walkthrough and the time-series graphs.

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
- **Map sizes (amended 2026-10-19, product review).** Small (66 × 42 = 2,772 cells) is the default and baseline target for acceptance. Standard (80 × 52 = 4,160 cells) and Large (104 × 64 = 6,656 cells) are supported via a size picker (DiaAPI command, see Commands below). Performance budgets scale per size (see Performance below).
- **Player-facing weather visuals.** That's V-1. The overlays are debug-grade.

## Architecture

### Placement

**Feature-local** (AD-002): the stage, the checks, the runner and the graphs live with the testbed, in `Cluiche/CoW/Modules/` (with `S25` prefixes) and `Cluiche/Assets/CoW/Stages/S25WeatherPatterns/`. The **scenario format, loader and check-line panel are CoW-Shared** (`Cluiche/CoW/Shared/Scenarios/`), because S-3 uses them too. S-2.5 builds them first (Feature 4).

```
SimPU (S25WeatherPatterns stage)
  ├─ WorldGenerationModule     loads the stage's world (SD-S1-010: preset + seed)
  ├─ GameSpeedControllerModule, RandomStreamModule
  ├─ WeatherModule             A4, history_weeks: 13, table baseline (no A1), WeatherDebugDomain
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
        float threshold;         // the GDD default or the frozen baseline
        Dia::Core::String256 detail;   // e.g. "ocean 0.83 > coast 0.71 > inland 0.32"
    };
}
```

**Helpers** (pure, shared by the checks):
- **Seasons (amended 2026-10-19, product review):** one table, as GDD AD-08: Spring weeks 1–13, Summer 14–26, Autumn 27–39, Winter 40–52. It's the same calendar A4 reads from GameSpeedController (91-day seasons, 13 weeks per season). An integration test asserts they agree.
- **Distance fields:** hex BFS distance from water-source cells and from the map edge, over Dia's `HexTopology`.
- **Storm tracker:** links `find_storms` centres (pressure < 0.3 clusters) week to week by nearest centre, giving a track per storm.
- **Dry spells:** consecutive dry weeks per S-1 region. Without S-1 Feature 9 there's one region.
- **Aggregates:** global and per-region means of moisture, precipitation and temperature, plus snow coverage %.

**The nine tests** (GDD §5) become checks:

| Test | Hard checks | Default magnitudes |
|------|-------------|--------------------|
| 1 Coastal Rain | mean moisture falls monotonically ocean > coast > inland band; coastal cells have more precipitation weeks than inland | ocean ≥ 0.8, coast ≥ 0.6, inland (≥ 8 from ocean) ≤ 0.4 |
| 2 Rain Shadow | `check_rain_shadow` reports a leeward shadow; windward precipitation > leeward; same shadow cells in two runs of the same seed | — |
| 3 Storm Movement | a spawned storm's centre moves between weeks; wind around the centre points toward it; the storm dissipates or intensifies by moisture | centre moves ≥ 5 cells in 5 weeks |
| 4 Seasonal Variation | global mean temperature peaks in Summer and bottoms in Winter, smoothly; Winter precipitation > Summer; temperature crosses the melt threshold in Spring | Winter precipitation ≥ 1.5× Summer |
| 5 Spring Floods | mountain `snow_pack` rises through Winter and falls through Spring; lowland ground water rises while snow pack falls | lowland ground-water peak lands in Spring (first run records the week) |
| 6 Droughts | `find_droughts` reports at least one region; inland cells are drier than coastal | ≥ 4 consecutive dry weeks in Summer or Autumn |
| 7 Variability | annual global rainfall differs across 4 years; each year keeps the seasonal shape | — |
| 8 Boundary | with `boundary.edge_inflow` on vs off, edge weather differs in the expected direction; interior cells differ by no more than a tolerance | interior tolerance |
| 9 Determinism | committed `WeatherFields` byte-identical at weeks 13, 26, 39, 52 across two runs | — |

"Smoothly" (Test 4) means the 52-week temperature curve has no more than two direction changes, after a 3-week moving mean.

**Baseline.** `Stages/S25WeatherPatterns/baseline.json` holds the frozen default magnitudes, one value per check per world. `cow.s25.baseline.record` writes it from the latest report and refuses to overwrite an existing value without `force`. Once a value is frozen, later runs compare against it (within ±10% relative) rather than the GDD default. A miss is reported as a **magnitude miss**, not a fail. The baseline is committed to the repo, so a change in it shows up in review.

### Headless runner

`S25HeadlessRunner` drives A4's pure `WeatherSimulation` directly, on a worker job, without `SimulationTick`, rendering or the bus. It uses the same initialise → spin-up → step-by-season sequence as `WeatherModule`, through a pure `WeatherRunner` that A4 exposes (see Amendments). So the runner can't drift from the live module. A short live-vs-runner comparison is an integration test.

- **Test plan.** One run per test setup: a 52-week run per world, a 4-year run for Test 7, two determinism runs (Test 9, with different executor range counts, which is stronger than two identical runs), and an `edge_inflow` on/off pair (Test 8). Scenario setup (presets, forcings, wind overrides) is applied through **A4's command executor bound to the runner's own simulation** (see Amendments), so a scenario means the same thing live and headless.
- **Performance (amended 2026-10-19, product review).**
  - **Headless:** a 52-week run, spin-up included, finishes in **≤ 5 s** at Small (GDD "52-week run", see Deviations).
  - **Weekly tick budgets per map size:** Small (2,772 cells) ≤ 4.2 ms, Standard (4,160 cells) ≤ 6.3 ms, Large (6,656 cells) ≤ 10 ms. The baseline/regression run at Small; one additional headless run at Large (no rendering) verifies the Large budget. The runner reports the slowest `Step()` across the 52 weeks from A4's step profile.
  - **Live smoke:** the stage runs 52 weeks at Fastest (about 12.1 s, capped by GameSpeedController) at Small with no weekly tick above 4.2 ms. DiaAutomation triggers it, and A4's step-duration metric supplies the times.
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
  "setup":   ["cow.weather.load_preset rain_shadow"],
  "watch":   "The range blocks the prevailing wind. Rain falls on the windward slope; the leeward side stays dry.",
  "checks":  ["s25.rain_shadow.reported", "s25.rain_shadow.windward_gt_leeward"],
  "walkthrough": [
      { "overlays": ["wind"],          "text": "Find the prevailing wind and the range across it." },
      { "overlays": ["precipitation"], "text": "Compare the two sides.", "check": "s25.rain_shadow.windward_gt_leeward" },
      { "overlays": ["ground_water"],  "text": "The shadow persists in ground water." } ] }
```

- **Setup** is a list of DiaAPI commands, so a scenario does nothing the panel can't (feedback: editor actions are DiaAPI commands).
- **Checks** are names registered by the hosting testbed (S-2.5 registers its `s25.*` checks; S-3 registers its own).
- **Walkthrough** steps switch overlays on (A4's overlay toggles, through commands), show the text, and pin one check line next to the overlay. `cow.scenario.walk next|prev` steps through them.
- The loader validates on load: unknown commands, unknown checks and unknown overlays are rejected with the file and line.

S-2.5 ships nine scenarios (one per test) plus the five presets as guided setups.

### Time-series graphs

`S25WeatherModule` records weekly aggregates (global moisture, precipitation and temperature means, snow coverage %, and the same per S-1 region) into a bounded ring of 4 × 52 weeks. It's tiny (about 5 values × regions × 208 weeks), isn't saved, and resets when the world loads. The panel draws them as line graphs with season bands. Step-back trims the ring to match.

### Click-to-inspect, step-back, boundary toggle

All three are A4's tools wired to the stage:
- **Click-to-inspect:** a click on the CellField view selects the cell, and A4's inspector shows its 9 weather fields (amended 2026-10-19, product review) plus biome and elevation.
- **Step-back:** the panel's button calls `cow.weather.step_back` (history 13 weeks). It's disabled when history is empty.
- **Boundary toggle:** a panel switch calls `cow.weather.set_param boundary.edge_inflow <on|off>`, effective next week. Test 8 runs both settings headless.

## Public Interface

### Stage

`Cluiche/Assets/CoW/Stages/S25WeatherPatterns/s25_weather_patterns.diastage`, launched from F-1's debug menu. It holds the default world (`s25_coast_range` as preset + seed, or may accept a `world_bundle` with terrain only; amended 2026-10-19, F-3 World Assets), the scenario list and the A4 module config (`history_weeks: 13`).

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

`cow.s25.world.load`, `cow.s25.world.size <small|standard|large>` (amended 2026-10-19, product review), `cow.s25.run` (headless plan; `world=`, `seed=`, `size=`), `cow.s25.run.cancel`, `cow.s25.report`, `cow.s25.baseline.record [force]`, `cow.s25.livesmoke`. CoW-Shared: `cow.scenario.list`, `cow.scenario.run`, `cow.scenario.walk next|prev`. The S25 panel card calls the same commands. All weather tools are A4's `cow.weather.*`.

## Features

| # | Feature | Description | Spec | Status |
|---|---------|-------------|------|--------|
| 1 | Stage & World | `.diastage`, F-1 menu entry, `S25WeatherModule`, world load and picker (SD-S1-010), size picker (Small/Standard/Large; amended 2026-10-19, product review), the two curated worlds + prechecks, A4 hosting (history 13, table baseline), time controls, step-back button, click-to-inspect wiring, boundary switch, S25 panel card. HTML mockup of the S25 card + A4 tools first | TBD | Draft |
| 2 | Weather Checks | Pure helpers (season table, distance fields, storm tracker, dry spells, aggregates), the nine tests as named `Hard`/`Default` checks, baseline file record/compare. TDD over hand-built hex terrain with known answers | TBD | Draft |
| 3 | Headless Runner & Report | `S25HeadlessRunner` on A4's `WeatherRunner` + command executor, the test plan, worker job with progress and cancel, report JSON, edge-case coverage table, headless and live-smoke performance, DiaAutomation trigger and readout, live-vs-runner integration test | TBD | Draft |
| 4 | Scenarios & Walkthrough (CoW-Shared) | Scenario format + loader + validator, `CheckRegistry`, check-line panel, walkthrough steps and `cow.scenario.*`; the nine S-2.5 scenarios and the five preset setups. S-3 Feature 3 consumes it | TBD | Draft |
| 5 | Time-Series Graphs | Weekly aggregate ring (global + per region, 4 years), panel line graphs with season bands, step-back trim | TBD | Draft |

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

## Status

**Status:** `Approved` (2026-10-18). Interview done the same day (check runner, test worlds, Test 7 risk, shared scenarios, 52-week budget, baseline tolerance, graphs). Sibling amendments applied to A4 and S-3; GDD deviations sent to the GDD session (`cow-29`). No open system-level questions. No plan yet; create `s25-weatherpatterns.plan.md` when implementation starts (Feature 2 first, once A4 Feature 1 exists).
