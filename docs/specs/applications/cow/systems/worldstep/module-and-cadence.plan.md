# Plan: World Step Module & Cadence (World Step Feature 2)

**Spec:** @docs/specs/applications/cow/systems/worldstep/module-and-cadence.md
**Status:** Not Started
**System plan:** @docs/specs/applications/cow/systems/worldstep/worldstep.plan.md (row 2)

**Start condition:** World Step Feature 1 (`step-chain.plan.md`) is Done, and DiaDeferredStep Features 1
(`DeferredStepScheduler`, `SerialExecutor`, `IStepLauncher` and its synchronous test launcher), 2 (capped
executor, used by exactly one integration test) and 4 (`DeferredStepModule`, `IStepKeySource`,
`SetKeySource`, `GetScheduler()`) are built. Feature 3 of DiaDeferredStep is **not** required here; this
feature declares `SerializePending`/`DeserializePending` as trivial stubs; `GetPendingVersion()` is implemented by the module and returns
the pending record version (`world_version`, 1) (amendment 7).

**Task 1 is a gate.** Per Gap-pass amendment 1 `DeferredStepModule` reads its key source lazily on each
`DoUpdate`, so `SetKeySource` may be called any time before the first `DoUpdate` and `WorldStepModule` calls it in its own
`DoStart`. Task 1 verifies that lazy read and whether `SimulationTick` reaches `WorldStepModule` before
`DeferredStepModule::DoUpdate` in the same sim step. Tasks 4 onward assume its result; if it fails,
the key-source design switches to variant B (task 3), which stays as the fallback. Do not start
task 3 before task 1 is recorded.

