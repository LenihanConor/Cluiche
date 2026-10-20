# System Spec: S-3 Ecology

**Parent:** @docs/specs/applications/cow/cow.md  
**Design source:** `Output Docs/GDD/10_PrototypeStrategy/S2_SingleCellEcology.md` and `S3_LivingEcology.md` (DesignDocs repo, both Locked 2026-10-11). **Merged here 2026-10-18** (see Deviations).  
**System under test:** @docs/specs/applications/cow/systems/a1-ecology/a1-ecology.md (A1 owns the tools; this testbed hosts them)

**Gameplay Domains:** debug

## Summary

S-3 Ecology is the testbed for CoW's **ecology layer**. The world is built in layers, and each testbed is the tool for understanding and tuning its own layer, with the layers below it as inputs:

```
Dragons / game   S-4+         later
Ecology          S-3          ← this testbed (A1, then A3)
Weather          S-2.5        A4
Terrain          S-1          live terrain editor
```

**Map sizes** (amended 2026-10-19, product review): Small 66×42 (2,772 cells, default), Standard 80×52 (4,160 cells), Large 104×64 (6,656 cells). S-3 provides a size picker (DiaAPI command: `cow.s3.map.size`) and keeps long-run default Small with Large opt-in.

S-3 loads an S-1 world and runs A1 on it in one of two modes, chosen from a panel inside the stage (AD-005):

- **Controlled:** the weather is pinned to a climate you set, and A3 is optional. This mode is for understanding and tuning: inspect why values changed, edit cells, tune constants live and save them, isolate a cell. The GDD's S-2 tests run here as guided scenarios.
- **Living:** A4 weather is live, and A3 joins when it exists. This mode is for emergence and long-run stability. The GDD's S-3 tests run here, as guided scenarios plus a headless long-run report.

The inspector, change ledger, overlays, graphs, tuning, painting and `isolate_cell` all belong to A1 (A1 Feature 9). S-3 adds the stage, the modes, the pinned climate, the scenarios and the long-run runner.

## Goals

1. **Understand ecology.** Every ecology value can be seen on the map, inspected per cell, and explained by cause through A1's change ledger.
2. **Tune ecology.** Constants and species data are changed live and saved back to their source JSON, against a known, repeatable climate.
3. **Control the inputs.** Choose the world (any S-1 world) and the weather source (pinned climate or live A4). Switch A3 on or off once it exists.
4. **Prove the GDD tests.** The old S-2 tests run as guided scenarios with live pass/fail lines. S-3's measurable long-run checks produce a headless report.
5. **Grow layer by layer.** Controlled mode ships first and needs neither A4 nor A3. Living mode works with A4 alone, and A3's parts switch on when A3 is built.

## Non-Goals

- **Equation correctness.** A1's GoogleTests prove the equations (A1 Test Strategy). The scenarios here show the behaviour, not the maths.
- **Shaping terrain.** That's S-1's job. S-3 loads worlds and never edits terrain. Painting a biome is an ecology edit, done through A1.
- **Proving weather.** That's S-2.5's job, and S-2.5 must pass before Living-mode results count.
- **Dragons.** No step 9 input. S-4 adds dragons to this layer later.
- **Replay from file.** Still an unbuilt F-1 Dia need. The long-run runner covers S-3's determinism check without it.
- **Real tile art (amended 2026-10-19, DiaCellLayers/DiaCellHistory).** S-1's terrain layers (Relief, Biome, Region …) are registered when the world loads, so they are available as debug base layers in the DiaCellLayers picker. Real tile art is still V-1's job, and every layer stays debug-grade.

**Launch-out from S-3.5 (amended 2026-10-19, S-3.5 World Sim):** at stage start, S-3 reads F-3's session hint (`out/CoW/session/next_bundle.json`). If one is present, it loads that bundle instead of its configured world, so "Open in S-3" from S-3.5 continues with the same world.

## Architecture

### Placement

**Feature-local** (AD-002): the stage, modes, climate inputs, scenarios and runner live with the testbed, in `Cluiche/CoW/Modules/` (with `S3` prefixes) and `Cluiche/Assets/CoW/Stages/S3Ecology/`. If S-4 needs the pinned climate, it's promoted to CoW-Shared then.

