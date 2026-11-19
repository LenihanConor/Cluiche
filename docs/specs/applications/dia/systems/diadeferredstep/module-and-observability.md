# Feature Spec: DeferredStepModule and Observability

**Parent:** @docs/specs/applications/dia/systems/diadeferredstep/diadeferredstep.md (Feature 4)
**Status:** Done (2026-11-18)
**Plan:** @docs/specs/applications/dia/systems/diadeferredstep/module-and-observability.plan.md
**Depends on:** Features 1, 2 and 3 (scheduler, job launcher, save participant)

## Summary

Everything built so far becomes a module a game can place in its manifest. `DeferredStepModule` is a
`SimModule` on SimPU that consumes `JobSystemService`, resolves the worker cap from `.diaapp`, owns
the launcher, scheduler and save participant, and calls `Tick` once per sim step with the key an
injected `IStepKeySource` reports. Shutdown drains. Four observability pillars land here — logs,
metrics, health and trace/profiling — with per-range job attribution and step summary records
deferred to Feature 5.

A `DeferredStepTestStage` in CluicheTest proves it in a real app before CoW depends on any of it: a
synthetic 50 ms step at k = 7 that must apply on the expected key, keep SimPU inside its frame
budget, and stall when k is dropped to 1.

## Goals

- A game gets deferred steps by adding one module to its manifest and registering a key source.
- Dia stays calendar-free: the module never derives a key, it only reads the one it is given.
- A stall, a cap saturation or a missing job system is visible from the session output without a
  debugger attached.
- The synthetic stage is the thing you run to understand how the system behaves, not just a test.

## Design

### Module shape

```
DeferredStepModule (SimModule, PUAffinity::kSim)
  consumes  ServiceStreamReader<Threading::JobSystem>  "JobSystemService"   (provided on MainPU)
  injected  SetSaveServices(SaveRegistry&, SaveManager&)                     (before Application::Start)
  injected  SetKeySource(IStepKeySource&)                                    (by the consumer's clock module)
  owns      JobStepLauncher ─► CapGate ─► CappedParallelExecutor
            DeferredStepScheduler
            DeferredStepSaveParticipant
            metrics, IHealthReporter
  consumers reach the scheduler with ModuleRef<DeferredStepModule>
```

`ModuleRef` is documented in `ModuleRefV2.h` as the sole inter-module access pattern in v2, and both
the consumer and this module live on SimPU, so no stream is involved and no PU boundary is crossed.
Only `JobSystemService` crosses one, Main → Sim, and it already exists.

### Ordering

`ProcessingUnit` ticks modules in the order they were added, which is manifest order. Two ordering
requirements follow, both expressed in the module doc and asserted where possible:

- `DeferredStepModule` must be listed **after** the module that advances the key source, so a key
  advance is seen in the same sim step it happens.
- Consumer modules that register channels must be listed **after** `DeferredStepModule`, because
  stop runs in reverse order — that way consumers unregister their channels while the scheduler is
  still alive.

`DiaSimTimeModule` drives the fixed-timestep accumulator, so `Tick` may run several times in one real
frame during catch-up (ST-011). Each run gets the key the source reports at that moment, so several
applies can legitimately happen in one frame.

### Key source

```cpp
namespace Dia::DeferredStep {

    // Implemented by the consumer's clock. Dia never interprets the value — it only
    // requires it to be monotonic (DFS-002).
    class IStepKeySource
    {
    public:
        virtual ~IStepKeySource() = default;
        virtual StepKey GetCurrentKey() const = 0;
    };
}
```

### Config

```json
{
  "type": "DeferredStepModule",
  "instance": "DeferredStepModule",
  "config": { "worker_cap": "auto", "ranges_per_worker": 4 }
}
```

`"auto"` resolves to `max(1, workerCount / 4)`; an integer is used as given and clamped to the
worker count.

### Metrics

| Name | Type | Meaning |
|---|---|---|
| `dia.deferredstep.in_flight` | gauge | channels not idle, set each `Tick` |
| `dia.deferredstep.kicks` | counter | accepted kicks |
| `dia.deferredstep.kicks_rejected` | counter | every `KickResult` other than `kLaunched` |
| `dia.deferredstep.applies` | counter | steps applied |
| `dia.deferredstep.stalls` | counter | applies that had to wait |
| `dia.deferredstep.stall_ms` | counter | total milliseconds SimPU spent stalled |
| `dia.deferredstep.slack_keys` | gauge | slack of the most recently applied step |
| `dia.deferredstep.cap_utilisation` | gauge | `slotsHeld / workerCap` at `Tick` time |
| `dia.deferredstep.inline_fallbacks` | counter | ranges that ran inline after a failed `TrySubmit` |

Per-channel breakdown is deliberately not here — Feature 5's step summary records carry channel,
keys, CPU ms, cap wait and worker wait per step instance, and two sources would disagree
(user decision 2026-11-13).

