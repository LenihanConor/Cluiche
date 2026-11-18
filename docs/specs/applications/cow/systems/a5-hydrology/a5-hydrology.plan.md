# Plan: A5 Hydrology (system)

**Spec:** @docs/specs/applications/cow/systems/a5-hydrology/a5-hydrology.md
**Status:** Not Started

This is the system-level tracker, rebuilt 2026-11-18 against the world-sim review amendment and the Spec-6 resolutions. Each row is a feature spec with its own plan, and the detailed tasks live there. A feature is Done only when its own plan's final task is Done. Plans are named below, never cited by task number: foundation and A4 plans are being rebuilt in parallel.

A5 sits in the weekly world step between A4 and A1, so the order inside one step is A4 -> A5 -> A1, with A3 inline. The cross-system order lives in the World Step plans and `a4-weather.plan.md`.

A5 no longer builds its own parameter, biome, save, command, selector or stage-skeleton machinery. It **uses** the foundation pieces below and adds only its own data (sidecar, presets, fields) and its own hooks.

## Build order and parallelism

Foundation first. These are separate specs with their own plans; A5 rows wait on them by name:

```
Parameter Files (DiaAttribute)
   -> Biome Registry (S-1)            [also needs Parameter Files for the terrain profile maps]
   -> Grid Search Helpers (DiaPathfinding)   [independent of the two above; S-1 migration only]
   -> World-Sim Kit (World Step F4)   [needs Parameter Files, Biome Registry, Grid Search Helpers]
   -> A4 Weather F1-F3 (+ the World Step plans for the stage chain)
   -> A5 Hydrology:

F1 data & parameters ---> F2 water pipeline ---+--> F6 presets & regression (pure parts; R-Reach on the fixed driver)
                                               |
                                               +--> F3 module & World Step stage --+--> F4 save / load
                                                      (needs the kit, World Step,    +--> F5 debug & history
                                                       A4 F1-F3, DiaDeferredStep)
```

What can start before the foundations are Done, on fakes or hand-built data:

- **F2 (water pipeline)** is pure logic on hand-built grids and a hand-filled `HydrologyParameters` struct. Everything except the parameter-snapshot task, the generated-world checks and the golden can start once F1's types, test grids, `BasinCurve`, derivation, `Initialise` and topology tasks are Done. Its tree grouping needs the Grid Search Helpers plan Done; its soil multiplier test needs the Biome Registry plan Done.
- **F1's** types, plain parameter struct, test grids, `BasinCurve` and derivation need only the `BiomeIndex` type from the Biome Registry plan. The sidecar, strict load, packs and binding tasks need the Parameter Files and Biome Registry plans Done.
- **F6's** loader, scripted driver and per-scenario checks need the World-Sim Kit's `CellSelectorSet` (needs the World-Sim Kit plan Done); its R-Reach on the fixed driver needs F2 only.
- **F3's** pure parts (sink aggregation, influence translation, snapshot, spin-up ticket) can be built against the kit's `FakeWorldStage` once the World-Sim Kit plan has the stage base Done. The real stage needs the World Step plans (stage chain), A4 F3 and DiaDeferredStep F1 to F4 Done.
- **F5's** pure parts (command model, event detection on snapshots, health evaluation) can start after F2; its commands go through `SimCommandSet` and `cow.hydro.params`, which need the World-Sim Kit plan Done.
- **F4** needs F3, the kit's save codec and DiaDeferredStep F3 Done.
- **A4 coupling.** A4 F1 to F3 are amended for the hand-off (`waterToGround`, `glacierInput`, firn, A5 inputs latched at `Capture`). A5 F3 and A4 F3 meet at `SetHydrologyProvider(provider, host)` and `SetWaterSource`, wired through the stage base's `OnStartExtra` / `OnStopExtra`. Build them in either order behind fakes, then integrate.

## Sign-offs (AD-001)

None needed in A5. All A5 code is CoW under `Cluiche/CoW`. Engine pieces are consumed, not changed: DiaDeferredStep, DiaCellLayers, DiaCellHistory, DiaAttribute `ParameterFile`/`ParamBinding`, DiaValueSchema, DiaCondition (through the kit), DiaPathfinding grid searches, `HexTopology`, `CellLayerGolden.h` and `SimQueryHost`. The Dia changes those foundations need were signed off in their own specs (2026-11-18).

