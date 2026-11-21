# Feature Spec: World Step Module & Cadence

**Parent:** @docs/specs/applications/cow/systems/worldstep/worldstep.md (Feature 2)
**Status:** `Done` (2026-11-20)
**Depends on:** World Step Feature 1 (`WorldStepChain`, `IWorldStepStage`, `LoadWorldStepConfig`); DiaDeferredStep Features 1, 2 and 4 (`DeferredStepScheduler`, `IStepKeySource`, `DeferredStepModule::GetScheduler()`, `SerialExecutor`); GameSpeedController's `SimulationTick` message (built); `SimQueryHostModule` with `SimHelpInfo` (built); DiaObservation pillars.
**Consumed by:** A4 Feature 3, A5 Feature 3 and A1 Feature 7 (they call `RegisterStage`; A5 added 2026-11-17, A5 Hydrology), S-2.5, S-2.6, S-3 and S-3.5 stages (each declares an instance), Feature 3 (save delegation and history signal sit on this module).

## Summary

`WorldStepModule`, the SimModule that turns the pure chain into a running world step: it owns the `world` DiaDeferredStep channel, reads the game day, kicks once per game week when every stage is ready, applies at `day + k`, and runs the same chain inline when configured synchronous. It is the `IStepKeySource` for the DeferredStep scheduler, retries a week it could not kick, and exposes two sim-hosted names, four metrics, health and logs.

Save delegation and the history signal's wiring are Feature 3. This feature provides the `Apply()` seam where Feature 3's single signal call goes.

## Goals

1. Exactly one kick per game week once ready, including weeks that are also Season/Year/Decade/Century boundaries.
2. A week is **never lost**: a kick that finds the channel in flight is retried on a following tick (decided 2026-11-16).
3. The step applies at exactly `kick day + k`, even if it finished early; a late step stalls and is reported, never slips.
4. A deferred run and a synchronous run of the same stages produce bit-identical stage state after the same number of applied steps.
5. A mis-composed stage (configured stage never registers) is a loud failure; normal stage teardown is silent.
6. Any partial registration of the sim-hosted names rolls back, and the module leaves nothing behind across stage re-entry.

## Non-Goals