## Acceptance Criteria

### Module lifecycle

1. `DeferredStepModule` is a `SimModule` with `kAllowedPUs = PUAffinity::kSim`, a `kTypeId` of
   `"DeferredStepModule"` and a `kDescription`; it is registered with `TypeRegistry` through the
   standard v2 registration macro and `dia validate manifest` accepts it.
2. `OnConnectStreams` connects a `ServiceStreamReader<Dia::Threading::JobSystem>` named
   `"JobSystemService"`.
3. `DoStart` resolves the job system, constructs the `CapGate` with the configured cap, the
   `JobStepLauncher`, the `DeferredStepScheduler` and the `DeferredStepSaveParticipant`, registers
   the metrics and the health reporter, and returns success.
4. `DoStart` with no `JobSystemService` available logs one Warning and starts anyway with a launcher
   that runs every step inline on SimPU (Feature 2 AC 17). The module is usable, serially.
   *Amended 2026-11-19:* the service can be committed after the module starts (CoW: JobSystemModule is on MainPU), so
   `DoUpdate` keeps checking. The first update where the reader is available and no channel holds a step
   (`DeferredStepScheduler::CanSwapLauncher`) swaps the inline launcher for the job launcher
   (`SetLauncher`) and logs one Info; steps never change launcher mid-flight.
5. `DoStop` calls `DrainAndDiscardAll`, detaches the save participant, unregisters the metrics and the
   health reporter, and asserts in Debug that no channel is still registered — consumers unregister
   in their own `DoStop`, which manifest ordering guarantees runs first.
6. No `Submit` reaches the job system after `DoStop` — the gate is empty and the scheduler has no
   channels, so a late `Kick` is impossible.
7. The module adds no singleton and no file static beyond `kTypeId` (`MetricRegistry::Instance()` and
   `HealthRegistry::Instance()` are pre-existing engine APIs, not new ones).

### Config

8. `"worker_cap": "auto"` resolves to `max(1, jobSystem.GetWorkerCount() / 4)`; on an 8-worker pool
   that is 2.
9. `"worker_cap": 3` resolves to 3. A value above the worker count is clamped and logs one Warning.
   `0`, a negative value or a non-integer, non-`"auto"` value logs one Warning and falls back to
   `"auto"`.
10. `"ranges_per_worker"` defaults to 4 when absent, and a value below 1 is clamped to 1 with one
    Warning.
11. Absent `config` entirely is valid: both keys take their defaults and nothing is logged above Info.

### Key source and tick

12. `SetKeySource` before `DoStart` makes `DoUpdate` call `scheduler.Tick(source.GetCurrentKey())`
    exactly once per sim step.
13. With no key source registered, `DoUpdate` does not call `Tick` and the module logs exactly one
    Warning (at the first update, not every frame).
14. A key source whose value does not change between sim steps produces repeated `Tick` calls at the
    same key, which apply each due step only once (Feature 1 AC 16).
15. During DiaSimTime catch-up, several sim steps in one real frame each produce their own `Tick`, and
    a key that advances across them applies steps in the right order within that frame (ST-011).
16. `GetScheduler()` exposes the scheduler to sibling modules by reference, so a consumer using
    `ModuleRef<DeferredStepModule>` can `Register`, `Kick` and `Unregister`.

### Save services

17. `SetSaveServices(registry, manager)` called before `Application::Start` makes `DoStart` attach
    the save participant, following `DiaSimTimeModule::SetSaveRegistry`'s precedent.
18. Without save services injected, the module starts normally, logs one Info, and simply has no save
    participation — a save then contains no `DeferredStep` section.

### Metrics

19. All nine metrics above are registered on `DoStart` with their documented names, types and
    unit/description info, and appear in a `MetricRegistry::Snapshot`.
20. `in_flight` and `cap_utilisation` are refreshed every `Tick`; the counters are advanced from the
    scheduler's and gate's stats, so no count is derived twice.
21. `stall_ms` and `stalls` move together with the scheduler's stall counters, verified by forcing a
    stall with a 1-key apply on the test stage.
22. Metrics are unregistered on `DoStop`, and a second start/stop cycle re-registers cleanly without
    duplicate-name warnings.

### Health

23. The module registers an `IHealthReporter` named `DeferredStep` whose `Report()` returns `kOK`
    when no stall has happened in the last 60 seconds.
24. A stall flips the report to `kDegraded` with reason `step_stall`, and it returns to `kOK` after
    60 stall-free seconds. It **never** returns `kFailing`, so a stall on a slow machine cannot flip
    an E2E run's `overall_status` to failing (user decision 2026-11-13).
25. `Report()` is thread-safe and allocation-free: it reads an atomic timestamp of the last stall, so
    it is safe from `SessionManager::Tick` on Main and from the crash handler.

