# Feature Spec: A4 WeatherModule & Deferred Step

**Parent:** @docs/specs/applications/cow/systems/a4-weather/a4-weather.md (Feature 3)
**Status:** Approved (2026-11-16). Amended 2026-11-17 (user decisions: A5 hand-off, Features 7–9): optional A5 provider latched at `Capture`, coupled spin-up job, World Step order. Amended 2026-11-17 (aligned to locked GDD, CHANGE-047; row 6d): `IWaterInputSource`, `IHydrologySpinUpHost` calls, `IsReadyForRevision`, working buffers allocated once per promotion, header placement of `IHydrologyInputsProvider`, influence book, climate cycles (frozen-phase spin-up), three-buffer `Commit`.
**Depends on:** A4 Features 1, 2 and 6 (`WeatherSimulation`, `WeatherRunner`, parameters, `ApplyPreset`); World Step Features 1 and 2 (`Dia::DeferredStep::IStepStage`, `WorldStepModule::RegisterStage`); DiaDeferredStep Features 1, 2 and 4 (`IParallelExecutor`, the capped executor, `DeferredStepModule`); S-1 (built): `WorldGenerationModule::GetCurrentWorld`, `IsReady(revision)`, `HoldGeneration()`/`ReleaseGeneration()`, `WorldGenerationComplete`/`WorldGenerationFailed`.
**Consumed by:** A4 Features 4 and 5, A1 (`WeatherUpdated`, `IBiomeSeasonalBaseline` provider seam, `WeatherSnapshot`), V-1, S-2.5.

**Amended 2026-11-18 (world-sim review, user decisions):** see "World-sim review amendment" below.

## Summary

`WeatherModule`: the SimModule that makes the pure simulation a live world-step stage. It loads parameters, ingests S-1's terrain on `WorldGenerationComplete`, spins the world up **off SimPU** (decided 2026-11-16), registers a `weather` stage with `WorldStepModule`, and from then on only does what the coordinator asks: `Capture(stepDay)`, `Run`, `Commit`. On commit it publishes `WeatherUpdated`. It owns no cadence, no gating on `SimulationTick`, and no save or history code.

## Goals

1. `WeatherModule` is a `WorldStepModule` stage, never kicking itself and never subscribing to `SimulationTick`.
2. The week and the season of every step come from the **step day** (decided 2026-11-16): `week = WeekOf(stepDay)`, `season = Season(SeasonOf(stepDay) % 4)`. There is no second week counter that can drift.
3. World regeneration (S-1 live editing, S-3.5) never stalls SimPU and never lets an old world's step touch a new world.
4. Every mutation from commands, A3, A1 or a preset is latched at `Capture`, never applied while a step is in flight.
5. A missing or bad parameter file, or a missing baseline provider, degrades loudly and the module still runs.
6. `IsReady(revision)` is the single truth for "this world is initialised and spun up".

## Non-Goals

- Cadence, `k`, the week gate, stalling, kick retry (World Step).
- Weather layers, overlays, commands, metrics, the panel (Feature 5).
- `WeatherSaveParticipant` and the pending-result blob (Feature 4).
- F-3 pack participant implementation (F-3 Feature 6; this feature exposes the apply-parameters seam it calls).
- The A3 bus subscription for `BiomeTransitionComplete` (exposed as a queue now, subscribed when A3 publishes it).

## Design

### Placement and manifest