```
SimPU (S3Ecology stage; amended 2026-10-19, DiaCellLayers/DiaCellHistory)
  ├─ WorldGenerationModule  loads the stage's world (SD-S1-010: .wld.json or preset + seed)
  ├─ GameSpeedControllerModule, RandomStreamModule, SaveLoadModule
  ├─ EcologyModule          A1, with EcologyDebugDomain (all tools)
  ├─ WeatherModule          A4, Living mode only
  ├─ (SuccessionModule)     A3, when it exists
  ├─ SuccessionDebugDomain  A3's debug tools (hosted by S-3; amended 2026-10-19, product review)
  └─ S3EcologyModule        mode switch, climate input, scenarios, long-run runner, S3 panel card, size picker
                             hosts viewer and timeline cards; default tracked layers in .diastage:
                             vegetation, herbivores, precipitation
```

### Modes

| | Controlled | Living |
|---|---|---|
| Weather input | `S3ClimateInput` (pinned) | A4 through `WeatherSnapshotInput` |
| A1 steps on | `SimulationTick` `band >= Season` (stub mode) | the frame after week 13 (A1 C5) |
| A3 | off by default, switchable once it exists | on once it exists |
| Purpose | understand, tune, guided S-2 tests | emergence, guided S-3 tests, long-run report |

Switching mode reinitialises the ecology from the world: weather and populations restart, and tuned constants are kept. A1's `cow.ecology.*` tools work in both modes.

The panel shows which layers are present: World (preset and seed, or file), Weather (pinned or A4), A3 (present or absent). If A4 isn't built, Living mode is disabled and the panel says why. If A3 is absent, Living mode runs without it: triggers are counted, and no biome changes except through `cow.ecology.apply_transition`.

### Pinned climate (Controlled mode)

`S3ClimateInput` implements A1's `IEcologyWeatherInput`. It is deterministic and has no RNG.

- **Base:** each cell's weekly temperature is A1's biome seasonal baseline (`IBiomeSeasonalBaseline`) for the current season. Its weekly rainfall is `biome.rainfall_baseline[season] / 13`, a `biomes.json` field that A1 Feature 1 adds.
- **Global sliders:** a rain multiplier, a temperature offset, and a year scale (GOOD 1.3 / AVERAGE 1.0 / BAD 0.7). These values are defined here and referenced by A1 (amended 2026-10-19, product review).
- **Region overrides:** the same three values per S-1 region, painted from the panel, for gradients such as a dry interior or a wet coast. These need S-1 Feature 9; without it there's one region.
- **Snow:** `SnowCover` is true while the temperature is below `snow_threshold`.

**Climatology (added once A4 is built):** `S3ClimatologyInput` runs A4's `WeatherSimulation` for a chosen number of years over the loaded world, headless. It stores per-cell weekly means for each week of the year (52 × cells × 3 values) and replays them. Pinned rainfall then has realistic coasts, rain shadows and snow melt, but the same weather every year. The global and region sliders scale it in the same way. It's cached per world and A4 parameter hash.

### Guided scenarios

**Format** (amended 2026-10-19, product review): S-3 Feature 3 uses the CoW-Shared scenario format from S-2.5 Feature 4. A scenario is a data file in `Stages/S3Ecology/scenarios/` using envelope `"format": "cow.scenario", "version": 1` and extension `.scenario.json` (amended 2026-10-19, F-3 World Assets):

```json
{ "format": "cow.scenario", "version": 1,
  "id": "s2_breeding_seasons", "mode": "Controlled", "world": "sandbox_small_temperate",
  "setup":  ["cow.s3.climate.set rain=1.0 year=GOOD", "cow.ecology.set_species cell=412 species=deer adult=0.5",
             "cow.ecology.set_species cell=412 species=elk adult=0.5", "cow.ecology.isolate_cell cell=412 on"],
  "watch":  "Step one season at a time. Deer breed in Spring only; elk in Spring and Fall.",
  "checks": ["births_only_in_breeding_season"] }
```

- **Setup** is a list of DiaAPI commands, so a scenario does nothing the panel can't (feedback: editor actions are DiaAPI commands).
- **Checks** are named C++ predicates registered by `S3EcologyModule`. They are evaluated after each band from A1's view and change ledger, and shown as live pass/fail lines.
- **Running a scenario** loads its world and mode, runs the setup, shows the watch text, and pins the check lines in the panel.

