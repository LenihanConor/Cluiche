# Feature Spec: A4 WeatherModule & Deferred Step

**Parent:** @docs/specs/applications/cow/systems/a4-weather/a4-weather.md (Feature 3)
**Status:** Approved (2026-11-16)
**Depends on:** A4 Features 1, 2 and 6 (`WeatherSimulation`, `WeatherRunner`, parameters, `ApplyPreset`); World Step Features 1 and 2 (`IWorldStepStage`, `WorldStepModule::RegisterStage`); DiaDeferredStep Features 1, 2 and 4 (`IParallelExecutor`, the capped executor, `DeferredStepModule`); S-1 (built): `WorldGenerationModule::GetCurrentWorld`, `IsReady(revision)`, `HoldGeneration()`/`ReleaseGeneration()`, `WorldGenerationComplete`/`WorldGenerationFailed`.
**Consumed by:** A4 Features 4 and 5, A1 (`WeatherUpdated`, `IBiomeSeasonalBaseline` provider seam, `WeatherSnapshot`), V-1, S-2.5.

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
  "config": { "parameters": "Global/Weather/weather_parameters.json", "spinup_weeks": 52 } }
```

`WorldGenerationModule` is added as a hard dependency (the system spec's sample omitted it). `spinup_weeks` is validated (`>= 0`, `<= 520`); 0 means no spin-up (tests).

### Module state

```cpp
WeatherParameters   mParams;                 // loaded; latched into the simulation at Capture
WeatherSimulation*  mActive;                 // the sim the stage steps; null until the first world is ready
WeatherSimulation*  mPending;                // a spun-up sim for the newest revision, waiting for a Capture boundary
uint32_t            mTargetRevision;         // the newest WorldGenerationComplete revision seen
uint32_t            mActiveRevision, mPendingRevision, mCaptureRevision;
MutationQueue       mQueue;                  // forcings, biome edits, parameter sets, preset, reload requests (bounded)
```

### The stage (`WeatherStage : IWorldStepStage`)

| Method | Behaviour |
|---|---|
| `IsReady()` | `(mPending != null && mPendingRevision == mTargetRevision) \|\| (mActive != null && mActiveRevision == mTargetRevision)`, and no Unhealthy cause. False during a spin-up job and after `WorldGenerationFailed` |
| `Capture(stepDay)` | 1. **Promote** `mPending` to `mActive` if present (the chain is Idle at this call, so no `Run` holds the old sim; the old sim is destroyed). 2. Drain `mQueue` in arrival order: parameter changes (`SetParameters`), forcings, biome edits (`SetBiome`), preset (`ApplyPreset`), reload. 3. `mCaptureRevision = mActiveRevision`. 4. `sim.Capture(Season(SeasonOf(stepDay) % 4), baseline)` |
| `Run(exec)` | `mActive->Run(exec)`; reads nothing SimPU-owned (DFS-002) |
| `Commit()` | If `mCaptureRevision != mTargetRevision` the step belongs to a regenerated world: **discard** (no `sim.Commit`, no message, one Info log, counted). Otherwise `sim.Commit()`, set the committed week to `WeekOf(capturedDay)`, publish `WeatherUpdated{week, season}` |
| `SerializePending` / `DeserializePending` | Feature 4 |

`WeatherSimulation::Commit` (Feature 2) increments its own week; this feature adds `SetCommittedWeek(uint64_t)` so the module can overwrite it with `WeekOf(stepDay)` immediately after `Commit`, and the snapshot's `GetWeek()` is that value. (Small amendment to Feature 2's `WeatherSimulation`.) A retried or restored step therefore reports the week it represents.

**Baseline provider.** `SetBaselineProvider(IBiomeSeasonalBaseline*)` is called by A1's `EcologyModule` in its `DoStart` (no compile dependency on A1; same shape as `RegisterStage`). With none, the module uses its `TableBiomeSeasonalBaseline` built from the parameters file and reports Degraded with an Info log naming the source. The provider is called only inside `sim.Capture`, never from a worker.

**Season.** `SeasonOf(day)` is a season **count** (`day / kDaysPerSeason`), not the `Season` enum; the enum is `SeasonOf(day) % 4` with Spring first. Day 0 is Spring.

### World ingest and spin-up (async)

On `WorldGenerationComplete{worldRevision}`:

1. `mTargetRevision = worldRevision`. This immediately makes `IsReady()` false and cancels any running spin-up job (it checks the revision between weeks).
2. Read terrain from `GetCurrentWorld()` into a `WeatherTerrain` copy (the module never holds the pointer). If there is no water-source cell (EC-01), log an Error, report Unhealthy and start no job.
3. Build a fresh `WeatherSimulation` and launch a job that runs `WeatherRunner::Start(…, spinup_weeks, executor)` using the DeferredStep capped executor (a participating coordinator, strict worker cap). Spin-up is silent: no `WeatherUpdated`, no history signal.
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

New: `WeatherModule.h/.cpp`, `WeatherStage.h/.cpp` (the `IWorldStepStage`, constructed by the module), `WeatherMessages.h`, `SpinUpJob.h/.cpp` (pure: the cancellable run over a sim and a revision source, unit-tested without the module). Edited: `WeatherSimulation` (`SetCommittedWeek`), `cow_*.diaapp` stage manifests, `CoW.vcxproj(.filters)`. Deleted: `JobSystemPlaceholderConsumerModule.*` and its entries.

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

## Files Touched

As Public API above; tests `Tests/GoogleTests/CoW/Weather/{TestWeatherModule,TestWeatherStage,TestSpinUpJob}.cpp`; `Assets/CoW/Stages/*/misc/ApplicationFlow/*.diaapp` for the hosting stages (S-2.5 owns its own instance when built). **Spec edits on approval:** a short amendment to A4 Feature 2 (`SetCommittedWeek`); the system spec's Deferred-execution table and Initial state (spin-up async, week from stepDay, stage-scoped manifest, `WeatherStep` is an `IWorldStepStage` not an `IDeferredStep`), the Public Interfaces block, and the "Spin-up runs synchronously" sentence. No Dia files are added or edited.

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

## Status

**Status:** `Approved` (2026-11-16). Plan: [weather-module.plan.md](weather-module.plan.md) (written 2026-11-16, not started).
