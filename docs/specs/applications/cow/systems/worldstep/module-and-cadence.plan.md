# Plan: World Step Module & Cadence (World Step Feature 2)

**Spec:** @docs/specs/applications/cow/systems/worldstep/module-and-cadence.md
**Status:** Not Started
**System plan:** @docs/specs/applications/cow/systems/worldstep/worldstep.plan.md (row 2)

**Start condition:** World Step Feature 1 (`step-chain.plan.md`) is Done, and DiaDeferredStep Features 1
(`DeferredStepScheduler`, `SerialExecutor`, `IStepLauncher` and its synchronous test launcher), 2 (capped
executor, used by exactly one integration test) and 4 (`DeferredStepModule`, `IStepKeySource`,
`SetKeySource`, `GetScheduler()`) are built. Feature 3 of DiaDeferredStep is **not** required here; this
feature declares `SerializePending`/`DeserializePending` as trivial stubs and `GetPendingVersion()` as a
constant (the `IDeferredStep` pure virtual the module spec does not list).

**Task 1 is a gate.** It proves whether `SimulationTick` reaches `WorldStepModule` before
`DeferredStepModule::DoUpdate` runs in the same sim step. Tasks 4 onward assume its result; if it fails,
the key-source design switches to the Open Design Question 1 fallback (task 3 variant B). Do not start
task 3 before task 1 is recorded.

No Dia code is edited by this feature (AD-001: not triggered). The one chain addition (optional per-stage
callback, task 17) is CoW Shared code in Feature 1's files.

## Implementation Patterns