| Scenario | GDD source | Mode | Checks (examples) |
|----------|------------|------|-------------------|
| Producer/consumer states | S-2 Test 1 | Controlled | state only moves one tier; matches the health bands |
| Age structure | S-2 Test 2 | Controlled | maturation after N seasons; no births without adults |
| Breeding seasons | S-2 Test 3 | Controlled | births only in breeding season |
| Competition | S-2 Test 4 | Controlled | capacity drops with competitors |
| Pack/herd | S-2 Test 5 | Controlled | Allee below `min_viable`; pack bonus above |
| Year quality | S-2 Test 6 | Controlled | GOOD growth > AVERAGE > BAD |
| Pressure effect | S-2 Test 7 | Controlled | prey avoids high predator/dragon scent (ledger: migration out) |
| Succession counters | S-2 Test 8 | Controlled | counter rises and decays per rule; trigger at 1.0 |
| Events | S-2 Test 9 | Controlled | cluster events fire at the minimum size (multi-cell now) |
| Functional groups | S-2 Test 10 | Controlled | group total = sum of tagged species |
| Emergence | S-3 Test 2 | Living | by eye; event counts shown |
| A3 across biomes | S-3 Test 3 | Living | A3 transitions follow staging (needs A3) |
| Visual tracking | S-3 Test 4 | Living | by eye with species/group overlays and arrows |
| Weather response | S-3 Test 6 | Living | accumulator = sum of 13 weeks; wet cells grow more grass; spring melt boom |

### Long-run runner (Living mode)

> **Amended 2026-10-19 (DiaCellLayers/DiaCellHistory):** The long-run runner writes the `dia.cell_history` export next to its report, so the inspector can read both live and offline data the same way. Snapshots are E1 files.