- Per-band `k`, two channels, or an incremental Century band (system Resolved Decision 1; revisit only with A1's measurement).
- `SerializePending`/`DeserializePending` delegation, stage-id tags and the history signal (Feature 3).
- Stage logic (A4, A1).
- Worker cap, stall policy, scheduler internals (DiaDeferredStep).

## Design

### Placement and manifest

`Cluiche/CoW/Modules/WorldStepModule.h/.cpp`, a `SimModule` on SimPU, **stage-scoped** (never `stages: ["all"]`): each hosting stage declares its own instance with its own `order`. The config is Feature 1's `WorldStepConfig` (`order`, `apply_after_days`, `deferred`).

```
manifest order (the stage .diaapp):   MessageBusHostModule … GameSpeedController
                                      → DeferredStepModule → WorldStepModule → WeatherModule → HydrologyModule → EcologyModule (HydrologyModule added 2026-11-17, A5 Hydrology; a stage that omits a module just drops it)
```

Reverse order on stop is what lets stage modules unregister while the scheduler is alive. `dependencies`: `DeferredStepModule`, `MessageBusHostModule`, `SimQueryHostModule`. `soft_dependencies`: `SaveLoadModule`, `HistoryDriverModule`. `dia validate manifest` must accept the type and reject a stage that lists `WorldStepModule` after the stages that register with it.

### Stage registration

```cpp
class WorldStepModule : public SimModule, public IDeferredStep, public IStepKeySource {
public:
    bool RegisterStage(StringCRC id, IWorldStepStage& stage);   // stages call this in DoStart
    void UnregisterStage(StringCRC id);                         // and in DoStop
    // …
};
```

Stages reach it with `ModuleRef<WorldStepModule>` (no singletons, no service locator). Registration is idempotent per id (a duplicate returns false and logs once). The module remembers every id it has ever seen register (`mEverRegistered`): a stage that registered and later left is **retired** (normal teardown), a configured id that never registered is **missing** (config error). Only `missing` is an error.

### Key source

`WorldStepModule` implements `Dia::DeferredStep::IStepKeySource` (decided 2026-11-16, closes system Open Q3). `GetCurrentKey()` returns `mLatestDay`, the `day` of the latest `SimulationTick` the module has received. It is registered with `DeferredStepModule::SetKeySource(*this)` before `DoStart` completes. The module is monotonic by construction: a tick with a smaller day (reload) resets it only through Feature 3's load path, never inside normal flow.

**Ordering hazard (verify in the first task).** `DeferredStepModule::DoUpdate` calls `Tick(GetCurrentKey())` once per sim step, and it is listed *before* this module. The key must therefore already reflect this step's tick when `DeferredStepModule` runs. That holds only if `SimulationTick` is dispatched to this module's handler **before** `DeferredStepModule::DoUpdate` in the same sim step. If the bus delivers into each module's own `DoUpdate`, the key is one step stale. The first plan task proves which, and if it is stale, `GetCurrentKey` instead reads the day directly from GameSpeedController's state through a `ModuleRef` (Open Design Question 1). Either way the key is the game day.

### Cadence

Per `SimulationTick` (one per game day), in this order, on SimPU:

1. **Record** `mLatestDay = tick.day`.
2. **Resync.** If the scheduler reports the `world` channel `kIdle` but `chain.Phase() != Idle`, the scheduler abandoned the step (a load, F3): call `chain.Abandon()` and log Info.
3. **Mode check.** If `deferred == false`, go to the synchronous path.
4. **Configuration check** (first tick after `DoStart` only): any configured id that has never registered, health Failing + Error log naming the ids, no kicks. Re-evaluated every tick, so a late registration clears it.
5. **Readiness.** If `!chain.AllReady()`: no kick, one Info log on the first not-ready tick and one on becoming ready (not per tick).
6. **Retry** a pending week if one is recorded (below).
7. **Week gate.** If `tick.band >= TimeBand::Week` the week is due. The `>=` is mandatory (`ResolveBand` returns only the largest band, so `== Week` skips Season/Year/Decade/Century weeks, which is 4 of 52).
8. **Kick.**

Kick:

```cpp
mStepDay = dayToRepresent;                       // the day this step stands for; stages see it in Capture(key)
result = scheduler.Kick(kChannelId, /*kickKey*/ mLatestDay, /*applyKey*/ mLatestDay + k);
```

`IDeferredStep::Capture()` (called by the scheduler inside `Kick`, on SimPU) calls `chain.Capture(mStepDay)`. A stage's `Capture(key)` therefore always receives the **day the step represents**, which equals the kick day normally and the missed day on a retry, so A1 resolves its due bands from the day it was meant for.

### Retry of a missed week

- `scheduler.Kick` returning `kAlreadyInFlight` for a due week records `mPendingDay = dayToRepresent`, increments `late_kicks`, logs a Warning, and sets health Degraded. One slot only.
- On each following tick, step 6 tries `Kick` for `mPendingDay`. kickKey is the *current* day (the scheduler requires monotonic kick keys), applyKey is `current day + k`, `mStepDay = mPendingDay`. On `kLaunched` the slot clears and an Info log records the delay in days.
- A second due week arriving **while a pending week is still waiting** is an Error, drops the new week (the older keeps its place, so order is preserved), and counts `lost_weeks`. This is the unrecoverable-lag case and should be unreachable at the supported speeds.
- `apply_after_days` is counted from the **actual kick**, so a retried step has a full `k` of slack.
- The system spec's "skipped, not queued" rule is replaced by this one.

### Live mode switch (amended 2026-11-16, S-2.5 Feature 1)

`WorldStepModule::SetDeferred(bool)` switches between the two modes at runtime. It is **refused with a reason** while a step is in flight or a pending week exists, and otherwise takes effect before the next due week. Switching to deferred registers the `world` channel on demand (the module must already depend on `DeferredStepModule`); switching to synchronous unregisters it (idle, so nothing is drained or discarded). Committed state is unchanged by a switch. `cow.worldstep.step` takes `{"action":"kick"|"set_mode","mode":"sync"|"deferred"}`; `kick` is the existing forced kick.

### Synchronous mode (`"deferred": false`)

On a due week with `AllReady()`: `chain.Capture(day)`, `chain.Run(serialExecutor)`, `chain.Commit()`, all inline in the same tick, then the history seam (Feature 3). It never touches the scheduler or the `world` channel, so `DeferredStepModule` need not be a dependency in that configuration (it stays listed; it is idle). There is no pending week and no stall. `late_kicks` is always 0.

The executor is a `SerialExecutor`. A parallel synchronous mode is out of scope: the oracle needs only serial, and the stages' own determinism test already covers range count.

**Deferred equals synchronous** means: after the same number of **applied steps** the stage state is bit-identical. It does not mean at the same game day: a deferred step applies at `day + k`, a synchronous one at `day`. The oracle test compares at apply count.

### `IDeferredStep` on the module

`Capture()` → `chain.Capture(mStepDay)`. `Run(exec)` → `chain.Run(exec)`. `Apply()` → `chain.Commit()`, then updates metrics and the seam for Feature 3's history signal. `SerializePending`/`DeserializePending` are Feature 3; here they are declared and trivially return.

### Stage unregistering while live

- `UnregisterStage` while idle: the chain drops it, the stage becomes retired, `AllReady()` becomes false, kicks stop, **no error**. Health is Degraded with "stage retired" only if the module itself is still running (a stage module can legitimately exit while the host stays up); the Info log names it.
- While a step is in flight the chain queues the removal (Feature 1). The module then asks the scheduler to drain and discard the `world` channel (`Unregister` then `Register` again so the channel stays available), calls `chain.Abandon()`, and applies the removal. No `Commit`, no history signal, per the Lifecycle section.

### Shutdown and re-entry

`DoStop`: unregister the `world` channel (drains, abandons), `chain.Abandon()`, unregister the sim-hosted names, clear `mEverRegistered`, `mPendingDay`, `mLatestDay`. Re-entry rebuilds everything from config in `DoStart`. A leaked channel registration, a stale pending week, or a surviving name is a test failure.

### Observability

- **Names** (sim-hosted through `SimQueryHostModule`, mandatory `SimHelpInfo`, category `worldstep`): `cow.worldstep.status` (query) and `cow.worldstep.step` (command: force one kick now, ignoring the week gate; refused with a reason while a step is in flight, while not ready, or when `deferred` is false and a step would be inline-only, in which case it runs inline). **Registration is all-or-nothing:** if the second `Register` fails, the first is removed in the same call, and `DoStart` reports the failure. A test forces the failure and asserts zero names left.
- **`status` payload:** mode, `k`, enabled stages in order, per-stage ready, retired and missing stage ids, `latestDay`, last kick day and last apply key, pending week or none, in-flight state, last stall ms.
- **Metrics** (`MetricInfo` on each, written on SimPU, SD-PERF-7): `cow.worldstep.step_ms` (total CPU across stages for the last applied step, summed from per-stage profile scopes), `cow.worldstep.slack_days` (apply key minus the day the step was found complete), `cow.worldstep.stalls` (monotonic counter from `SchedulerStats.stalls`), **`cow.worldstep.late_kicks`** (renamed from the system spec's `skipped_kicks`; kicks that had to be retried), plus `cow.worldstep.lost_weeks`.
- **Health:** Failing while a configured stage is missing. Degraded on a stall in the last 16 applied steps, on a retried or lost week in the last 16 applies, or on a retired stage. Healthy otherwise. `IHealthReporter` only.
- **Logs** (`WST`): Info on configure (mode, order, `k`), first ready, first not-ready, retry success with delay, retired stage. Warning on a late kick, an unknown registered stage (once per id), a duplicate registration. Error on a missing configured stage and on a lost week.
- **Trace/profile:** `DIA_TRACE_ZONE` on Capture, Run and Apply; `DIA_PROFILE_SCOPE` per stage inside each (the chain accepts an optional per-stage callback for this so the chain stays free of DiaObservation).

## Public API

New in `Cluiche/CoW/Modules/`: `WorldStepModule.h/.cpp`, `WorldStepMessages` is **not** added (no new bus message; stages publish their own). `cow_*.diaapp` / stage manifests declare instances. Registered type `WorldStepModule` with `kAllowedPUs = Sim`.

## Acceptance Criteria

### Module and manifest
1. `dia validate manifest` accepts a stage declaring `WorldStepModule` with `order`, `apply_after_days`, `deferred`, and rejects an invalid config with the key named (the Feature 1 faults).
2. The module refuses to start on MainPU.
3. A stage that lists `WorldStepModule` after a module that registers with it fails validation or a startup assertion that names both.

### Key source and ordering
4. `GetCurrentKey()` equals the day of the latest `SimulationTick` **as seen by `DeferredStepModule` in the same sim step** (the ordering hazard test; the plan's first task).
5. The key is monotonic across 400 simulated days with several ticks in one frame.

### Cadence
6. Over 52 weeks, with every stage ready, exactly 52 kicks are issued, including the 4 weeks that are also Season/Year/Decade/Century boundaries (not 48).
7. No kick before `AllReady()`; the module logs the not-ready state once, not per tick.
8. A due week while the channel is in flight is not lost: `late_kicks` increments, health is Degraded, the week kicks on the next free tick, with `applyKey = kick day + k`, and a stage's `Capture` receives the **missed** day.
9. A second due week while one is pending is dropped with an Error and `lost_weeks` increments; the pending week still kicks first.
10. The step applies at exactly `kick day + k` even when it finished early, and not before.
11. A step that is still running at its apply key stalls SimPU, then applies; the stall appears in the log, `stalls` and Degraded health.
12. Under simulated catch-up (several sim steps in one real frame) the apply key never slips and applies are more frequent, not larger.
13. A stage's `Capture(key)` receives the same key as every other stage in that step.

### Synchronous mode
14. `deferred: false` runs Capture, Run, Commit inline in the due tick, in declared order, with no scheduler kick and no channel.
15. After the same number of **applied steps**, a deferred run and a synchronous run of two real-shaped fake stages are bit-identical (the determinism oracle), including a stage whose `Run` calls `ParallelFor` several times.

### Missing and retired stages
16. A configured stage that has not registered by the first tick sets health Failing, logs an Error naming it, and blocks kicks; registering it later clears Failing and kicks begin.
17. A registered stage not in `order` is skipped with exactly one Warning.
18. A stage that registered and then unregisters while idle stops kicks with no Error; the Info log names it.
19. A stage unregistered while a step is in flight causes the step to be drained and discarded (no `Commit`, no history seam call), the channel is usable afterwards, and nothing leaks.
20. If the scheduler abandons a step (a load), the next tick calls `chain.Abandon()` so a later kick is accepted.

### Lifecycle
21. Enter, exit, enter again: the second visit rebuilds the chain from config, kicks again, and shows no leaked channel registration, pending week or stale key.
22. `DoStop` with a step in flight abandons it (no `Commit`, no history seam call).

### Commands and observability
23. `cow.worldstep.status` and `cow.worldstep.step` register with non-empty help; a forced failure of the second registration leaves **no** names behind.
24a. `SetDeferred` succeeds when idle and is refused with a reason while a step is in flight or a week is pending; a switch changes no committed state, registers/unregisters the channel with no leak, and the mode-equivalence oracle holds across a switch (amended 2026-11-16).
24. `cow.worldstep.step` forces one kick regardless of the week gate and is refused with a reason while a step is in flight or stages are not ready.
25. The four metrics plus `lost_weeks` are registered with `MetricInfo`, are written only on SimPU, and `step_ms` equals the sum of per-stage CPU for the step.
26. Health transitions: Healthy → Degraded on a late kick or stall → Healthy after 16 clean applies; Failing only for a missing stage.

## Files Touched

New: `WorldStepModule.h/.cpp`; `Tests/GoogleTests/CoW/WorldStep/{TestWorldStepModule,TestWorldStepCadence,TestWorldStepLifecycle}.cpp`; stage manifests for the hosting stages gain an instance (done as each testbed is built). Edited: `CoW.vcxproj(.filters)`, `GoogleTests.vcxproj(.filters)`, `dia module registration`. **System spec edits on approval:** replace the "A kick is skipped (not queued)" cadence line with the retry rule; rename `skipped_kicks` to `late_kicks` and add `lost_weeks`; close Open Design Questions 2 and 3; replace the Public Interfaces `WorldStepChain` block with Feature 1's. No Dia files are added or edited.

## Binding Decisions

| ID | Decision | How this feature complies |
|----|----------|---------------------------|
| PD-002 | ProcessingUnit/Phase/Module architecture | SimModule on SimPU; stages reach it by `ModuleRef`; no singletons |
| PD-004 | No STL in public APIs | Containers are `DynamicArrayC` and fixed-capacity Dia types |
| AD-001 | Ask permission before Dia edits | None. `IStepKeySource`, `Kick`, `Tick` and `SerialExecutor` are used as specified in the Approved DiaDeferredStep |
| AD-002 | Four-tier dependency model | CoW-Shared logic in `Shared/WorldStep/`; the SimModule is `CoW/Modules` |
| DFS-002 | Caller-supplied keys, monotonic | The key is the game day from `SimulationTick`; retries use the current day as kickKey so it stays monotonic |
| DFS-003 | A late step stalls at its apply key; the key never slips | The module never re-times an apply; retry only moves the **kick** later |
| SD-PERF-7 | Telemetry cost budgets | Four gauges and counters, written once per applied step on SimPU; no per-cell telemetry |
| Sim-hosted name cap (64/PU) | S-3.5's real budget risk | Exactly two names, with rollback |

## Test Plan

GoogleTests with a fake `SimulationTick` source, a real `DeferredStepScheduler` over a **synchronous test launcher** (DiaDeferredStep ships one for tests) and two fake stages from Feature 1's `FakeWorldStepStage`; no real job pool except one integration test using the capped executor. TDD order: manifest/config → key source and the ordering hazard → cadence (52 kicks) → retry → apply timing and stall → synchronous mode and the oracle → missing/retired stages → lifecycle → commands and observability. Catch-up and stall tests drive the key directly. Sanitizer: ASan/UBSan over the lifecycle tests (stage unregistered mid-step, re-entry). No mutation checks. Gap audit and edge pass is the last plan task. **Observation scan** at the verify step per the project rule.

## Open Design Questions

1. **Is the key one sim step stale?** It depends on when `SimulationTick` reaches this module relative to `DeferredStepModule::DoUpdate`. If stale, `Tick` applies a step one day late relative to the unaffected design and the oracle would still pass (both modes use the same stale key), which is exactly why this needs an explicit test rather than relying on the oracle. Fallback: read the day from GameSpeedController state by `ModuleRef`. Decide on the evidence of the first plan task.
2. ~~**What day does the history signal carry for a retried step?**~~ **Resolved 2026-11-16 (Feature 3):** the signal carries both `applyKey` and `stepDay`, and the driver labels by `stepDay`. Original question: The signal carries the **apply key** (system spec). For a retried step the apply key is `retry day + k`, so the history label drifts by the delay and `HistoryDriverModule`'s label arithmetic (S-3.5 Feature 1) must treat the key as "the day this snapshot became visible", not "the game week represented". That is consistent with its stated purpose (match the readable state) but the S-3.5 spec says labels come from the key. Confirm in Feature 3 and S-3.5, or pass `mStepDay` as well.
3. **Is Degraded the right health for a retired stage?** On stages that deliberately end a layer early (S-3 Controlled drops weather) a retired stage should be silent. The module cannot tell deliberate from accidental. Current rule: Degraded with an Info log. If that proves noisy in S-3.5, make retirement silent and keep Failing for missing only.

## Gap-pass amendments (2026-11-16)

Found by the plan-writing pass; each item below **supersedes** any contradicting text above. Numbering is stable.

1. **`SetKeySource` timing.** `DeferredStepModule` reads its key source lazily on each `DoUpdate`, so `SetKeySource` may be called **any time before the first `DoUpdate`**; `WorldStepModule` calls it in its own `DoStart` (which runs after `DeferredStepModule`'s `DoStart` and before any `DoUpdate`). The "before `DoStart`" wording here and in DiaDeferredStep Feature 4 is superseded (see that spec's amendment).
2. **`slack_days`** is `applyKey - firstSeenCompleteDay`, where the module polls `scheduler.GetState(world)` at the start of each tick's cadence step and records the day it first sees `kComplete`. No new scheduler API.
3. **`KickResult` handling.** `kLaunched` clears any pending week. `kAlreadyInFlight` is the retry case. `kUnknownChannel`: Error log, re-register the channel and treat the week as lost (`lost_weeks`). `kKeyNotMonotonic` and `kApplyBeforeKick`: Error log, assertion in Debug, treat the week as lost.
4. **Retry and a new due week in the same tick.** Step 6 (retry) runs first. If it launches, the pending slot is empty, so the same tick's new due week then finds the channel in flight, misses, and becomes the new pending week. This is allowed; order is preserved.
5. **`cow.worldstep.step`.** In **synchronous** mode it runs one inline step regardless of the week gate (there is no in-flight state to refuse). In **deferred** mode it forces one kick and is refused with a reason while a step is in flight, a week is pending, or stages are not ready. The contradictory sentence is superseded.
6. **Metrics are five:** `step_ms`, `slack_days`, `stalls`, `late_kicks`, `lost_weeks` ("four metrics plus `lost_weeks`" is superseded).
7. **`GetPendingVersion()`** is implemented by the module and returns the pending record version (`world_version`, 1).
8. **`DeferredStepModule` is a hard dependency in every configuration** (the live Sync/Deferred switch needs it). "Need not be a dependency in synchronous mode" is superseded.
9. **Stage failure after a load** (a stage that cannot restore its pending result) is **Degraded**, not Failing. Failing stays reserved for a missing configured stage.

## Status

**Status:** `Done` (2026-11-20). Approved 2026-11-16. Amended 2026-11-17 (A5 Hydrology): three stage ids in `order`; the k/slack estimate must be re-measured with A5 in (A5's step cost is report-only). The estimate text lives in the system spec's Cadence section. Plan: [module-and-cadence.plan.md](module-and-cadence.plan.md) (written 2026-11-16, not started).