### Trace and profiling

26. `DIA_TRACE_ZONE` and `DIA_PROFILE_SCOPE` wrap `Capture`, `Apply`, the stall `Wait`, `DrainAll` and
    the module's `DoUpdate`, each named with the channel where one applies.
27. A profiling session over the test stage shows the stage's `Run` scopes on worker `thread_id`s and
    its `Capture` / `Apply` scopes on the Sim thread — the proof that the work actually left SimPU.
    (Worker-thread profile registration comes from the already-built DiaThreading job observation.)

### CluicheTest stage

28. `DeferredStepTestStage` is scaffolded with `dia scaffold stage DeferredStep --modules DeferredStepModule`
    and runs on SimPU, with the stage module acting as both the `IStepKeySource` and the consumer that
    registers one channel.
29. The synthetic step spins for a configurable duration (default 50 ms) in `Run`, over a cell array
    split through the executor, and writes a checksum into its working copy that `Apply` commits — so
    correctness is checked from committed data, not from pixels.
30. At k = 7 with a key advancing once per sim step: the step applies on exactly kick key + 7, zero
    stalls are recorded, and SimPU's frame time stays inside the stage budget. Verified from the
    session's metrics snapshot and a committed-state dump, not a screenshot.
31. At k = 1 the same step stalls: `stalls > 0`, `stall_ms > 0`, health reads `kDegraded`, and the
    applied value is still correct and still on the expected key — a stall degrades frame time, never
    correctness.
32. With `worker_cap` forced to 1, the stage still completes every step (Feature 2 AC 11 in a real
    app) and `cap_utilisation` reports 1.0 while a step is in flight.
33. An E2E test in the existing harness runs the stage headless and asserts on the session output:
    no error-level log records, `applies` equal to the expected count, `stalls == 0` at k = 7.

### Observation

34. Logs, channel `DeferredStep`: Info on start with the resolved cap, ranges-per-worker and worker
    count; Info on stop with lifetime kick/apply/stall counts; Warning for each config fallback,
    the missing key source and the missing job system. Nothing is logged per frame.

## Files Touched

| File | Change |
|---|---|
| `Dia/DiaDeferredStep/IStepKeySource.h` | New |
| `Dia/DiaDeferredStep/DeferredStepModule.h/.cpp` | New — the SimModule |
| `Dia/DiaDeferredStep/DeferredStepMetrics.h/.cpp` | New — the nine registrations and their update |
| `Dia/DiaDeferredStep/DeferredStepHealth.h/.cpp` | New — the rolling-window reporter |
| `Dia/DiaDeferredStep/DiaDeferredStep.vcxproj(.filters)` | New files (`dia docs vcxproj-add`); add DiaApplicationFlow, DiaStreams, DiaObservation references |
| `Dia/DiaDeferredStep/dia.diadeferredstep.architecture.module.md` | Add the remaining `dependent_modules`; document both manifest ordering requirements (`dia check deps`) |
| `Cluiche/CluicheTest/Modules/TestStages/DeferredStepTestStageModule.h/.cpp` | New — stage, key source, synthetic step |
| `Cluiche/CluicheTest/CluicheTest.vcxproj(.filters)` | New stage files |
| `Cluiche/Assets/CluicheTest/.../cluiche_main.diaapp` | Stage registration plus `DeferredStepModule`, ordered after the stage's key advance and before the stage's channel registration per the Design section |
| `Cluiche/Tests/GoogleTests/DiaDeferredStep/TestDeferredStepModule.cpp` | New |
| `Cluiche/Tests/GoogleTests/DiaDeferredStep/TestDeferredStepMetricsHealth.cpp` | New |
| `Dia/DiaCLI/tests/e2e/test_deferred_step_stage.py` | New — AC 33 |
| `docs/reference/registry/module-registry.md` | `dia docs registry` |

## Binding Decisions

| ID | Decision | How this feature complies |
|---|---|---|
| PD-002 / AD-004 | ProcessingUnit / Phase / Module | `DeferredStepModule` is a `SimModule` on SimPU; workers only run `Run` |
| DFS-001 | A system of its own, with neither DiaSimTime nor DiaThreading gaining a dependency on the other | The module depends on both; neither depends on it, and nothing was added to either |
| DFS-002 | Caller-supplied keys | `IStepKeySource` hands the key over; Dia never computes one (user decision 2026-11-13) |
| DFS-004 | Cap read from `.diaapp` | AC 8–11 |
| DFS-005 | Saves drain | The module attaches Feature 3's participant (AC 17) |
| ST-011 (DiaSimTime) | Fixed-timestep accumulator on SimPU | AC 15: `Tick` runs per drained sim step, so several applies can land in one real frame |
| PU-boundaries-are-streams | No mutex-guarded cross-PU reads | `JobSystemService` is a ServiceStream; consumer access is `ModuleRef`, same PU; health reads one atomic |
| no-singletons | No new statics | AC 7 |
| game-logic-on-SimPU | Game and test logic runs on the sim thread | The stage module is a SimPU module; only `Run` leaves the thread, against its own working copy |
| cell-data-over-pixels | Verify world-sim work with dumps and stats, not screenshots | AC 29–31 assert on committed data, metrics and health |
| AD-001 | Sign-off for engine API changes | New files inside this system's module plus one manifest entry per app; no change to another Dia module's public API |