A Dia need found mid-task is flagged and asked about, not placed in the game.

## Cross-feature rules

- **Use, don't rebuild.** A5 tasks use `ParameterFile`/`ParamBinding`, the `BiomeRegistry`, the grid search helpers, `WorldStepStageBase`, the save codec, `SimCommandSet`, `ParamCommandHelper`, `WorldCellFields` and `CellSelectorSet`. A task that would write an A5-private version of one of these is wrong: stop and flag it.
- **Naming** comes from `WorldSimNames.h`: stage id `hydro`, participant id `cow.hydro`, layer ids `hydro.<field>`, schema keys `cow.hydro.layers/<field>`, formats `cow.hydro_params`, `cow.hydro_state`, `cow.hydro_preset`. Events are declared in `cow_worldsim.diagamemessages`.
- **Verification cadence:** each task runs a narrow `dia run googletest --filter="<suite under work>" --shards 2` (for example `*HydrologyPipeline*`, always `--shards 2`); the feature-wide `*Hydrology*` filter runs only in that feature's scoped-verification task. `SLOW_` suites (100-year runs, R-Reach, any run of 10 or more simulated years) are skipped by default and run once in the feature's scoped-verification task with `--filter="SLOW_*"` and once in the weather-stack close-out. The **full** suite, the single Release build and the cross-feature checks run once, in the World Step weather-stack close-out. Row 7 executes its cross-feature checks there:
  - deferred vs synchronous over three stages;
  - save mid-step;
  - R-Reach coupled with A4.