**Dia confirmations owed (AD-001, not triggered by this plan's own code):** the lazy `SetKeySource` read and the lazy
save-participant attach are design changes to the unbuilt DiaDeferredStep Feature 4. Signed off by the user 2026-11-16 (AD-001); proceed with the
DiaDeferredStep Feature 4 tasks that implement them. This plan only consumes them (start condition: those Feature 4 tasks are built).

No Dia code is edited by this feature (AD-001: not triggered). The per-stage hook is Feature 1's `IChainObserver`
(`SetObserver`, step-chain.plan.md task 13); the module only implements and installs it (task 17).

## Implementation Patterns

### Files
```
Cluiche/CoW/Modules/WorldStep/WorldStepModule.h/.cpp                  the SimModule
Tests/GoogleTests/CoW/WorldStep/TestWorldStepModule.cpp     registration, key source, commands, observability
Tests/GoogleTests/CoW/WorldStep/TestWorldStepCadence.cpp    cadence, retry, apply timing, stall, catch-up, sync, oracle
Tests/GoogleTests/CoW/WorldStep/TestWorldStepLifecycle.cpp  missing/retired, mid-step unregister, DoStop, re-entry, SetDeferred
Tests/GoogleTests/CoW/WorldStep/WorldStepTestRig.h          shared test rig (see below); in Tests, not shipped
```
Added with `dia docs vcxproj-add` (CoW and GoogleTests, with filters). Module type registered as `WorldStepModule`
with `kAllowedPUs = Sim`, following the registration pattern of the neighbouring CoW SimModules in
`Cluiche/CoW/Modules/` (read `GameSpeedControllerModule` first and copy its shape: `kTypeId`, `kDescription`,
`OnConnectStreams`, config load in the factory).

### Class shape
`class WorldStepModule : public SimModule, public IDeferredStep, public IStepKeySource` (as the spec).
Members, all `m`-prefixed: `StepChainConfig mConfig`, `StepChain mChain`, `StepKey mLatestDay`,
`StepKey mStepDay`, `int64_t mPendingDay` (-1 = none), `int64_t mFirstSeenCompleteDay` (-1 = none), `StageIdList mEverRegistered`,
`DeferredStepScheduler* mScheduler` (borrowed from `ModuleRef<DeferredStepModule>`), `SerialExecutor mSerial`,
`uint32_t mCleanApplies` (window of 16), counters for the five metrics, last-kick/last-apply keys, last stall ms.
Everything is SimPU-only, so there are no locks. Stages hold `ModuleRef<WorldStepModule>`; no singleton.

### Test rig (`WorldStepTestRig.h`)
- Fake `SimulationTick` source: publishes a tick for a given day with `band = ResolveBand(day)` (the real
  `ResolveBand`, so the 4-in-52 skip is exercised against real data), several ticks per "frame" for catch-up.
- A real `DeferredStepScheduler` over DiaDeferredStep's **synchronous test launcher**, a real `WorldStepModule`
  (or the module's logic driven directly where the PU/phase runtime adds nothing), two `FakeStepStage`
  objects from Feature 1. Stall and early-finish tests drive the key directly.
- Only task 18 uses a real job pool and the capped executor.
- Check how `Tests/GoogleTests/CoW/TestGameSpeedControllerModule.cpp` boots a module under test and reuse it.

### Cadence implementation (SimPU, in the spec's step order)
Record -> resync -> mode check -> configuration check -> readiness -> retry -> week gate (`band >= TimeBand::Week`)
-> kick. Write this as one `OnSimulationTick(const SimulationTick&)` with each numbered spec step a small named
private method, so each AC maps to one method and the order is a single readable list. Do not reorder.
`mStepDay` is set immediately before `scheduler.Kick`; `IDeferredStep::Capture()` reads it. The kick key is
always `mLatestDay`; apply key `mLatestDay + k`. The retry never lowers the kick key (monotonic, DFS-002).

**`KickResult` handling (amendment 3):** `kLaunched` clears any pending week. `kAlreadyInFlight` is the retry case (record `mPendingDay`). `kUnknownChannel`: Error log, re-register the channel, week lost (`lost_weeks`). `kKeyNotMonotonic` and `kApplyBeforeKick`: Error log, `DIA_ASSERT` in Debug, week lost.

**Retry plus a new due week in one tick (amendment 4):** step 6 (retry) runs first; if it launches, the pending slot is empty, so the same tick's new due week finds the channel in flight, misses, and becomes the new pending week. Allowed; order is preserved.

**`slack_days` (amendment 2):** `applyKey - firstSeenCompleteDay`; at the start of each tick's cadence step poll `scheduler.GetState(world)` and record the day it first reads `kComplete`. No new scheduler API.

### Missing vs retired
`mEverRegistered` is a fixed-capacity id list. Computed each tick, not cached: missing = in `order`, never in
`mEverRegistered`; retired = in `mEverRegistered`, not currently registered. Only missing sets Failing.
One-shot logs use per-id and per-state latches (`mLoggedNotReady`, `mWarnedUnknown[id]`) cleared in `DoStop`.

### Sim-hosted names
`cow.worldstep.step` (amendment 5): in **synchronous** mode it runs one inline step regardless of the week gate; in **deferred** mode it forces one kick and is refused with a reason while a step is in flight, a week is pending, or stages are not ready.

Register both names through `SimQueryHostModule` in one helper that records which succeeded and removes
the first if the second fails (rollback inside the same call), then returns false so `DoStart` reports failure.
Test with a forced failure of the second registration (inject a failing registrar or pre-register a colliding name).
`SimHelpInfo` is mandatory and non-empty, category `worldstep`. Payload builders write into fixed-capacity buffers.

### Observability
- Metrics (five, amendment 6): `cow.worldstep.step_ms`, `slack_days`, `stalls`, `late_kicks`, `lost_weeks`, each with `MetricInfo`,
  written once per applied step on SimPU (SD-PERF-7). `stalls` mirrors `SchedulerStats.stalls`.
- Health: `IHealthReporter` only. Failing = missing stage; Degraded = stall/late/lost in the last 16 applies, or a
  retired stage; Healthy otherwise. The 16-apply window is a small ring of flags, not a timestamp.
- Logs on the `WST` channel at the levels in the spec. Per-id and per-transition latches so nothing logs per tick.
- Trace and profile: `DIA_TRACE_ZONE` in `Capture`, `Run`, `Apply`; per-stage `DIA_PROFILE_SCOPE` through Feature 1's `IChainObserver` (task 17) so the chain keeps no DiaObservation dependency.
- `DeferredStepModule` is a hard dependency in every configuration, including `deferred == false` (amendment 8). Health: a stage that fails to restore its pending result after a load is **Degraded**, not Failing; Failing is reserved for a missing configured stage (amendment 9).

### Verification cadence
- Per task: narrow `--filter` on the suite under work (e.g. `WorldStepCadence*`) with `--shards 2`. The feature-wide filter `WorldStepModule*:WorldStepCadence*:WorldStepLifecycle*:StepChain*` runs only in task 22 (2026-11-17, speed-ups).
- Kill any leftover `CluicheTest.exe` before E2E-style runs and read the session log, not the summary line
  (feedback_dia_run_gotchas). Full suite runs once, in the Weather-stack close-out (worldstep.plan.md row 6). Sanitizer: on request only (user, 2026-11-17). Any `SLOW_` test runs once in task 22 and once in the close-out; perf is Debug ballpark only (2026-11-17, speed-ups).

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 0 | Confirm start conditions (Feature 1 Done including `IChainObserver`; DiaDeferredStep Features 1, 2, 4 built; `SetKeySource` and `GetScheduler()` present; `DeferredStepModule` listed as a hard dependency in every configuration, amendment 8). Create the module and three test files plus the rig header; register all with `dia docs vcxproj-add`; build `dia run googletest --filter="WorldStep*" --shards 2` | Builds; zero new tests | Done | haiku | Mechanical, TDD-exempt. Report BLOCKED if any dependency is missing. Do not hand-edit vcxproj; 5e51e797d; module tests green Debug+Release |
| 1 | **GATE: key-source ordering-hazard proof.** Write a test that runs real `GameSpeedControllerModule` (or a fake tick publisher through the real bus), a real `DeferredStepModule` and a recording stub module listed after it, and records, for each sim step, the day the stub saw via `SimulationTick` and the key `DeferredStepModule::DoUpdate` passed to `Tick`. Also record the module start order against `SetKeySource` timing (see Open items 1). Also verify the lazy read: `SetKeySource` called from `WorldStepModule::DoStart` (after `DeferredStepModule::DoStart`, before any `DoUpdate`) is honoured on the first `DoUpdate`. Output: a written verdict in Notes (not stale / one step stale) | AC 4 (the proof) | Done | sonnet | First plan task per the spec. Needs the DiaDeferredStep Feature 4 lazy-read amendment built (AD-001: confirm with the user before that Dia task). If stale, record it and switch task 3 to variant B (read the day from GameSpeedController state through a `ModuleRef`). If a Dia change would be needed, STOP and flag (AD-001); do not edit Dia. Ask before escalating; 5e51e797d; module tests green Debug+Release |
| 2 | RED then GREEN: `WorldStepModule` skeleton: `kTypeId`, `kAllowedPUs = Sim`, `kDescription`, config load via Feature 1's `LoadStepChainConfig`, type registration, `dia validate manifest` accepts a stage declaring it and rejects a bad config naming the key, refuses start on MainPU | AC 1, 2 | Done | sonnet | Run `dia validate manifest` as part of the task, not only the unit test; 5e51e797d; module tests green Debug+Release |
| 3 | RED then GREEN: key source. Variant A (default): implement `IStepKeySource`, `GetCurrentKey() = mLatestDay`, subscribe to `SimulationTick`, `SetKeySource(*this)` in `WorldStepModule::DoStart` (any time before the first `DoUpdate`, amendment 1). Variant B stays as the fallback, only if task 1 proved stale or the lazy read cannot be verified | AC 4 (against the real `DeferredStepModule` from task 1's rig), 5 (monotonic over 400 days with several ticks per frame) | Done | sonnet | A tick with a smaller day only resets through Feature 3's load path; assert the module itself never lowers the key. Record which variant in Notes; 5e51e797d; module tests green Debug+Release |
| 4 | RED then GREEN: manifest ordering validation: a stage that lists `WorldStepModule` after a module that registers with it fails `dia validate manifest` or a startup assertion naming both | AC 3 | Done | sonnet | Prefer the existing manifest validator if it already checks dependency order; otherwise a startup assertion. Do not change the validator schema without telling the user. **Amended 2026-11-18:** the validator now enforces `requires_tick_after` (DiaDeferredStep F4; same-`.diaapp` PU lists only, not across manifests composed in a `.diagame`). Use it: in `cow_main.diaapp` give `DeferredStepModule` `"requires_tick_after": ["WorldStepModule"]` and every `world`-channel consumer stage module `"requires_tick_after": ["DeferredStepModule"]`, and add a manifest test that a reversed order fails. This is the CoW half of the old backlog row "Game clock implements `IStepKeySource`" (superseded: the key source is `WorldStepModule`, not the clock). The same task also adds `DeferredStepModule` and `WorldStepModule` to the CoW SimPU list if the placeholder-retirement change has not already added `DeferredStepModule`; Validator tests df92735a3 (AC3). cow_main.diaapp entry deferred to A4 F3 (first stage module): empty order is rejected and a missing stage fails health |
| 5 | RED then GREEN: stage registration surface: `RegisterStage`/`UnregisterStage` pass-through to the chain, idempotent per id (duplicate false and one log), `mEverRegistered`, missing/retired/unknown computation, one Warning per unknown id | AC 17, 18 (stop kicks without Error, Info names it), registration half of 16 | Done | sonnet | Registration only; no kicking yet. Retired = Degraded + Info; missing = Failing + Error wired in task 14; 5e51e797d; module tests green Debug+Release |
| 6 | RED then GREEN: `IDeferredStep` on the module and channel registration with the scheduler: `Capture()` -> `chain.Capture(mStepDay)`, `Run` -> `chain.Run`, `Apply()` -> `chain.Commit()`, `GetPendingVersion()` returns the pending record version `world_version` (1) (amendment 7), `SerializePending`/`DeserializePending` stubs. Channel registered `world` in `DoStart` when `deferred`, never in synchronous mode | AC 13 (every stage gets the same key), 21 (partly: registration/unregistration symmetric) | Done | sonnet | Stubs are explicitly Feature 3's; leave a one-line TODO naming Feature 3, nothing more; 5e51e797d; module tests green Debug+Release |
| 7 | RED then GREEN: cadence: per-tick order, readiness gating, week gate with `>=`, kick with `kickKey = mLatestDay`, `applyKey = mLatestDay + k` | AC 6 (exactly 52 kicks over 52 weeks including the 4 boundary weeks, not 48), 7 (no kick before ready; not-ready logged once and ready logged once) | Done | sonnet | Use the real `ResolveBand`. Include a test that asserts 48 would fail; 5e51e797d; module tests green Debug+Release |
| 8 | RED then GREEN: apply timing, stall and catch-up | AC 10 (applies at exactly kick day + k, early-finish still waits), 11 (late step stalls SimPU, then applies; log, `stalls`, Degraded), `slack_days` = `applyKey - firstSeenCompleteDay` from per-tick `GetState(world)` polling (amendment 2), 12 (several sim steps per frame: key never slips, applies more frequent not larger) | Done | sonnet | Drive the key directly rather than through the bus. Stall detection relies on `SchedulerStats.stalls` from DiaDeferredStep; do not re-derive it. `slack_days` polls `scheduler.GetState(world)` for the first `kComplete`; no new scheduler API; 5e51e797d; module tests green Debug+Release |
| 9 | RED then GREEN: retry of a missed week: `kAlreadyInFlight` records `mPendingDay`, `late_kicks`, Warning, Degraded; retry next tick with current kickKey and `mStepDay = mPendingDay`; second miss while pending is an Error, drops the new week, `lost_weeks`; `KickResult` table (amendment 3): `kLaunched` clears pending, `kUnknownChannel` re-registers and loses the week, `kKeyNotMonotonic`/`kApplyBeforeKick` Error + Debug assert + week lost | AC 8 (kicks on next free tick, `applyKey = kick + k`, stage `Capture` receives the **missed** day), 9, amendment 4 (retry launches and a new due week in the same tick becomes the new pending week, order preserved), amendment 3 (one test per non-launched `KickResult`) | Done | sonnet | Retry runs before the week gate each tick (spec step 6 before 7); 5e51e797d; module tests green Debug+Release |
| 10 | RED then GREEN: synchronous mode: `chain.Capture`, `Run(serial)`, `Commit` inline on the due tick, no scheduler, no channel, `late_kicks` always 0, history seam left as an empty private method | AC 14 | Done | sonnet | The "seam" is a single no-op private method Feature 3 fills; no listener code here; 5e51e797d; module tests green Debug+Release |
| 11 | RED then GREEN: determinism oracle: deferred and synchronous runs of two real-shaped fake stages (one calls `ParallelFor` several times) compared by byte after the same number of **applied steps** (not game days) | AC 15 | Done | sonnet | Compare at apply count, per the spec. Include the case where the deferred run has a step in flight at the comparison point; 5e51e797d; module tests green Debug+Release |
| 12 | RED then GREEN: `SetDeferred(bool)`: refused with a reason while a step is in flight or a week is pending; on-demand channel registration and unregistration with no leak; committed state unchanged; oracle holds across a switch | AC 24a | Done | sonnet | Check the registration count on the scheduler before and after each switch; 5e51e797d; module tests green Debug+Release |
| 13 | RED then GREEN: missing-stage Failing and resync: configured stage not registered by the first tick -> Failing + Error naming it + no kicks; late registration clears it; scheduler-abandoned step (`kIdle` but chain not Idle) triggers `chain.Abandon()` and an Info log | AC 16, 20 | Done | sonnet | Re-evaluated every tick per spec step 4. Simulate the scheduler abandon with the test launcher; the real trigger is a load (Feature 3); 5e51e797d; module tests green Debug+Release |
| 14 | RED then GREEN: stage unregister while live: idle unregister (Degraded "stage retired" + Info, kicks stop, no Error); mid-step unregister -> scheduler `Unregister` then `Register` the `world` channel (drains, discards), `chain.Abandon()`, removal applied, no `Commit`, no history seam call, channel usable afterwards | AC 18 (idle half), 19 | Done | sonnet | Prove no leak: scheduler channel count identical before and after. Under the stage-unregistered-then-destroyed pattern from Feature 1; 5e51e797d; module tests green Debug+Release |
| 15 | RED then GREEN: lifecycle: `DoStop` abandons an in-flight step (no `Commit`, no seam), unregisters the channel and both names, clears `mEverRegistered`, `mPendingDay`, `mLatestDay`; `DoStart` again rebuilds from config | AC 21, 22 | Done | sonnet | This is the pattern where the CellLayers E2E flaked, so assert explicitly on every item the spec lists as a leak: channel registration, pending week, stale key, surviving name; 5e51e797d; module tests green Debug+Release |
| 16 | RED then GREEN: sim-hosted names with rollback: `cow.worldstep.status` (payload per spec) and `cow.worldstep.step` (amendment 5: synchronous mode runs one inline step regardless of the week gate; deferred mode forces one kick and is refused with a reason while a step is in flight, a week is pending, or stages are not ready) | AC 23 (non-empty `SimHelpInfo`; forced second-registration failure leaves zero names), 24 (both `step` modes) | Done | sonnet | Respects the 64-name per-PU cap: exactly two names; 5e51e797d; module tests green Debug+Release |
| 17 | RED then GREEN: metrics, health, logs, trace. Per-stage cost: implement Feature 1's `IChainObserver` in the module and install it with `SetObserver` (no chain edit here), used for `DIA_PROFILE_SCOPE` and per-stage CPU summation. `step_ms` equals the sum of per-stage CPU. Health state machine with the 16-apply window. Logs on `WST` per the spec | AC 25 (five metrics registered with `MetricInfo`, written only on SimPU, `step_ms` sum, `slack_days` from the polled first-complete day), 26 (Healthy -> Degraded on late kick or stall -> Healthy after 16 clean applies; Failing only for a missing configured stage; a stage failing to restore after a load is Degraded, amendment 9) | Done | sonnet | Depends on Feature 1 task 13 (`IChainObserver`). `slack_days` uses the polling in task 8; 5e51e797d; module tests green Debug+Release |
| 18 | RED then GREEN: one integration test with a real `JobSystem` and the capped executor: two real-shaped stages, 52 weeks, deferred, bit-identical to the serial synchronous run | AC 15 (real-pool variant), 11 or 12 as available | Done | sonnet | The only test using a real pool, per the spec's Test Plan. Not a `SLOW_` test unless it exceeds the project's fast-iteration budget. Run it a few hundred iterations once and record the count; 5e51e797d; module tests green Debug+Release |
| 19 | Exhaustive test pass: gap audit of every AC (1-26, 24a) against named tests, plus edge cases: tick day 0; `apply_after_steps` 1 and 365 against catch-up; `order` of one stage; stage registered after the first tick (late registration); two due weeks in one frame; `SetDeferred` while idle with a retired stage; names collide with an existing registration; double `DoStop` | Gap list in Notes; edge tests green | Done | sonnet | No mutation check. Report any AC with no test, do not silently widen the scope; 5e51e797d; module tests green Debug+Release |
| 20 | Removed 2026-11-17 (sanitizer on request only) | n/a | Not Started | haiku | Row kept so numbering stays stable (2026-11-17, speed-ups) |
| 21 | Observation Opportunity Scan across all touched files and the five DiaObservation pillars (logs, traces, profiling, metrics, health). Suggestions only | Suggestions recorded in Notes | Done | sonnet | Does not block. Candidates: per-channel counters on the scheduler side, a drain log on `DoStop`; Scan done with F3 (b9c994482) |
| 22 | Feature verification run: `dia run googletest --filter="WorldStepModule*:WorldStepCadence*:WorldStepLifecycle*:StepChain*" --shards 2` (include any `SLOW_` test of this feature once), `dia check deps`, `dia validate manifest`; report pass/fail and failing names only. Debug perf ballpark only; Release check moved to the Weather-stack close-out (2026-11-17, speed-ups) | All green | Done | haiku | Full suite moved to Weather-stack close-out (worldstep.plan.md row 6). Kill any leftover `CluicheTest.exe` first; Debug 607/610 and Release 606/606 pass, unsharded |
| 23 | Close-out (do **not** run now): `dia docs plan docs/specs/applications/cow/systems/worldstep/module-and-cadence.plan.md <n> --status Done` per row; `dia docs spec-done docs/specs/applications/cow/systems/worldstep/module-and-cadence.md`; update system plan row 2; apply the "system spec edits on approval" from the spec's Files Touched if not already done; commit | Spec Done, plan Done, system plan updated | Not Started | haiku | Listed for the implementer; executes only after task 22 is green |

## Open items

Spec silences and tensions. Resolved items cite the Gap-pass amendments (2026-11-16).

1. **`SetKeySource` timing contradicts the manifest order.** Resolved by amendment 1 (2026-11-16): the key source is read lazily on each `DoUpdate`; `SetKeySource` may be called any time before the first `DoUpdate`, and `WorldStepModule` calls it in its own `DoStart`. Task 1 verifies it; variant B stays as the fallback. The Dia side (DiaDeferredStep Feature 4 lazy read) needs user confirmation first (AD-001).
2. **The ordering-hazard fallback** (read the day from GameSpeedController state by `ModuleRef`) adds a dependency on `GameSpeedControllerModule` that the manifest and `dependencies` list do not declare. Still open; only relevant if task 1 fails.
3. **`slack_days`.** Resolved by amendment 2 (2026-11-16): per-tick polling of `scheduler.GetState(world)`; no new scheduler API.
4. **A retry and a new due week on the same tick; other `KickResult` values.** Resolved by amendments 3 and 4 (2026-11-16).
5. **`cow.worldstep.step` when `deferred == false`.** Resolved by amendment 5 (2026-11-16).
6. **Metrics count.** Resolved by amendment 6 (2026-11-16): five metrics.
7. **`IDeferredStep::GetPendingVersion()`.** Resolved by amendment 7 (2026-11-16): module-implemented, returns `world_version` (1).
8. **The chain's per-stage callback.** Resolved by Feature 1 amendment 2 (2026-11-16): `IChainObserver`, planned in step-chain.plan.md task 13.
9. **Health Degraded for a retired stage** is Open Design Question 3 and may be revisited after S-3.5. Stage failure after a load is Degraded (amendment 9, 2026-11-16).
10. **`DeferredStepModule` as a dependency in synchronous mode.** Resolved by amendment 8 (2026-11-16): a hard dependency in every configuration.
11. **Stale system spec lines** (see `step-chain.plan.md` Open items 7) still read "skipped, not queued" in places until the Feature 2 approval edits are applied (task 23).