## Test Plan

- **GoogleTests** (`--filter=DeferredStepModule*`, `--shards 2`), driving the module through a test
  `ProcessingUnit` the way other module tests do:
  - lifecycle: start with and without `JobSystemService`, stop drains, double start/stop
  - config: `"auto"`, explicit, over-cap, zero, garbage, missing `config`, `ranges_per_worker` bounds
  - key source: present, absent (one warning only, checked over 100 updates), unchanged key, catch-up
  - save services: injected and not injected
  - metrics: all nine present after start, absent after stop, values tracking a forced stall
  - health: OK, Degraded on stall, recovery after the window, `Report()` from a second thread
- **Stage and E2E:** `dia run cluichetest --stage DeferredStep` for AC 30–32, then the pytest E2E for
  AC 33. Leftover `CluicheTest.exe` processes are killed before each run (known gotcha); the E2E
  harness does not build, so the stage is built first.
- **Verification cadence:** cheap scoped runs while iterating; one full `dia run googletest --shards 2`
  plus `dia check sanitizer` plus a Release run in this feature's final task, covering Features 1–4
  together.
- **Observation opportunity scan** at the Verify step, across all five pillars, reported in the plan
  notes.
- **Gap audit** as the final task: every config fallback branch, every lifecycle path and every metric
  has a test.

## Open Design Questions

1. **Nothing enforces the manifest ordering requirements.** `ProcessingUnit` ticks in array order, so
   a game that lists `DeferredStepModule` before its clock module silently ticks on a stale key, and
   one that lists it after its consumers unregisters channels against a stopped scheduler. The module
   doc states both rules and `DoStart` logs what it can see, but a real fix is a `dia validate manifest`
   rule (for example "a module declaring `requires_tick_after` must appear later in the list"). Worth
   a backlog row rather than inventing a one-off check here.
2. **Is a 60-second health window right for a day-scale key?** At Slow speed a game day is minutes,
   so a single stall marks the system Degraded for 60 seconds and then clears even though only one
   step ran in that time. A window counted in **keys** rather than seconds (for example "a stall in
   the last 8 applies") would track the simulation rather than the wall clock. Seconds match how
   every other Dia health reporter works, so it stays seconds for v1.
3. **Should the stage's synthetic step be configurable from the console at runtime?** Changing spin
   duration and k live is exactly how you would build intuition for choosing k in CoW
   (testbeds-drive-new-systems), and DiaConsole's command model exists. It is extra scope for a
   feature already carrying a stage, so v1 reads both from stage config; live control is a candidate
   follow-up.

## Resolved Decisions

1. **Injected `IStepKeySource`** (user decision 2026-11-13). The module ticks itself, so the tick
   point and ordering are the engine's business and cannot be forgotten by a consumer, while the key
   itself stays entirely the game's.
2. **Global metrics only** (user decision 2026-11-13). Per-channel detail lives in Feature 5's step
   summaries, so there is one source of truth for it.
3. **Degraded only, never Failing** (user decision 2026-11-13). A stall is a performance signal, not
   a correctness failure, and must not fail an E2E run for machine-speed reasons.
4. **The integration stage ships with this feature** (user decision 2026-11-13), so the module is
   proven in a real app before CoW's A4/A1/A3 amendment depends on it.

## Gap-pass amendments (2026-11-16)

Found by the plan-writing pass; each item below **supersedes** any contradicting text above. Numbering is stable.

1. **`SetKeySource` timing (from World Step Feature 2).** `SetKeySource` may be called any time **before the first `DoUpdate`**; `DoUpdate` reads the source each step. "Before `DoStart`" is superseded. A key source registered after the first `DoUpdate` is still accepted but the module logs the one-time "no key source" Warning until it is.
2. **Lazy attach of the save participant (from World Step Feature 3).** `DeferredStepModule` calls `DeferredStepSaveParticipant::Attach` on its **first `DoUpdate`**, not in `DoStart`, so the participant is registered after every other module's participants and deserializes last. If no `SaveLoadModule`/registry is injected, nothing is attached (unchanged).
3. **AD-001: signed off by the user 2026-11-16** for both changes (lazy `SetKeySource` read and lazy save-participant attach); and, in the same sign-off, the additive `SimQueryHost::Execute` that S-2.5 Feature 4 may need.