### Files
```
Cluiche/CoW/Modules/WorldStepModule.h/.cpp                  the SimModule
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
Members, all `m`-prefixed: `WorldStepConfig mConfig`, `WorldStepChain mChain`, `StepKey mLatestDay`,
`StepKey mStepDay`, `int64_t mPendingDay` (-1 = none), `StageIdList mEverRegistered`,
`DeferredStepScheduler* mScheduler` (borrowed from `ModuleRef<DeferredStepModule>`), `SerialExecutor mSerial`,
`uint32_t mCleanApplies` (window of 16), counters for the five metrics, last-kick/last-apply keys, last stall ms.
Everything is SimPU-only, so there are no locks. Stages hold `ModuleRef<WorldStepModule>`; no singleton.

### Test rig (`WorldStepTestRig.h`)
- Fake `SimulationTick` source: publishes a tick for a given day with `band = ResolveBand(day)` (the real
  `ResolveBand`, so the 4-in-52 skip is exercised against real data), several ticks per "frame" for catch-up.
- A real `DeferredStepScheduler` over DiaDeferredStep's **synchronous test launcher**, a real `WorldStepModule`
  (or the module's logic driven directly where the PU/phase runtime adds nothing), two `FakeWorldStepStage`
  objects from Feature 1. Stall and early-finish tests drive the key directly.
- Only task 18 uses a real job pool and the capped executor.
- Check how `Tests/GoogleTests/CoW/TestGameSpeedControllerModule.cpp` boots a module under test and reuse it.

### Cadence implementation (SimPU, in the spec's step order)
Record -> resync -> mode check -> configuration check -> readiness -> retry -> week gate (`band >= TimeBand::Week`)
-> kick. Write this as one `OnSimulationTick(const SimulationTick&)` with each numbered spec step a small named
private method, so each AC maps to one method and the order is a single readable list. Do not reorder.
`mStepDay` is set immediately before `scheduler.Kick`; `IDeferredStep::Capture()` reads it. The kick key is
always `mLatestDay`; apply key `mLatestDay + k`. The retry never lowers the kick key (monotonic, DFS-002).

### Missing vs retired
`mEverRegistered` is a fixed-capacity id list. Computed each tick, not cached: missing = in `order`, never in
`mEverRegistered`; retired = in `mEverRegistered`, not currently registered. Only missing sets Failing.
One-shot logs use per-id and per-state latches (`mLoggedNotReady`, `mWarnedUnknown[id]`) cleared in `DoStop`.

### Sim-hosted names
Register both names through `SimQueryHostModule` in one helper that records which succeeded and removes
the first if the second fails (rollback inside the same call), then returns false so `DoStart` reports failure.
Test with a forced failure of the second registration (inject a failing registrar or pre-register a colliding name).
`SimHelpInfo` is mandatory and non-empty, category `worldstep`. Payload builders write into fixed-capacity buffers.

### Observability
- Metrics: `cow.worldstep.step_ms`, `slack_days`, `stalls`, `late_kicks`, `lost_weeks`, each with `MetricInfo`,
  written once per applied step on SimPU (SD-PERF-7). `stalls` mirrors `SchedulerStats.stalls`.
- Health: `IHealthReporter` only. Failing = missing stage; Degraded = stall/late/lost in the last 16 applies, or a
  retired stage; Healthy otherwise. The 16-apply window is a small ring of flags, not a timestamp.
- Logs on the `WST` channel at the levels in the spec. Per-id and per-transition latches so nothing logs per tick.
- Trace and profile: `DIA_TRACE_ZONE` in `Capture`, `Run`, `Apply`; per-stage `DIA_PROFILE_SCOPE` through an optional
  chain callback (task 17) so the chain keeps no DiaObservation dependency.

### Verification cadence
- Per task: `dia run googletest --filter="WorldStepModule*:WorldStepCadence*:WorldStepLifecycle*:WorldStepChain*" --shards 2`.
- Kill any leftover `CluicheTest.exe` before E2E-style runs and read the session log, not the summary line
  (feedback_dia_run_gotchas). One full run in the second-to-last task. Sanitizer over the lifecycle tests only.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 0 | Confirm start conditions (Feature 1 Done; DiaDeferredStep Features 1, 2, 4 built; `SetKeySource` and `GetScheduler()` present). Create the module and three test files plus the rig header; register all with `dia docs vcxproj-add`; build `dia run googletest --filter="WorldStep*" --shards 2` | Builds; zero new tests | Not Started | haiku | Mechanical, TDD-exempt. Report BLOCKED if any dependency is missing. Do not hand-edit vcxproj |
| 1 | **GATE: key-source ordering-hazard proof.** Write a test that runs real `GameSpeedControllerModule` (or a fake tick publisher through the real bus), a real `DeferredStepModule` and a recording stub module listed after it, and records, for each sim step, the day the stub saw via `SimulationTick` and the key `DeferredStepModule::DoUpdate` passed to `Tick`. Also record the module start order against `SetKeySource` timing (see Open items 1). Output: a written verdict in Notes (not stale / one step stale) | AC 4 (the proof) | Not Started | sonnet | First plan task per the spec. If stale, record it and switch task 3 to variant B (read the day from GameSpeedController state through a `ModuleRef`). If a Dia change would be needed, STOP and flag (AD-001); do not edit Dia. Ask before escalating |
| 2 | RED then GREEN: `WorldStepModule` skeleton: `kTypeId`, `kAllowedPUs = Sim`, `kDescription`, config load via Feature 1's `LoadWorldStepConfig`, type registration, `dia validate manifest` accepts a stage declaring it and rejects a bad config naming the key, refuses start on MainPU | AC 1, 2 | Not Started | sonnet | Run `dia validate manifest` as part of the task, not only the unit test |
| 3 | RED then GREEN: key source. Variant A (default): implement `IStepKeySource`, `GetCurrentKey() = mLatestDay`, subscribe to `SimulationTick`, `SetKeySource(*this)` at the point task 1 showed is legal. Variant B only if task 1 proved stale | AC 4 (against the real `DeferredStepModule` from task 1's rig), 5 (monotonic over 400 days with several ticks per frame) | Not Started | sonnet | A tick with a smaller day only resets through Feature 3's load path; assert the module itself never lowers the key. Record which variant in Notes |
| 4 | RED then GREEN: manifest ordering validation: a stage that lists `WorldStepModule` after a module that registers with it fails `dia validate manifest` or a startup assertion naming both | AC 3 | Not Started | sonnet | Prefer the existing manifest validator if it already checks dependency order; otherwise a startup assertion. Do not change the validator schema without telling the user |
| 5 | RED then GREEN: stage registration surface: `RegisterStage`/`UnregisterStage` pass-through to the chain, idempotent per id (duplicate false and one log), `mEverRegistered`, missing/retired/unknown computation, one Warning per unknown id | AC 17, 18 (stop kicks without Error, Info names it), registration half of 16 | Not Started | sonnet | Registration only; no kicking yet. Retired = Degraded + Info; missing = Failing + Error wired in task 14 |
| 6 | RED then GREEN: `IDeferredStep` on the module and channel registration with the scheduler: `Capture()` -> `chain.Capture(mStepDay)`, `Run` -> `chain.Run`, `Apply()` -> `chain.Commit()`, `GetPendingVersion()` constant, `SerializePending`/`DeserializePending` stubs. Channel registered `world` in `DoStart` when `deferred`, never in synchronous mode | AC 13 (every stage gets the same key), 21 (partly: registration/unregistration symmetric) | Not Started | sonnet | Stubs are explicitly Feature 3's; leave a one-line TODO naming Feature 3, nothing more |
| 7 | RED then GREEN: cadence: per-tick order, readiness gating, week gate with `>=`, kick with `kickKey = mLatestDay`, `applyKey = mLatestDay + k` | AC 6 (exactly 52 kicks over 52 weeks including the 4 boundary weeks, not 48), 7 (no kick before ready; not-ready logged once and ready logged once) | Not Started | sonnet | Use the real `ResolveBand`. Include a test that asserts 48 would fail |
| 8 | RED then GREEN: apply timing, stall and catch-up | AC 10 (applies at exactly kick day + k, early-finish still waits), 11 (late step stalls SimPU, then applies; log, `stalls`, Degraded), 12 (several sim steps per frame: key never slips, applies more frequent not larger) | Not Started | sonnet | Drive the key directly rather than through the bus. Stall detection relies on `SchedulerStats.stalls` from DiaDeferredStep; do not re-derive it |
| 9 | RED then GREEN: retry of a missed week: `kAlreadyInFlight` records `mPendingDay`, `late_kicks`, Warning, Degraded; retry next tick with current kickKey and `mStepDay = mPendingDay`; second miss while pending is an Error, drops the new week, `lost_weeks` | AC 8 (kicks on next free tick, `applyKey = kick + k`, stage `Capture` receives the **missed** day), 9 | Not Started | sonnet | Retry runs before the week gate each tick (spec step 6 before 7). A retry and a new due week on the same tick: see Open items 4 |
| 10 | RED then GREEN: synchronous mode: `chain.Capture`, `Run(serial)`, `Commit` inline on the due tick, no scheduler, no channel, `late_kicks` always 0, history seam left as an empty private method | AC 14 | Not Started | sonnet | The "seam" is a single no-op private method Feature 3 fills; no listener code here |
| 11 | RED then GREEN: determinism oracle: deferred and synchronous runs of two real-shaped fake stages (one calls `ParallelFor` several times) compared by byte after the same number of **applied steps** (not game days) | AC 15 | Not Started | sonnet | Compare at apply count, per the spec. Include the case where the deferred run has a step in flight at the comparison point |
| 12 | RED then GREEN: `SetDeferred(bool)`: refused with a reason while a step is in flight or a week is pending; on-demand channel registration and unregistration with no leak; committed state unchanged; oracle holds across a switch | AC 24a | Not Started | sonnet | Check the registration count on the scheduler before and after each switch |
| 13 | RED then GREEN: missing-stage Failing and resync: configured stage not registered by the first tick -> Failing + Error naming it + no kicks; late registration clears it; scheduler-abandoned step (`kIdle` but chain not Idle) triggers `chain.Abandon()` and an Info log | AC 16, 20 | Not Started | sonnet | Re-evaluated every tick per spec step 4. Simulate the scheduler abandon with the test launcher; the real trigger is a load (Feature 3) |
| 14 | RED then GREEN: stage unregister while live: idle unregister (Degraded "stage retired" + Info, kicks stop, no Error); mid-step unregister -> scheduler `Unregister` then `Register` the `world` channel (drains, discards), `chain.Abandon()`, removal applied, no `Commit`, no history seam call, channel usable afterwards | AC 18 (idle half), 19 | Not Started | sonnet | Prove no leak: scheduler channel count identical before and after. Under the stage-unregistered-then-destroyed pattern from Feature 1 |
| 15 | RED then GREEN: lifecycle: `DoStop` abandons an in-flight step (no `Commit`, no seam), unregisters the channel and both names, clears `mEverRegistered`, `mPendingDay`, `mLatestDay`; `DoStart` again rebuilds from config | AC 21, 22 | Not Started | sonnet | This is the pattern where the CellLayers E2E flaked, so assert explicitly on every item the spec lists as a leak: channel registration, pending week, stale key, surviving name |
| 16 | RED then GREEN: sim-hosted names with rollback: `cow.worldstep.status` (payload per spec) and `cow.worldstep.step` (force a kick ignoring the week gate; refused with reason when in flight or not ready) | AC 23 (non-empty `SimHelpInfo`; forced second-registration failure leaves zero names), 24 | Not Started | sonnet | The `step` command's behaviour when `deferred == false` is ambiguous in the spec: see Open items 5. Implement the literal reading (runs inline) and note it. Respects the 64-name per-PU cap: exactly two names |
| 17 | RED then GREEN: metrics, health, logs, trace. Per-stage cost: add an **optional** per-stage callback to `WorldStepChain` (CoW Shared edit to Feature 1's files, no Dia edit) used for `DIA_PROFILE_SCOPE` and per-stage CPU summation. `step_ms` equals the sum of per-stage CPU. Health state machine with the 16-apply window. Logs on `WST` per the spec | AC 25 (five metrics registered with `MetricInfo`, written only on SimPU, `step_ms` sum), 26 (Healthy -> Degraded on late kick or stall -> Healthy after 16 clean applies; Failing only for missing) | Not Started | sonnet | Re-run the Feature 1 chain tests after the chain edit. `slack_days` needs the day the step was found complete: Open items 3 |
| 18 | RED then GREEN: one integration test with a real `JobSystem` and the capped executor: two real-shaped stages, 52 weeks, deferred, bit-identical to the serial synchronous run | AC 15 (real-pool variant), 11 or 12 as available | Not Started | sonnet | The only test using a real pool, per the spec's Test Plan. Not a `SLOW_` test unless it exceeds the project's fast-iteration budget. Run it a few hundred iterations once and record the count |
| 19 | Exhaustive test pass: gap audit of every AC (1-26, 24a) against named tests, plus edge cases: tick day 0; `apply_after_days` 1 and 365 against catch-up; `order` of one stage; stage registered after the first tick (late registration); two due weeks in one frame; `SetDeferred` while idle with a retired stage; names collide with an existing registration; double `DoStop` | Gap list in Notes; edge tests green | Not Started | sonnet | No mutation check. Report any AC with no test, do not silently widen the scope |
| 20 | Sanitizer: `dia check sanitizer` scoped to `WorldStepLifecycle*` (stage unregistered mid-step, re-entry) plus `WorldStepChain*` | ASan/UBSan clean | Not Started | haiku | DiaCLI run-and-report only |
| 21 | Observation Opportunity Scan across all touched files and the five DiaObservation pillars (logs, traces, profiling, metrics, health). Suggestions only | Suggestions recorded in Notes | Not Started | sonnet | Does not block. Candidates: per-channel counters on the scheduler side, a drain log on `DoStop` |
| 22 | One full verification run: `dia run googletest --shards 2`, `dia check deps`, `dia validate manifest`; report pass/fail and failing names only. Release perf gates are reported, not enforced | All green | Not Started | haiku | The only unscoped run in this plan. Kill any leftover `CluicheTest.exe` first |
| 23 | Close-out (do **not** run now): `dia docs plan docs/specs/applications/cow/systems/worldstep/module-and-cadence.plan.md <n> --status Done` per row; `dia docs spec-done docs/specs/applications/cow/systems/worldstep/module-and-cadence.md`; update system plan row 2; apply the "system spec edits on approval" from the spec's Files Touched if not already done; commit | Spec Done, plan Done, system plan updated | Not Started | haiku | Listed for the implementer; executes only after task 22 is green |

## Open items

Spec silences and tensions, recorded and not resolved here.

1. **`SetKeySource` timing contradicts the manifest order.** The spec says the key source is registered "before
   `DoStart` completes", and DiaDeferredStep Feature 4 says `SetKeySource` is set "before `DoStart`". But
   `DeferredStepModule` is listed **before** `WorldStepModule`, so its `DoStart` (which constructs the scheduler)
   runs first. Whether the key source may be set after the scheduler exists is not specified. Task 1 must settle it.
2. **The ordering-hazard fallback** (read the day from GameSpeedController state by `ModuleRef`) adds a dependency on
   `GameSpeedControllerModule` that the manifest and `dependencies` list do not declare.
3. **`slack_days`** is "apply key minus the day the step was found complete", but the scheduler's reported API
   (`SchedulerStats`, `Tick`) is not stated to expose when completion was detected. A Dia change would need AD-001;
   the plan flags and does not assume it.
4. **A retry and a new due week on the same tick.** If the retry succeeds and the channel is then busy, the new week
   becomes pending; the spec does not state this explicitly, nor what happens on `kKeyNotMonotonic`, `kNotReady` or
   any `KickResult` other than `kLaunched`/`kAlreadyInFlight`.
5. **`cow.worldstep.step` when `deferred == false`** is worded as "refused ... when `deferred` is false and a step would
   be inline-only, in which case it runs inline", which is self-contradictory. Plan implements "runs inline".
6. **Metrics count:** the spec says "four metrics plus `lost_weeks`" and lists five; plan registers five.
7. **`IDeferredStep::GetPendingVersion()`** is a pure virtual in the Dia spec but is absent from the module spec and
   the system spec's interface block. The plan adds a constant.
8. **The chain's per-stage callback** is mentioned only here ("the chain accepts an optional per-stage callback");
   Feature 1's interface has no such member. Task 17 adds it; Feature 1's spec is not updated by this plan.
9. **Health Degraded for a retired stage** is Open Design Question 3 and may be revisited after S-3.5.
10. **`DeferredStepModule` as a dependency in synchronous mode** is stated both ways ("need not be", "stays listed"); plan
    keeps it listed and idle.
11. **Stale system spec lines** (see `step-chain.plan.md` Open items 7) still read "skipped, not queued" in places until
    the Feature 2 approval edits are applied (task 23).