`Cluiche/CoW/Modules/WeatherModule.h/.cpp`, `WeatherMessages.h` (hand-written PODs following `GameSpeedMessages.h`). **Stage-scoped**, not `stages: ["all"]` (corrects the system spec's sample): each hosting stage declares its own instance, listed **after** `WorldStepModule`.

```json
{ "instance_id": "WeatherModule", "type_id": "WeatherModule",
  "stages": ["S25WeatherPatternsStage"],
  "dependencies": ["MessageBusHostModule", "WorldStepModule", "WorldGenerationModule", "SimQueryHostModule"],
  "soft_dependencies": ["SaveLoadModule", "CellLayersModule", "CellHistoryModule"],
  "config": { "parameters": "Global/Weather/weather_parameters.json", "spinup_weeks": 52,
              "climate": { "file": "Global/Weather/climate_cycles.json", "start": "warm" } } }
```

(The optional `climate` block is Feature 8's; absent means no cycles. Amended 2026-11-17, row 6d.)

`WorldGenerationModule` is added as a hard dependency (the system spec's sample omitted it). `spinup_weeks` is validated (`>= 0`, `<= 520`); 0 means no spin-up (tests).

### Module state

```cpp
WeatherParameters   mParams;                 // loaded; latched into the simulation at Capture
WeatherSimulation*  mActive;                 // the sim the stage steps; null until the first world is ready
WeatherSimulation*  mPending;                // a spun-up sim for the newest revision, waiting for a Capture boundary
uint32_t            mTargetRevision;         // the newest WorldGenerationComplete revision seen
uint32_t            mActiveRevision, mPendingRevision, mCaptureRevision;
uint64_t            mCapturedDay;            // the stepDay of the step captured now (also saved in the pending record, Feature 4)
MutationQueue       mQueue;                  // forcings, biome edits, parameter sets, preset, reload requests (bounded)
InfluenceBook       mInfluences;             // Feature 7 (amended 2026-11-17, row 6d)
ClimateCycles       mClimate;                // Feature 8: definitions, per-world phases; null-equivalent with no file
IHydrologyInputsProvider* mHydroProvider;    // non-owning; null = no A5
IHydrologySpinUpHost*     mHydroHost;        // non-owning; null = A4 spins up alone
```

**Working buffers are allocated once per promotion** (amended 2026-11-17, row 6d; A5 `hydrology-module.md`). A promoted `WeatherSimulation` sizes its working `WeatherFields` and `WeatherHandoff` (`glacierInput`, `influenceWater`) in `Initialise` and never reallocates them between `Capture` and `Commit`, so the pointers `GetWorkingOutput` returns stay valid for A5's whole step. Feature 9's three-buffer rotation swaps which buffer is which only inside `Commit`, never between `Capture` and `Commit`.

### The stage (`WeatherStage : Dia::DeferredStep::IStepStage`)

| Method | Behaviour |
|---|---|
| `IsReady()` | `(mPending != null && mPendingRevision == mTargetRevision) \|\| (mActive != null && mActiveRevision == mTargetRevision)`, and no Unhealthy cause. False during a spin-up job and after `WorldGenerationFailed` |
| `Capture(stepDay)` | 1. **Promote** `mPending` to `mActive` if present (the chain is Idle at this call, so no `Run` holds the old sim; the old sim is destroyed). 2. Drain `mQueue` in arrival order: parameter changes (`SetParameters`), forcings, biome edits (`SetBiome`), preset (`ApplyPreset`), reload. 3. `mCaptureRevision = mActiveRevision`, `mCapturedDay = stepDay`. 4. Latch A5's previous committed week into a `HydrologyInputs` (SimPU-side, from the provider below, including A5's `glacierCheckWeek` and `glacierSnowFloor`; `present == false` when none is set; amended 2026-11-17, A5 Hydrology and row 6d). 5. `snapshot = mInfluences.Latch()` (Feature 7). 6. `sim.Capture(Season(SeasonOf(stepDay) % 4), WeekOf(stepDay) % kWeeksPerYear, baseline, hydro, snapshot)`, then `sim.LatchClimate(stepDay)` when cycles are active (Feature 8) |
| `Run(exec)` | `mActive->Run(exec)`; reads nothing SimPU-owned (DFS-002) |
| `Commit()` | If `mCaptureRevision != mTargetRevision` the step belongs to a regenerated world: **discard** (no `sim.Commit`, no message, one Info log, counted; the book ages nothing). Otherwise `sim.Commit()` (a three-buffer rotation recording `commitDay`, Feature 9), `mInfluences.Age(commitDay)` (Feature 7), set the committed week to `WeekOf(capturedDay)`, publish `WeatherUpdated{week, season}`. `commitDay` is the apply day: the module reads the current sim day from its time seam, which at `Apply` is the apply key (how the stage reads it is settled in plan Task 5) |
| `SerializePending` / `DeserializePending` | Feature 4 |

`WeatherSimulation::Commit` (Feature 2) increments its own week; this feature adds `SetCommittedWeek(uint64_t)` so the module can overwrite it with `WeekOf(stepDay)` immediately after `Commit`, and the snapshot's `GetWeek()` is that value. (Small amendment to Feature 2's `WeatherSimulation`.) A retried or restored step therefore reports the week it represents.

**Baseline provider.** `SetBaselineProvider(IBiomeSeasonalBaseline*)` is called by A1's `EcologyModule` in its `DoStart` (no compile dependency on A1; same shape as `RegisterStage`). With none, the module uses its `TableBiomeSeasonalBaseline` built from the parameters file and reports Degraded with an Info log naming the source. The provider is called only inside `sim.Capture`, never from a worker.

**Hydrology provider (amended 2026-11-17, A5 Hydrology and row 6d).** `SetHydrologyProvider(IHydrologyInputsProvider*, IHydrologySpinUpHost*)` is called by A5's `HydrologyModule` in its `DoStart` (same shape as the baseline provider: raw non-owning pointers, null means no A5; the second argument is the spin-up host, which `HydrologyModule` also implements, so it passes `this` twice; A5 `hydrology-module.md` says `SetHydrologyProvider(this)`, owed an edit to pass the host too). The interface returns copies of `surfaceWetness`, `openWater` and `glacierEligible` from A5's last **committed** week, plus A5's `GLACIER_CHECK_WEEK` and `GLACIER_SNOW_FLOOR` (read from A5's parameter file so A4 and A5 cannot disagree; GDD §11.3; owed to A5's provider). It is called only inside `Capture` on SimPU, never from a worker. With none set, A4 runs weather alone (S-2.5 stays `["weather"]`) and nothing is Degraded.

**Header placement (amended 2026-11-17, row 6d).** `IHydrologyInputsProvider` lives in its own header, `Cluiche/CoW/Shared/Weather/HydrologyInputsProvider.h` (with `HydrologyInputs` kept in `WeatherTypes.h`, Feature 1), not in `WeatherModule.h`, so `HydrologyModule` (A5, in `Modules/`) implements it without including the weather module. A5's pure `Shared/Hydrology/` code includes nothing from `Shared/Weather/`.

**Water source for A5 (amended 2026-11-17, row 6d; A5 `hydrology-module.md`).** `WeatherModule` implements A5's `CoW::Hydrology::IWaterInputSource` (declared in A5's header-only `Shared/Hydrology/HydrologyCoupling.h`: PODs and interfaces, so this is the only compile dependency A4's module has on A5, and it is one-way):

```cpp
bool GetWorkingOutput(WaterSourceView& out) const override;   // pointers to the working waterToGround, temperature, snowPack, glacierInput for the step captured on this call; false when not ready or the step is discarded
bool CopyGroundWaterInfluence(DynamicArrayC<float>& signedDepth) const override;   // Feature 7: influenceWater × ground_water_factor; false = none active
bool IsReadyForRevision(uint32_t worldRevision) const override;   // IsReady() && worldRevision == mTargetRevision
```

A5's `HydrologyModule` registers itself as the consumer (`SetWaterSource(&weather)`); A4 does not look A5 up. A4's `Capture` runs before A5's (declared order), so the buffers exist and belong to the same step; the pointers are read only in A5's `Run`, and stay valid until A4's `Commit` (working buffers allocated once per promotion).

**Climate cycles and the influence book (amended 2026-11-17, row 6d).** The module owns a `ClimateCycles` (Feature 8: loaded from the `climate` config block in `DoStart`, hot-reloaded with `QueueReload`/`params reload target:cycles`; an invalid file gives Degraded and no cycles) and an `InfluenceBook` (Feature 7: `AddInfluence`/`RemoveInfluence`/`ListInfluences` are SimPU-only calls that return an id at once, so they do not use the mutation queue; `mInfluences.Clear()` on a new world revision, a preset and a state import with other dimensions). The module also implements Feature 9's `IInfluencePreviewSource` over the book and exposes `GetSmoothed()`.

**Season.** `SeasonOf(day)` is a season **count** (`day / kDaysPerSeason`), not the `Season` enum; the enum is `SeasonOf(day) % 4` with Spring first. Day 0 is Spring.

### World ingest and spin-up (async)

On `WorldGenerationComplete{worldRevision}`:

1. `mTargetRevision = worldRevision`. This immediately makes `IsReady()` false and cancels any running spin-up job (it checks the revision between weeks).
2. Read terrain from `GetCurrentWorld()` into a `WeatherTerrain` copy (the module never holds the pointer). If there is no water-source cell (EC-01), log an Error, report Unhealthy and start no job.
3. Build a fresh `WeatherSimulation` and launch a job that runs `WeatherRunner::Start(…, spinup_weeks, executor, …, climateFreezeDay)` using the DeferredStep capped executor (a participating coordinator, strict worker cap). Spin-up is silent: no `WeatherUpdated`, no history signal. **Coupled spin-up (amended 2026-11-17, A5 Hydrology and row 6d):** when an `IHydrologySpinUpHost` is set, the module calls `host->BeginSpinUp(revision)` on SimPU as it launches the job (it ingests the world for that revision if A5 has not yet). A non-null ticket means A5 joins: the ticket is a self-contained simulation that **is** the `IHydrologyInputsProvider` for the job, so each spin-up week the job latches A5's spin-up state exactly as a live `Capture` does, calls `weatherRunner.StepWeek`, then `ticket->StepWeek(weekIndex, WaterSourceView over A4's working buffers, exec)`, in World Step order. A null ticket (A5 not joining: init failed, or A5 holds restored state) means A4 spins up alone. On completion with the revision still current, the module sets `mPending` and calls `host->CompleteSpinUp(ticket, revision)` at the same `DoUpdate` poll, so the coordinator's next `Capture` promotes **both** together; a stale or cancelled job destroys the ticket with `IHydrologySpinUpHost::Destroy` (A5 holds nothing of it). **Length is `spinup_weeks`, 52 weeks, with the climate cycles frozen at the world's start phase (`climateFreezeDay`)** (aligned to locked GDD §19, frozen-phase spin-up; the earlier "extended to the longest cycle's settling period" is gone). A5 seeds its slow stores from `StartColdness`. With no host the job runs A4 alone, exactly as before.
4. When the job finishes and its revision still equals `mTargetRevision`, it posts completion to SimPU; the module sets `mPending`, `mPendingRevision`. `IsReady()` becomes true and the coordinator's next `Capture` promotes it. A job whose revision is stale discards its sim.

The job runs on a worker through `JobSystemService` `TrySubmit(fn, tag)` with the `world` tag. With no job system (headless unit tests) a **synchronous launcher** seam runs the same code inline, so tests are deterministic. The spun-up state is bit-identical either way (it is the `WeatherRunner` oracle).

While no kick can happen (`AllReady()` is false whenever `IsReady()` is false), spin-up never overlaps a weekly step of this stage, and the coordinator's other stages do not step either, so the capped executor is not shared concurrently.

`WorldGenerationFailed{reason, …}`: Unhealthy with the reason, `IsReady()` false, any running spin-up is cancelled; the last good world is **not** kept active (S-1's last accepted world stays untouched, but A4's state belongs to a world that is no longer current, so A4 waits for the next success). `HoldGeneration()` is called in `DoStart` and released once the subscriptions are in place, so the first world is never missed.

### Parameters

Loaded in `DoStart` with Feature 1's `LoadWeatherParameters`; on any failure the module falls back to `DefaultWeatherParameters()`, logs an Error with the file and dotted key, and reports Degraded. The file is read **once at init**, never again (consumer requirement, 2026-10-20); the values are copied into the simulation at `Capture`.

Public methods, all latch through `mQueue` and are **safe to call at any time on SimPU**:

```cpp
bool QueueParameterSet(const char* dottedKey, float value, ParamError& err);   // Feature 1 SetParam on a copy; rejects bad input immediately
bool QueueParameterOverrides(const Json::Value& overrides, ParamError& err);   // packs and reload merge (atomic)
bool QueueReload();                                                            // re-reads the file at the next Capture
bool QueueForcing(const WeatherForcing&);  bool QueueBiomeChange(uint32_t cell, Biome);
bool QueueRegionForcing(const WeatherForcing&, const DynamicArrayC<uint32_t>& cells);   // Feature 5: the cell set is registered inside Capture
bool QueuePreset(const WeatherPreset&);
```

Each returns false when `mQueue` (capacity 128 entries) is full and logs a Warning. `Feature 5` registers the commands over these. The F-3 weather pack participant calls `QueueParameterOverrides`. A parameter change takes effect at the next kick's `Capture`, never inside `Run`.

### Application of a queued preset

A queued preset is applied inside `Capture`, before `sim.Capture`, when the chain is provably idle: `ApplyPreset` (Feature 6) resets and refills the committed fields, so it can never run during a step. The module records the preset's season only for the status panel; the step's season is still derived from the day (a preset's `season` is an authoring hint for the **headless runner**, which advances its own clock).

### Snapshot

`GetSnapshot()` returns a `WeatherSnapshot` over `mActive`'s committed fields. It is valid until the next `Commit` or promotion, and subscribers read it **in response to `WeatherUpdated`** only (the system spec's access rule). Before the first world is ready it returns an empty snapshot (`GetCellCount() == 0`).

### Messages

`WeatherMessages.h`: `WeatherUpdated { uint64_t week; Season season; }`. The `BiomeTransitionComplete` payload is A3's to define; until A3 publishes it, `QueueBiomeChange` is the only entry point. No other bus traffic.

### Retiring the placeholder

`JobSystemPlaceholderConsumerModule` (`Cluiche/CoW/Modules/`), its `cow_main.diaapp` entry and vcxproj lines are deleted in this feature: `WeatherModule` is the real `JobSystemService` consumer through the spin-up job. The orphan-provider check passes through `WeatherModule`.

### Stage exit and re-entry

`DoStop`: unregister the stage from `WorldStepModule`, cancel and join the spin-up job (wait for completion, discard its result), destroy both sims, clear `mQueue`, release `HoldGeneration` if still held. Re-entry rebuilds from config on the next `DoStart`; a stale job result from the previous visit must never be posted (the revision and a per-visit generation id both gate it).

### Observability

Health (`IHealthReporter`): Degraded on fallback parameters, fallback baseline, a discarded step, a full mutation queue; Unhealthy on no water source or world generation failed. Logs on the A4 channel `WTH`: Info on parameter source, baseline provider, world revision and spin-up start/finish with duration, a discarded step; Warning on a full queue and a dropped stale spin-up; Error on parameter file failure and EC-01. Traces: `DIA_TRACE_ZONE` around `Capture` and `Commit` and around the spin-up job. Metrics, commands and the profile surface are Feature 5.

## Public API

New: `WeatherModule.h/.cpp`, `WeatherStage.h/.cpp` (the `Dia::DeferredStep::IStepStage`, constructed by the module), `WeatherMessages.h`, `HydrologyInputsProvider.h` (`IHydrologyInputsProvider`; amended 2026-11-17, row 6d), `SpinUpJob.h/.cpp` (pure: the cancellable run over a sim and a revision source, unit-tested without the module).

`WeatherModule` also exposes (amended 2026-11-17, row 6d): `SetHydrologyProvider(IHydrologyInputsProvider*, IHydrologySpinUpHost*)`; the `IWaterInputSource` methods above; Feature 7's `AddInfluence`/`RemoveInfluence`/`ListInfluences`/`GetInfluenceRevision`/`GetInfluencePreviewSource`; Feature 9's `GetSmoothed()`; Feature 8's cycle accessors (`GetClimate()`). Edited: `WeatherSimulation` (`SetCommittedWeek`), `cow_*.diaapp` stage manifests, `CoW.vcxproj(.filters)`. Deleted: `JobSystemPlaceholderConsumerModule.*` and its entries.

## Acceptance Criteria

### Module and manifest
1. `dia validate manifest` accepts the instance, requires `WorldGenerationModule` and `WorldStepModule`, and rejects a manifest listing `WeatherModule` before `WorldStepModule`.
2. `spinup_weeks` outside `[0, 520]` or non-integer is a config error with the key named.
3. The module registers stage id `weather` with `WorldStepModule` in `DoStart` and unregisters in `DoStop`.

### World ingest and spin-up
4. `WorldGenerationComplete` makes `IsReady(rev)` false until the spin-up for `rev` completes; afterwards it is true for `rev` and false for `rev + 1`.
5. A world with no water-source cell leaves `IsReady()` false, logs an Error, reports Unhealthy and starts no job.
6. A second `WorldGenerationComplete` during a running spin-up cancels it; its result is discarded and never becomes active; only the newest revision becomes ready.
7. A spun-up simulation equals `WeatherRunner` run synchronously for the same terrain and `spinup_weeks` (bit-identical committed fields). Spin-up publishes no `WeatherUpdated`.
8. With a **synchronous launcher** the module behaves identically, with the job completing before `WorldGenerationComplete` returns.
9. `WorldGenerationFailed` sets Unhealthy and `IsReady()` false until the next success.
10. `HoldGeneration()` in `DoStart` guarantees the first `WorldGenerationComplete` is received.

### The stage
11. `Capture(stepDay)` derives the season as `Season(SeasonOf(stepDay) % 4)` (days 0, 91, 182, 273 at 91 days per season map to Spring, Summer, Autumn, Winter, using the real `kDaysPerSeason`), and the published week is `WeekOf(stepDay)`.
12. A retried step (represented day earlier than the kick day) publishes the week and uses the season of the **represented** day.
13. `Commit` publishes exactly one `WeatherUpdated` per applied step, after `sim.Commit`, with the committed week already set. A discarded step publishes nothing.
14. A step captured under revision `r` that commits after `WorldGenerationComplete(r+1)` is discarded: no commit, no message, a counter and an Info log. The new world's state is untouched.
15. `Promote` happens only inside `Capture`: a `WorldGenerationComplete` plus finished spin-up while a step is in flight does not change the sim that `Run` uses (proved with a fake executor whose `ParallelFor` observes the sim pointer before and after).
16. After `Capture`, mutating the module's source state (parameters, queue, terrain copy) does not change `Run`'s result (the DFS-002 poison test, once at module level).
17. `Run` never calls the baseline provider (a provider that fails the test when called off SimPU).

### Parameters and queue
18. A missing file, a bad value, or an unknown key falls back to defaults, logs an Error naming the dotted key, reports Degraded, and the module still steps.
19. `QueueParameterSet` with a valid value takes effect at the next `Capture` and not before; an invalid value returns false immediately with the error and queues nothing.
20. `QueueParameterOverrides` is atomic; a queued reload re-reads the file at the next `Capture` and not mid-step; a reload that fails keeps the previous parameters and logs.
21. All queued mutations drain in arrival order inside `Capture`, before `sim.Capture`. A 129th entry returns false with one Warning and degrades health.
22. A queued preset is applied inside `Capture` with the chain idle and takes effect on that step; the committed state a reader sees changes only at the next `Commit`.
23. The module never reads the parameter file again after init except through a queued reload.

### Baseline provider
24. With no provider the table baseline is used and health is Degraded; after `SetBaselineProvider(p)` the next `Capture` uses it and health recovers.
25. A provider returning a value outside `[0, 1]` is clamped at the call site inside `Capture`, logged as an Error once per biome/season, and Degraded.

### Snapshot and readiness
26. `GetSnapshot().GetCellCount() == 0` before the first world is ready, and equals the world's cell count afterwards. `GetWeek()` equals the last published week.
27. A reader of the snapshot inside a `WeatherUpdated` handler sees the committed state of that step.
28. The placeholder module, its manifest entry and its vcxproj lines no longer exist, and `dia check` / the orphan-provider check passes with `WeatherModule` as the `JobSystemService` consumer.

### Lifecycle
29. Enter, exit, enter: nothing from the first visit survives: no stage registration, no pending sim, no queued mutation, no spin-up result (a job that finishes after exit is discarded).
30. `DoStop` during a running spin-up cancels and joins it without a crash; the sanitizer run is clean.

### Amended 2026-11-17 (row 6d): new criteria
35. `IWaterInputSource::GetWorkingOutput` returns pointers into the working `waterToGround`, `temperature`, `snowPack` and `glacierInput` of the captured step, with `cellCount == N`; it returns false when the module is not ready or the step is discarded. The pointers are identical at `Capture` and at `Commit` (working buffers are allocated once per promotion), and A4's `Capture` runs before A5's in World Step order.
36. `IsReadyForRevision(rev)` is true only for the target revision once the module is ready, false for `rev + 1`, and false during spin-up.
37. With a fake `IHydrologySpinUpHost`, the coupled job calls `BeginSpinUp(rev, start)` once, where `start` holds the per-cell `StartColdness` and `StartSnowSurvivesSummer` arrays from Feature 8 (all 0 with no cycles; seam fix 2026-11-17), steps the ticket after each A4 week with A4's working view, and calls `CompleteSpinUp(ticket, rev)` at the same `DoUpdate` poll that sets `mPending`. A null ticket spins up A4 alone. A stale revision or a cancel calls `Destroy(ticket)` and never `CompleteSpinUp`. The coupled result equals the synchronous coupled run (AC 33).
38. Spin-up length is `spinup_weeks` (52) with cycles on or off, and the cycle offsets are identical in all 52 weeks (frozen phase, Feature 8 AC 28); nothing extends it.
39. `Capture` calls `mInfluences.Latch()` and `Commit` calls `Age(commitDay)` once per applied step; a discarded step ages nothing; the book is cleared on a new world revision (Feature 7 ACs 28–33).
40. `IHydrologyInputsProvider` is declared in `HydrologyInputsProvider.h`, and a translation unit including only it (not `WeatherModule.h`) compiles.

## Files Touched

As Public API above; tests `Tests/GoogleTests/CoW/Weather/{TestWeatherModule,TestWeatherStage,TestSpinUpJob}.cpp`; `Assets/CoW/Stages/*/misc/ApplicationFlow/*.diaapp` for the hosting stages (S-2.5 owns its own instance when built). **Spec edits on approval:** a short amendment to A4 Feature 2 (`SetCommittedWeek`); the system spec's Deferred-execution table and Initial state (spin-up async, week from stepDay, stage-scoped manifest, `WeatherStep` is a `Dia::DeferredStep::IStepStage` not an `IDeferredStep`), the Public Interfaces block, and the "Spin-up runs synchronously" sentence. No Dia files are added or edited.

## Binding Decisions

| ID | Decision | How this feature complies |
|----|----------|---------------------------|
| PD-002 | Module architecture | SimModule on SimPU; collaborators by `ModuleRef`; no singletons; the baseline provider is pushed in, not looked up |
| PD-004 | No STL in public APIs | Queue, snapshot and error types are Dia containers |
| AD-001 | Ask permission before Dia edits | None. `JobSystemService`/`TrySubmit`, the capped executor, `WorldStepModule` are used as Approved |
| DFS-002 | `Run` reads only the captured working copy | `Capture` latches everything; promotion and mutation drain happen only inside `Capture`; criteria 15–17 |
| DFS-003 | A late step stalls; the key never slips | The stage never re-times anything; a discarded step is a loss, counted and logged, never a retime |
| SD-PERF-7 | Telemetry cost budgets | Only SimPU writes health and logs; no per-cell telemetry |
| World Step Resolved Decision 4 | Order is declarative | The module registers by id `weather` and has no knowledge of other stages |

## Test Plan

GoogleTests with hand-built terrain, a fake `WorldStepModule` that records registrations and a driver that calls `Capture`/`Run`/`Commit`, a fake `WorldGenerationModule` event source, a fake message bus, and the synchronous launcher for the job. One integration test uses the real capped executor with 2 workers. TDD order: spin-up job (pure), stage derivation of week and season, revision guards, queue and parameter latching, baseline, lifecycle. Sanitizer over the cancel/join and re-entry tests. Scoped `--filter` while iterating, one full `--shards >= 2` run at the end. No mutation checks. Gap audit and edge pass is the last plan task. **Observation scan** at verify.

## Open Design Questions

1. **A discarded step costs a week.** A step captured for revision `r` and committed after `r+1` arrives is thrown away. That is right for a regenerated world, but it also fires when the *same* world is re-announced (S-1 re-publishes `WorldGenerationComplete` on import). If that re-announcement can carry an unchanged world, the module should compare a content hash and skip regeneration; S-1 exposes `GetWorldContentHash` only as a pending F-3 seam. Until then a re-announced identical world re-spins and loses at most one in-flight week.
2. **Promotion inside `Capture` means a late spin-up is waited on.** If the spin-up is still running at the next due week, `IsReady()` is false and the week is simply not kicked (the coordinator waits). At 52 weeks and ~10 ms CPU that is well under a game week at every speed preset, but at the Slowest preset with a 2-worker cap it is an unmeasured assumption. Feature 5's `profile` and the first S-2.5 run should report spin-up wall time.
3. **Should the module own its `WeatherSimulation` by value instead of two pointers?** Two heap instances (active and pending) allocate ~0.5 MB at Large per regeneration and are destroyed in `Capture`. A swap of two members is allocation-free but forces the pending sim to be built in place while the active one is still stepping. Pointer ownership chosen for clarity; revisit if the allocation shows in the Large perf profile.

## Gap-pass amendments (2026-11-16)

Found by the plan-writing pass; each item below **supersedes** any contradicting text above. Numbering is stable.

1. **Mutation queue entry types.** The queue's tagged entries are: `SetParam`, `ParamOverrides`, `Forcing`, `RegionForcing`, `BiomeChange`, `Preset`, `Reload`, plus **`StateReplace`** (a decoded weather-state record from `state import`, applied at `Capture` like a preset: reseed then restore) and **`ClearWindOverrides`** (removes every wind override). New API: `QueueStateReplace(const WeatherStateRecord&)` and `QueueClearWindOverrides()`. `QueueRegionForcing` is part of this feature's API (it is not a later amendment).
2. **Spin-up completion.** The job writes its result into a SimPU-owned slot (`SpinUpSlot`: an atomic state `Idle/Running/Done/Cancelled`, the revision, and the simulation pointer). `WeatherModule::DoUpdate` polls the slot each update; on `Done` with the target revision it sets `mPending`. No bus message, no callback onto SimPU.
3. **Hosting stage.** The manifest sample is S-2.5's. Every hosting stage (S-2.5, S-3, S-3.5) declares its own `WeatherModule` instance after its own `WorldStepModule`.
4. **Season at the end of spin-up.** Spin-up runs `spinup_weeks` from calendar offset 0 (Spring). With 52 weeks it ends at the end of Winter, one step before Spring. The first live step takes its season from its step day and may differ from the season spin-up ended in (a world loaded mid-year). That is accepted: spin-up exists to settle winds, moisture and temperature, and temperature relaxes toward its seasonal target within the first weeks (heat capacity). No correction is made.
5. **Season arithmetic** uses the real `kDaysPerSeason` and `kDaysPerWeek` constants from `GameSpeedController.h`, never a literal 91 or 7.
6. **`WeatherSimulation::ClearWindOverrides()`** (remove every wind override, keep other forcings) is added by **this feature** alongside `QueueClearWindOverrides`; Feature 5's `wind clear` uses it through the queue.

7. (amended 2026-11-17, A5 Hydrology) **Optional A5 provider and coupled spin-up.** `SetHydrologyProvider`, the `HydrologyInputs` latch in `Capture` and the coupled spin-up job are described in the stage table and the ingest section above. World Step order is `["weather", "hydrology", "ecology"]`; this module still registers only by id `weather` and knows nothing of other stages.
9. (amended 2026-11-17, aligned to locked GDD, CHANGE-047; row 6d) **Spin-up is 52 frozen-phase weeks.** Any text above saying the spin-up is "extended" by climate cycles is superseded. `SetHydrologyProvider` takes the spin-up host as a second argument. `IWaterInputSource`, `IsReadyForRevision`, the allocate-once working buffers and the header placement are in the Design section and criteria 35–40.
8. (amended 2026-11-17, A5 Hydrology) **New acceptance criteria.** 31. `Capture` latches the provider's `surfaceWetness`, `openWater`, `glacierEligible`, `glacierCheckWeek` and `glacierSnowFloor` once; poisoning the provider after `Capture` does not change `Run` (DFS-002); the provider is called only on the capturing thread. 32. With no provider set, `Capture` passes `present == false`, health is not Degraded, and the run equals a run built before A5 existed. 33. With a provider, the spin-up job runs A4 and A5 together and a synchronous coupled run (`WeatherRunner` + `HydrologyRunner`) gives bit-identical committed fields for both. 34. A cancelled coupled spin-up discards both simulations.

## World-sim review amendment (2026-11-18)

User decisions from the world-sim review. Each item **supersedes** any contradicting text in this spec. Sources: [summary](../../../../../research/worldsim_data_driven/summary.md) §0, §0.1; [parameter-files](../../../dia/systems/diaattribute/parameter-files.md); [biome-registry](../s1-worldgeneration/biome-registry.md); [worldsim-kit](../worldstep/worldsim-kit.md); [grid-search-helpers](../../../dia/systems/diapathfinding/grid-search-helpers.md); [blackboard-scopes](../../../dia/systems/diablackboard/blackboard-scopes.md).

1. **Stage (system items 4, 6).** Supersedes The stage (`WeatherStage : Dia::DeferredStep::IStepStage`) table, Module state and Stage exit and re-entry where they hand-roll active/pending, revisions, queue, spin-up slot, hold and discard. `WeatherStage` derives `WorldStepStageBase`; A4 implements `InitialiseSim`, `LatchInputs`, `ApplyEntry` (the 9 kinds of Open Q1 item 1), `RunStep`, `PublishCommit`. Stage id `weather`; order `["weather", "hydro", "ecology"]`; `ValidatePending` is on the interface.
2. **Parameters (item 1).** Supersedes the Parameters section, the manifest `parameters` path, and the Parameters and queue ACs about fallback. `DoStart` loads the sidecar through `ParameterFile`; failure sets health Failing and the stage is never ready (no `DefaultWeatherParameters` fallback). `QueueParameterSet` / `ParamOverrides` entries call `ParameterFile::Set` / `ApplyPack` when drained. `FileWatcher` (dev builds) queues `Reload` (item 11).
3. **Health and metrics (item 5).** Supersedes the Observability section: Degraded causes stay (fallback baseline, discarded step, queue full); the Unhealthy causes become Failing (no water source, generation failed, parameter sidecar fault). Metrics come from the base.
4. **Messages (item 10).** Supersedes the Messages section: `WeatherUpdated` is declared in `cow_worldsim.diagamemessages` and generated.
5. **`PublishCommit` (items 4, 18).** After `sim.Commit` it ages the influence book, publishes `WeatherUpdated`, and writes season, climate phase and drought flags to the world blackboard with `Blackboard::Set`.
6. **Save id (item 6).** Participant registers as `cow.weather`.

## Status

**Status:** `Approved` (2026-11-16). Amended 2026-11-17 (A5 hand-off). Amended 2026-11-17 (aligned to locked GDD, CHANGE-047; row 6d). Plan: [weather-module.plan.md](weather-module.plan.md) (written 2026-11-16, not started).

**Amended 2026-11-18 (world-sim review).** Status stays `Approved`. Amended 2026-11-18 (world-sim review decisions; user-approved designs in the specs referenced below).


## Draoi layer amendment (2026-11-21)

Applies the World-Sim Kit's "Draoi layer amendment" (@docs/specs/applications/cow/systems/worldstep/worldsim-kit.md) and the system spec's matching section (@docs/specs/applications/cow/systems/a4-weather/a4-weather.md). Supersedes contradicting text above. Design decisions are unchanged; status stays `Approved`.

- **Stays in CoW (this whole feature):** `WeatherModule` and `WeatherStage : WorldStepStageBase` remain CoW code at `Cluiche/CoW/Modules/`, along with the manifest, `WeatherMessages` / `WeatherUpdated` (bus notification only), snapshot publication, spin-up job, health and metrics. It is the stage adapter of PD-013.
- **Becomes thin calls into `DraoiHexWorldWeather`:** `InitialiseSim`, `ApplyEntry`, `RunStep` and `RestoreCommitted` call `WeatherSimulation` `Initialise` / `Capture` / `Run` / `Commit` and `WeatherRunner`; `LatchInputs` and `PublishCommit` stay adapter code. `WeatherSimulation` and `WeatherRunner` are no longer owned or specced here; see Features 1 and 2.
- **Reworded:** "Module state" lists the sim as an owned `DraoiHexWorldWeather` object. `IWaterInputSource` / `IHydrologyInputsProvider` / `IBiomeSeasonalBaseline` seams become **optional input structs of per-cell arrays**, filled once per step at `Capture` by the adapter (kit items 4 and 5): the baseline array, and `HydrologyInputs { surfaceWetness, openWater, glacierEligible }` owned by `DraoiHexWorldWeather` and filled by `FillHydrologyInputs` from `DraoiHexWorldHydrology` (names per the A5 amendment); the adapter also fills A5's `WaterInput` arrays (including the optional `groundWaterInfluence`) from A4's working copy, and the coupled spin-up driver stays here: the Baseline-provider and A5-provider ACs now test that the adapter fills the arrays and that the defaults apply when absent. The "no water source" Failing cause becomes "no water-input array and no default available". `IParallelExecutor` comes from DiaThreading.
- **Files Touched:** `Shared/Weather/WeatherSimulation.*` and other pure types are no longer touched here (they live in the Draoi package); only the module, stage, messages, manifest and adapter tests are.
- **Open question:** none new (see the system spec's influence-book placement question).