- Sanitizer on request only. **No mutation checks.** Debug perf is a ballpark note, never a gate. World-sim results are verified from cell data and stats, not screenshots.
- **TDD** for all new public API (`.claude/skills/tdd.md`). RED is proven with quoted output. Mechanical tasks are exempt.
- **Gap audit** closes every feature plan, followed by the Observation Opportunity Scan, scoped verification and close-out.
- **Layer goldens:** `hydrology_layers.golden.json`. F2 owns the `pipeline/` keys and F6 the `scenario/` keys. Keys use the renamed layer ids (`hydro.soil_moisture` and so on). Regenerate only deliberately (`DIA_UPDATE_GOLDEN=1`). The number of goldens is recounted at build.
- **Water balance** is checked in every pipeline-touching task (Feature 2's tolerance). A balance regression is a stop-and-debug, not a tolerance bump.
- **Commit after each task**, with the plan row updated in the same commit (`dia docs plan`).
- **Model column:** haiku and sonnet only. Ask before any opus dispatch (feedback_confirm_opus).
- **Perf is report-only, Debug ballpark** (`cow.hydro.step_ms`). A bad ballpark queues a perf pass, it does not gate.

## Tasks

Row number = feature number. Row 0 is the foundation check. Recommended build order after the foundations: **1, 2, 6, 3, 4, 5**.

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 0 | **Foundation readiness check, read only.** Confirm the Parameter Files, Biome Registry, Grid Search Helpers, World-Sim Kit and A4 F1 to F3 plans are Done or in the state each feature below needs; list per feature which foundation it is waiting on. Note that blackboard-scopes.md is not used by any A5 spec | Findings recorded in notes | Not Started | haiku | TDD-exempt. Write "needs <plan> Done" on each waiting feature row; never start an A5 task that would reimplement a missing foundation piece |
| 1 | **Feature 1: Data & Parameters.** @hydrology-data.plan.md | Its final task | Not Started | — | Root. Types and `Initialise` are pure logic on hand-built terrain; the sidecar, strict load, packs and binding tasks need Parameter Files + Biome Registry Done |
| 2 | **Feature 2: Water Pipeline.** @water-pipeline.plan.md | Its final task | Not Started | — | Holds the design risk (cascade, basins, balance). Starts after F1's types, grids, derivation, `Initialise` and topology tasks. Soil multiplier from S-1's terrain profile |
| 3 | **Feature 3: Module & World Step Stage.** @hydrology-module.plan.md | Its final task | Not Started | — | Derives `WorldStepStageBase` (hooks only). Needs World-Sim Kit, the World Step plans, A4 F1 to F3, DiaDeferredStep F1 to F4 and S-1 amendment 8 for real worlds |
| 4 | **Feature 4: Save / Load.** @hydrology-save.plan.md | Its final task | Not Started | — | Kit codec: `SavedField` list, biome palette, `cow.hydro`. Needs F3, the World-Sim Kit and DiaDeferredStep F3 |
| 5 | **Feature 5: Debug & History.** @hydrology-debug.plan.md | Its final task | Not Started | — | 8 layers (`hydro.<field>` ids), six sim-hosted command names (`state`, `basins`, `find`, `params`, `preset`, `balance`; recount at build), history defaults, health. Pure parts can start after F2 |
| 6 | **Feature 6: Presets & Regression.** @hydrology-presets-and-regression.plan.md | Its final task | Not Started | — | `cow.hydro_preset` files, `CellSelectorSet` selectors, `hydro.basin_kind`. Fixed-driver parts start after F2 and the World-Sim Kit. The coupled `SLOW_` R-Reach waits for A4 F3 + A5 F3 |
| 7 | **System close-out.** System checks live in the World Step weather-stack close-out: this row is Done when that row is Done for this system's items. Cross-feature checks (executed in that combined close-out): the 104-week deferred-vs-synchronous oracle over `["weather","hydro"]` and `["weather","hydro","ecology"]` (once A1 is in); a save taken mid-step and reloaded is bit-identical; the coupled `SLOW_` R-Reach; `dia check deps`, `dia docs precommit`, `dia check spec-sync`. Full suite, Release build and sanitizer are not run by this row | All feature plans Done; pass/fail and failing names only | Not Started | haiku | Report a failing cross-feature check, don't fix it in this row |
| 8 | **S-2.6 handoff.** When the GDD locks S-2.6 Rivers & Floods (its interview was requested 2026-11-17), write the Cluiche S-2.6 testbed spec (`/spec-system`) and add it to `BACKLOG.md` | Spec drafted and approved | Not Started | sonnet | Not an A5 deliverable. It is here so it isn't lost |
| 9 | **Close-out:** `dia docs spec-done docs/specs/applications/cow/systems/a5-hydrology/a5-hydrology.md --archive-plan` | Spec Done | Not Started | haiku | **Do not run until rows 1 to 7 are Done** |

## Open items

1. **The S-1 amendment 8 build sits outside this plan** (S-1 `consumer-amendments.plan.md`). F3's real-world ingest needs it, so schedule it before row 3.
2. **The A1 water-sink provider** (vegetation ET) is A1's to build, against A5 F3's `IWaterSinkProvider`. It is not tracked here.
3. **The GDD open item:** the DRY_RIVERBED marker duration (20 to 100 years) is likely too long. Revisit it in S-2.6.
4. **No feature owns three amendment items.**
   - Amendment item 13: the F-3 `hydro` pack kind with a `HydrologyPackParticipant`, and the T-2 form route for `cow.hydro_params`. There is no feature spec or plan row for either.
   - Amendment item 9: A5's contribution of `hydro.*` fields (including `hydro.basin_kind`) to `WorldCellFields`. hydrology-presets-and-regression.md relies on it, but no feature lists the field set or registration (hydrology-data.plan.md builds only the pure basin-kind rule).
   - Amendment item 8: `FileWatcher` reload of the sidecar and packs, which is shared with `params {action: reload}`.
5. **Parameter lifecycle is undefined** (see hydrology-data.plan.md and water-pipeline.plan.md Open items): which `hydro.*` parameters may change between steps, who takes the `ParamBinding` snapshot, and how per-biome map columns bind into `BiomeIndex`-indexed arrays (parameter-files.md shows member-pointer binding only).
6. **Stale body text in a5-hydrology.md.** Architecture and Public Interfaces still say stage `hydrology`, `HydrologyStage : IWorldStepStage`, 43 descriptors, `cow.hydro.set_param`, `hydro.soil` layer ids and `.hydropreset.json`; these are superseded by the amendment and Spec-6 resolutions, but the "Superseded text" table does not list layer ids, the preset path or the `crossing` selector. Counts to recount at build: 43 scalar parameters, 3 biome maps, 8 layers, 6 command names, goldens.
7. **Terrain ingest list** omits terrain type and `soil_depth_mult` (hydrology-data.plan.md Open item 2).
8. **Spec-6 resolution 3 (biome palette in saves)** and the glacier/biome handoff to A3 name biomes through the registry (`Alpine`/`Tundra` in "A3 reads"); the registry has no stated roles for them (only `ocean`, `lake`, `fallback`), so A3's biome choice cannot go through roles today.