`cow.s3.longrun seed=<n> seasons=<n> species=<n>` runs headless on a worker job. It drives the pure simulations directly (A4 `WeatherSimulation`, A1 `EcologySimulation`, and A3's logic when present) in the same band order as the live modules, not through `SimulationTick`, so 10,000 seasons don't need 910,000 frames.

- **Budget: the whole report finishes in ≤ 5 minutes wall-clock** (decided 2026-10-18). The default run is Small, 10,000 seasons, about 15 species. Estimated at about 3 minutes per run: A4 about 1.5 min (130,000 weeks at about 0.7 ms) plus A1 about 1–1.5 min. The two determinism runs go on separate worker jobs in parallel. Large is opt-in (`map=Large`, about 6.5 minutes per run) and isn't held to the budget.
- The report records the elapsed time. If the default run goes over 5 minutes, the report flags it and the default season count is cut to fit. The new count goes to the GDD fold-back. Optimising the code is the first fix; the season count is the last.
- The species-scaling check uses short runs (100 seasons each at 5, 10 and 15 species, at Large), so it stays inside the budget.
- It shows progress in the panel and can be cancelled.
- It writes `out/CoW/s3/longrun_<seed>.json` with S-3 Test 5's measurable checks, and `longrun_<seed>.cell_history.json` (amended 2026-10-19, DiaCellLayers/DiaCellHistory):

| Check | Pass |
|-------|------|
| Determinism | two runs of the same seed are byte-identical (hash of the final saved state) |
| Species diversity | ≥ 50% of starting species present at the end |
| No runaway values | no species above 10× carrying capacity |
| Tick time | season band time at season 10,000 within 10% of season 1,000 |
| Succession | ≥ 10% of cells transitioned at least once (needs A3; otherwise reported as skipped) |
| Species scaling | season band time is roughly linear in species count (5, 10, 15) at Large, against A1's 16 ms gate |

The runner and the live modules share the band-order code. If the runner and a live run of the same length disagree, that's a bug: a short version of this comparison is an integration test.

### Worlds

The stage names its default world in its `.diastage` (SD-S1-010). The panel's world picker lists S-1 presets and `.wld.json` files. Two small sandbox worlds ship with the stage as S-1 preset + seed: `sandbox_small_temperate` (grassland, forest, lake, river) and `sandbox_small_gradient` (a wet coast to a dry interior). They're for the guided scenarios. Hand-built test grids stay in A1's GoogleTests only.

## Public Interface

### Stage

`Cluiche/Assets/CoW/Stages/S3Ecology/s3_ecology.diastage`, launched from F-1's debug menu. It holds the default world (preset + seed, `.wld.json` or `world_bundle`; amended 2026-10-19, F-3 World Assets), the default mode (Controlled) and the scenario list.

### Module

```cpp
namespace CoW::S3 {
    enum class EcologyMode : uint8_t { Controlled, Living };

    class S3ClimateInput final : public CoW::Ecology::IEcologyWeatherInput { /* biome baseline × sliders, region overrides */ };
    class S3ClimatologyInput final : public CoW::Ecology::IEcologyWeatherInput { /* A4 weekly means, after A4 */ };

    class S3EcologyModule : public Dia::ApplicationFlow::SimModule {
    public:
        void SetMode(EcologyMode mode);                 // reinitialises ecology; keeps tuning
        void RunScenario(Dia::Core::StringCRC id);
        const S3CheckResults& GetCheckResults() const;
        S3LongRunHandle StartLongRun(const S3LongRunParams& params);
    };
}
```

### Commands (DiaAPI)

`cow.s3.mode`, `cow.s3.world.load`, `cow.s3.climate.set`, `cow.s3.climate.region`, `cow.s3.climate.source` (`stub` or `climatology`), `cow.s3.scenario.run`, `cow.s3.scenario.list`, `cow.s3.checks`, `cow.s3.longrun`, `cow.s3.longrun.cancel`. The S3 panel card calls the same commands. All ecology tools are A1's `cow.ecology.*`.

## Features

| # | Feature | Description | Spec | Status |
|---|---------|-------------|------|--------|
| 1 | Stage & Modes | `.diastage`, F-1 menu entry, world load and picker (SD-S1-010: preset + seed, `.wld.json`, or `world_bundle`; amended 2026-10-19, F-3 World Assets), `S3EcologyModule`, Controlled/Living switch and reinitialise (authoring loop exposed via F-3's World Assets card; amended 2026-10-19, F-3 World Assets), layer-presence panel card, time controls (GameSpeedController), size picker (`cow.s3.map.size`), the two sandbox worlds. HTML mockup of the S3 card + A1 tool panel first (amended 2026-10-19, product review) | TBD | Draft |
| 2 | Pinned Climate | `S3ClimateInput`: biome baseline, global sliders, year scale (GOOD 1.3 / AVERAGE 1.0 / BAD 0.7 values defined here, referenced by A1), region overrides, snow threshold; `cow.s3.climate.*`. Pure logic, TDD (amended 2026-10-19, product review) | TBD | Draft |
| 3 | Guided Scenarios | Uses the CoW-Shared scenario format, loader and check-line panel from S-2.5 Feature 4 (hard dependency; amended 2026-10-19, product review); setup as DiaAPI command lists, named check predicates over A1's view and ledger, live check lines; the 10 Controlled scenarios and 4 Living scenarios | TBD | Draft |
| 4 | Long-Run Runner | Headless band driver over the pure simulations, worker job with progress and cancel, report JSON with the six checks, live-vs-runner integration test | TBD | Draft |
| 5 | Climatology Source | `S3ClimatologyInput`: headless A4 years, per-cell weekly means, cache by world + parameter hash, `cow.s3.climate.source`. Needs A4 Features 1–2 | TBD | Draft |

**Build order:** 1 → 2 → 3 gives a working Controlled mode with no A4 or A3. Feature 4 follows A4 (Living). Feature 5 follows A4 Features 1–2. Living-mode scenarios light up with A4, and A3's checks with A3.

## Inherited Binding Decisions

| ID | Decision | How it applies |
|----|----------|----------------|
| PD-002 | ProcessingUnit/Phase/Module architecture | `S3EcologyModule` is a SimModule on SimPU, and game logic stays on SimPU. It reaches A1, A4 and S-1 through `ModuleRef` |
| PD-004 | No STL containers in public APIs | Climate fields and results use `DynamicArrayC` |
| AD-002 | Four-tier dependency model | Feature-local. Promote the pinned climate to CoW-Shared if S-4 needs it |
| AD-003 | GDD owns design intent | Merging S-2 and S-3 changes the GDD roster. Deviations are listed below |
| AD-005 | One stage per testbed | One stage. Controlled and Living are modes chosen in its panel, not separate stages |

## Deviations from GDD S-2 and S-3

For the DesignDocs owner to fold back into `TestbedRoster.md`, `S2_SingleCellEcology.md` and `S3_LivingEcology.md`:

1. **S-2 and S-3 merge into S-3 Ecology**, a single stage with Controlled and Living modes. S-2 retires from the roster. The numbering follows the layers: S-1 terrain, S-2.5 weather, S-3 ecology, S-4 dragons.
2. **Each layer's testbed is the tool for understanding and tuning that layer.** A1 owns the tools (inspector, change ledger, tuning with write-back, painting, graphs), and S-3 hosts them. A3's `SuccessionDebugDomain` is also hosted by S-3 (amended 2026-10-19, product review).
3. **No single-cell world.** The S-2 tests run on small S-1 worlds, with `isolate_cell` where one cell has to be alone. The equations are proven in A1's GoogleTests.
4. **Year quality GOOD 1.3 / AVERAGE 1.0 / BAD 0.7 values are defined in S-3's pinned climate** (amended 2026-10-19, product review), not an A1 field (A1 C6).
5. **S-2's ecology states:** "Depleted" is Collapsed, and "Recovering" is a flag (A1 C4).
6. **S-2 Test 9's cluster events are multi-cell scenarios now.** On one cell they couldn't fire.
7. **Long-run checks run headless over the pure simulations**, not by fast-forwarding the clock. S-3 Test 5.1's determinism check doesn't need the replay reader.
8. **S-3 Test 5.6's 200×200 target becomes A1's ≤ 16 ms at Large (6,656 cells).** The long-run report has a ≤ 5-minute budget at Small. If 10,000 seasons doesn't fit, the season count drops.
9. **S-3's A3 stage names** (Barren→Pioneer→Established→Mature→Ancient) are replaced by A3's real transitions and recovery stages (A1 C8).
10. **S-3's pressure overlay** shows A1's real pressure fields (prey, predator, dragon and carrion scent, water gradient) rather than "predator pressure and climate stress".
11. **Map sizes** (amended 2026-10-19, product review): S-1 now builds and tests all three sizes (Small, Standard, Large). S-3 adds a size picker and keeps long-run default Small with Large opt-in.

## Dependencies

**Requires:**
- **A1 Ecology**, Features 1–7 and 9 (Feature 8 for save/load).
- **S-1 World Generation**, Features 1 (Terrain) and 8 (Biome Classification), for worlds and the world hand-off. Feature 9 (regions) is soft: without it there's one climate region.
- F-1 (debug menu, debug host), GameSpeedController, `RandomStreamModule`, E1. All Done.
- Dia CellField (dia-needs #14), through A1's overlays.

**Soft (modes and checks light up as they arrive):**
- **A4 Weather:** Living mode, Feature 4 (runner) and Feature 5 (climatology). S-2.5 should pass before Living-mode results are trusted.
- **A3 Biome Succession:** transitions in Living mode, the A3 scenario, and the succession long-run check. A3's `SuccessionDebugDomain` is hosted by S-3 (amended 2026-10-19, product review).
- **S-2.5 Feature 4** (CoW-Shared scenario format, loader and check-line panel): hard dependency for Feature 3 (amended 2026-10-19, product review).

**Required by:** S-4 Dragon (its live mode runs in this world, GDD roster), and V-1 for real tile art.

## Test Strategy

- **Unit (Feature 2):** the baseline matches A1's `IBiomeSeasonalBaseline`; the sliders and year scale multiply as specified; region overrides apply only inside their region; the input is deterministic.
- **Unit (Feature 3):** scenario files load and validate (bad commands and unknown checks are rejected); every named check gets a pass case and a fail case on a hand-built grid.
- **Integration (Features 1, 4):** switching mode reinitialises the ecology and keeps tuning. The runner and a live run of 40 seasons on the same seed give identical A1 state. Cancel stops cleanly.
- **Proof:** the 14 scenarios walked through on the sandbox worlds, and long-run reports on 10 seeds (S-3 metrics). These are recorded in the plan when they run.

## Resolved Design Decisions

1. **Long-run budget: ≤ 5 minutes for the whole report** (2026-10-18). Default Small, 10,000 seasons, determinism runs in parallel; see Long-run runner.
   - Rejected: accepting a long run (more than 20 minutes) as a background job. Too slow to use while tuning.
   - Rejected: cutting the GDD's 10,000 seasons up front. The estimate fits; cut only if measured time says so.
2. **A mode switch keeps tuning but resets edits and paint** (2026-10-18). A "carry the ecology over" option waits until it's needed in practice.

## Open Design Questions

None at system level. Feature-level questions go in each feature spec.

## Status

**Status:** `Approved` (2026-10-18). Interview done the same day (name, pinned climate, proof style, Living-mode growth, 5-minute long-run budget). No open system-level questions. No plan yet; create `s3-ecology.plan.md` when implementation starts (Controlled mode, Features 1–3, needs A1 Features 1–7 and 9 plus S-1 Features 1 and 8).
