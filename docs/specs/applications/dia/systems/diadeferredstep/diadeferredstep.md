# System Spec: DiaDeferredStep

## Parent Application
@docs/specs/applications/dia/dia.md

**Gameplay Domains:** simulation

## Purpose

DiaDeferredStep runs a slow, stateful simulation step on the job pool **without SimPU waiting for it**, and applies the result at a fixed, deterministic point in sim time.

Today a step that is too heavy for one sim frame has two options, and both are bad:
- **Run it inside one frame.** Even when the work is spread across the job pool, SimPU waits at the end of the frame for it, so the cost still lands on the critical path as a spike.
- **Spread it across frames by hand.** Every system reinvents the bookkeeping, and saves can land mid-step.

DiaDeferredStep works in four parts. Inputs are captured on SimPU at kick key N, the step runs on workers against its own working copy, the result is applied on SimPU at apply key N+k, and if the result isn't ready by then SimPU stalls and reports the stall.

The **key** is a monotonic `uint64_t` supplied by the caller, so Dia knows nothing about game calendars. For example, CoW passes its game-day index.

**First consumer:** the CoW world sim chain, A4 Weather → A1 Ecology → A3 Biome Succession. Its coincident calendar-band frames (week + season + year + …) are budgeted at ≤26 ms of SimPU's 33 ms (30 Hz) frame, and the Century frame has no budget at all. Those systems are amended to use DiaDeferredStep after this system is built.

**Dependency chain:**
`DiaDeferredStep → DiaThreading (JobSystem, TrySubmit), DiaApplicationFlow (SimModule), DiaSimTime (SimTimeContext), DiaSaveGame (ISaveable), DiaObservation (logs, metrics, health, trace), DiaCore (StringCRC, DynamicArrayC)`

## Responsibilities

- Define `IDeferredStep`, the contract a consumer implements: `Capture` on SimPU, `Run` on workers against the working copy only, `Apply` on SimPU, and serialization of a completed result that hasn't been applied yet.
- Own the step lifecycle per channel: Idle → Captured → Running → Complete → Applied. At most one step is in flight per channel.
- Apply deterministically: `Tick(currentKey)` applies every due step whose `applyKey <= currentKey`, in kick order, on SimPU. A result is never applied before its key, even if it finished early.
- **Late policy (DFS-003):** if a step isn't complete when its apply key arrives, SimPU waits for it that frame. The stall is reported through a log, metrics and health.
- Provide `IParallelExecutor` in Dia, moved from the A4 spec. Provide a JobSystem-backed implementation that enforces a **worker cap** (DFS-004): all deferred-step jobs combined occupy at most M workers, and further ranges queue inside DiaDeferredStep. When `TrySubmit` fails, a range runs inline. A `SerialExecutor` is provided for tests and as the fallback.
- **Save rule (DFS-005):** when a save begins, wait for every in-flight step to finish (drain), then serialize each completed but not-yet-applied result together with its apply key. On load, restore those pending results and apply them at their keys.
- Handle shutdown: drain or abandon in-flight steps cleanly, with no `Submit`-after-`Shutdown`.
- Provide `DeferredStepModule`, a SimModule on SimPU. It owns the scheduler, reads the worker cap from `.diaapp` config, and calls `Tick` each sim step after DiaSimTime.
- Observability (all five pillars):
  - Logs: `DIA_LOG_WARNING` on a stall, `DIA_LOG_INFO` on a drain.
  - Metrics: `dia.deferredstep.*` (in flight, stall count, stall ms, run ms per channel, slack keys at completion, cap utilisation).
  - Health: `IHealthReporter` goes Degraded when stalls occur in a rolling window.
  - Trace and profiling: `DIA_TRACE_ZONE` and `DIA_PROFILE_SCOPE` on Capture, Run, Apply and Drain. `Run` scopes are recorded on worker threads with the channel as owner (needs @docs/specs/applications/dia/systems/diathreading/job-observation.md, 2026-11-12).
  - Job attribution (2026-11-12): every range is submitted with `JobTag{owner = channel id, tag = kick key, range, rangeCount}`, so the Perf Inspector and Perfetto can split worker time by channel (for example weather vs ecology).
  - Step summaries (2026-11-12): one record per step instance (channel, kick key, apply key, kick, first range start, ready, apply, stall ms, CPU ms across ranges, cap-wait ms, worker-wait ms, range count, inline fallbacks), written into SimPU's `JobTimingTable` when the step is applied (@docs/specs/applications/dia/systems/diaobservation/perf-inspector.md Revision 2026-11-12, tier 2). **Cap wait** is measured by the capped executor (range queued inside DiaDeferredStep until it is handed to the JobSystem); **worker wait** is the job's submit-to-start time from its `JobTimingRecord`.
- Provide test helpers in `DiaDeferredStep/Testing/`: a manual executor that completes ranges on demand and a scripted key driver.
- Provide `dia.deferredstep.architecture.module.md` and `DiaDeferredStep.vcxproj`, registered in `Cluiche.sln`.

## Non-Responsibilities

- **What a step computes.** Weather, ecology and succession logic stay in CoW.
- **Calendar or band logic.** The caller decides when to kick a step and chooses k, in its own key units.
- **Thread management and scheduling policy in the JobSystem.** The JobSystem stays a plain first-in-first-out queue. The worker cap lives in this system's executor, not in DiaThreading.
- **Ordering between channels.** A chain whose stages depend on each other (A4 → A1 → A3) runs as **one** channel, and the consumer orders the stages inside `Run`.
- **Cross-PU work.** Everything except `Run` happens on SimPU. `Run` touches only its working copy, so no streams are needed.
- **Debug UI.** A `DiaDeferredStepVisualDebugger` debug domain may follow later; the metrics and health signals are enough for v1.

## Design

### Lifecycle (one channel)

```
SimPU, key N          workers                       SimPU, key N+k
Kick ─► Capture() ─► Run(executor) on working copy ─► Tick: Apply()
                     (SimPU keeps simulating)         if not complete: Wait + report stall
```

- `Capture` copies every input the step needs into the working copy. After `Capture` returns, `Run` must not read any SimPU-owned state. This rule is what makes the design thread-safe without locks (DFS-002).
- `Run` must be a pure function of the captured inputs, and its result must be identical at any range count (serial or parallel). A4 already requires this.
- `Apply` swaps or commits the working copy into the committed state that gameplay, DiaCellHistory and debug tools read. Readers only ever see committed state.
- A `Kick` on a channel that already has a step in flight is rejected (it returns false and logs). Consumers choose k so that steps don't overlap. For CoW, k is at most the weekly cadence.

### Public interfaces (sketch; finalised per feature)

```cpp
namespace Dia::DeferredStep {

    using StepKey = uint64_t;

    class IParallelExecutor {
    public:
        virtual ~IParallelExecutor() = default;
        // Calls fn(begin, end) over [0, count) split into ranges; returns when all ranges are done.
        virtual void ParallelFor(uint32_t count, uint32_t minRange,
                                 const RangeFn& fn) = 0;
    };

    class IDeferredStep {
    public:
        virtual ~IDeferredStep() = default;
        virtual void Capture() = 0;                              // SimPU
        virtual void Run(IParallelExecutor& executor) = 0;       // worker thread; working copy only
        virtual void Apply() = 0;                                // SimPU
        virtual void SerializePending(SaveGame::SaveContext& ctx) const = 0;
        virtual void DeserializePending(SaveGame::LoadContext& ctx) = 0;
    };

    enum class StepState { kIdle, kRunning, kComplete };

    class DeferredStepScheduler {
    public:
        void       Register(Core::StringCRC channelId, IDeferredStep& step);
        void       Unregister(Core::StringCRC channelId);       // drains first
        bool       Kick(Core::StringCRC channelId, StepKey kickKey, StepKey applyKey);
        void       Tick(StepKey currentKey);                    // apply due steps; stall if late
        void       DrainAll();                                  // save, shutdown
        StepState  GetState(Core::StringCRC channelId) const;
    };
}
```

### Worker cap

The capped executor counts deferred-step ranges in flight across all channels. With M = 2 on an 8-worker pool, world steps never hold more than 2 workers, so gameplay jobs submitted later wait behind at most 2 world ranges, not the whole step. M is set per app in `.diaapp`. The default is `max(1, workers / 4)`.

## Features

| # | Feature | Notes | Status | Spec |
|---|---|---|---|---|
| 1 | Scheduler core | `IDeferredStep`, `IStepLauncher`, channel lifecycle, `Kick` / `Tick` / `DrainAll`, kick-order apply, late stall, double-kick rejection, `Testing/` helpers. Also ships the `IParallelExecutor` interface + `SerialExecutor` (needed to compile `Run` and the manual launcher). Pure logic, TDD with the manual launcher | Approved | [scheduler-core.md](scheduler-core.md) |
| 2 | Capped executor | `JobStepLauncher`, `CapGate` (strict M, coordinator participates), `CappedParallelExecutor`, `TrySubmit` inline fallback, per-range cap/worker timing, serial-vs-parallel bit-identity tests | Done | [capped-executor.md](capped-executor.md) |
| 3 | Save / load | Automatic drain on `OnSaveStarted`; one `ISaveable` participant holding every channel's pending result + apply key; abandon on `OnLoadStarted`; restore applies at the key; load mid-run equals the uninterrupted run | Done | [save-load.md](save-load.md) |
| 4 | DeferredStepModule + observability | SimModule on SimPU, `JobSystemService` reader, `.diaapp` worker cap, injected `IStepKeySource`, shutdown drain, logs, 9 global metrics, Degraded-only health, trace/profile zones; CluicheTest `DeferredStep` stage | Done | [module-and-observability.md](module-and-observability.md) |
| 5 | Job attribution + step summaries | Part A: `JobTag` per range and coordinator, `JobSystem::TrySubmit(fn, tag)`. Part B: one `StepSummaryRecord` per step instance into SimPU's `JobTimingTable`, with cap wait and worker wait measured inside DiaDeferredStep | Done | [job-attribution.md](job-attribution.md) |

**Build order:** 1 → 2 → 3 → 4 → 5. Features 1 and 2 are independent and can run in parallel if
needed. Feature 5 Part B's final wiring task is the only work blocked on another system
(perf-inspector Task 8, `JobTimingTable`); everything else is buildable today.

**Consumer follow-ups (CoW, not in this system):**
- Amend A4, A1 and A3 to run as one `world` channel.
- Turn S-3.5's spike check into a SimPU-frame gate.
- Implement `IStepKeySource` on CoW's game clock and register it with `DeferredStepModule`.
- Retire `JobSystemPlaceholderConsumerModule` — `DeferredStepModule` becomes the real
  `JobSystemService` consumer.
- Choose k in game days for each speed preset.
- ~~Amend E1's save to call `DrainAll`~~ — dropped 2026-11-13: Feature 3 drains automatically on
  `SaveManager::OnSaveStarted`, so no game-side save change is needed.

## Amendments

**2026-11-13 (feature-spec interview).** Five decisions taken while writing Features 1–5; the
Responsibilities and Design sections above read as originally approved except where noted here.

| # | Amendment |
|---|---|
| A1 | **`IStepLauncher` is the single seam.** The scheduler holds only a launcher, which owns the executor and calls `Run`; the scheduler never sees `IParallelExecutor`, so Feature 1 has no DiaThreading dependency. `Launch` cannot fail — a launcher that cannot go async runs `Run` synchronously and returns a completed token |
| A2 | **`IParallelExecutor` + `SerialExecutor` move from Feature 2 to Feature 1**, because `IDeferredStep::Run` needs the interface to compile and the manual launcher needs an executor to pass in. Feature 2 keeps the capped, JobSystem-backed implementation |
| A3 | **The worker cap is strict and includes coordinators.** DFS-004 is read literally: all deferred-step jobs together hold at most M workers. Deadlock at M = 1 is avoided by the coordinator running ranges itself instead of idling inside `ParallelFor` |
| A4 | **Feature 4 is split into 4 and 5.** `JobTimingTable` is perf-inspector Task 8 and still `Planned`, so job attribution and step summaries became Feature 5. Feature 4 ships the module and four pillars and is buildable today. DiaDeferredStep measures its own cap-wait and worker-wait figures, so Feature 5 depends on `JobTimingTable` only as a destination |
| A5 | **The module reads its key from an injected `IStepKeySource`**, and registers only global metrics — per-channel detail lives in Feature 5's step summaries, so there is one source of truth for it. Health is Degraded-only, never Failing, so a stall cannot fail an E2E run |

Also recorded: the module doc is `dia.diadeferredstep.architecture.module.md`, following the
`dia.dia<module>.` convention DiaCellHistory uses rather than the `dia.deferredstep.` name written
above; and the CluicheTest integration stage ships inside Feature 4.

## Dependencies

| Dependency | Use | Status |
|---|---|---|
| DiaThreading | `JobSystem`, `TrySubmit`, `GetWorkerCount` | Done |
| DiaApplicationFlow | `SimModule`, `.diaapp` module config | Done |
| DiaSimTime | `SimTimeContext`; module ordering after `DiaSimTimeModule` | Done |
| DiaSaveGame | `ISaveable`, `SaveContext` / `LoadContext` | Done |
| DiaObservation | Logs, `MetricRegistry`, `IHealthReporter`, trace and profile macros | Done |
| DiaThreading job-observation | `JobTag` on `Submit`, worker-thread profile registration | Built (2026-11-13); Feature 5 adds a `TrySubmit(fn, tag)` overload |
| DiaObservation `JobTimingTable` | Step summary destination only — DiaDeferredStep measures its own cap-wait and worker-wait figures | perf-inspector Task 8, `Planned`; blocks Feature 5 Part B's final wiring task only |

## Decisions

| ID | Decision | Status | Binding |
|---|---|---|---|
| DFS-001 | A new Dia system and module, `DiaDeferredStep`, rather than a feature of DiaSimTime or DiaThreading. Neither of those gains a dependency on the other (user decision 2026-11-10) | Accepted | Yes |
| DFS-002 | Apply points are caller-supplied monotonic `StepKey`s, never wall-clock or frame counts. `Run` reads only the captured working copy | Accepted | Yes |
| DFS-003 | A late step stalls SimPU at its apply key and is reported. The apply key never slips (user decision 2026-11-10) | Accepted | Yes |
| DFS-004 | Contention with gameplay jobs is controlled by a worker cap in DiaDeferredStep's executor; the JobSystem stays first-in-first-out with no priorities (user decision 2026-11-10) | Accepted | Yes |
| DFS-005 | Saves drain in-flight steps, then store pending results with their apply keys. Steps are not re-run on load (user decision 2026-11-10) | Accepted | Yes |
| DFS-006 | Chosen over a dedicated follower PU ("WorldPU"). A follower PU would need new PU roles, PUs that are stepped by another PU, a shared world clock, cross-PU rewiring of A4/A1/A3 and mirrored debug tooling, and its extra thread would oversubscribe the job pool (user decision 2026-11-10) | Accepted | Yes |

## Inherited Binding Decisions

| ID | Decision | How it constrains DiaDeferredStep |
|---|---|---|
| PD-001 | StringCRC for IDs | Channel ids and metric names are `StringCRC` |
| PD-002 / AD-004 | ProcessingUnit/Phase/Module | The scheduler is owned by `DeferredStepModule` on SimPU. Workers run only `Run`, so no cross-PU state is shared |
| PD-004 / AD-002 | No STL in public APIs | Channel tables use `DynamicArrayC`. `RangeFn` follows the existing `JobFn` (`std::function`) precedent in DiaThreading |
| AD-001 | Module docs; sign-off for engine API changes | New module doc. Moving `IParallelExecutor` from A4 into Dia: sign-off given 2026-11-12 |
| AD-003 | `Dia::<Module>::` namespace | `Dia::DeferredStep::` |
| SD-PERF-7 (perf-inspector) | Telemetry cost budgets and wire rules | Step summaries are one record per step instance, written on SimPU at apply; no per-range records leave SimPU except through the tier 3 raw ring |
| ST-011 (DiaSimTime) | Fixed-timestep accumulator on the Sim PU | `Tick` runs once per drained sim step, so several applies can happen in one real frame during catch-up |

## Test Strategy

- **GoogleTests (Features 1–3), with the manual executor:**
  - A step applies at exactly its apply key, even when it finished early.
  - A late step stalls and emits the warning, metric and health signal.
  - A double kick is rejected.
  - Steps on several channels apply in kick order.
  - `DrainAll` with nothing in flight is a no-op.
  - Unregister drains the channel first.
  - The worker cap is never exceeded (concurrency counter under contention).
  - A failed `TrySubmit` runs the range inline.
  - Serial and parallel runs produce identical results.
  - Save mid-step, then load: the result is equal to the uninterrupted run (pending result applied at the same key).
- **Sanitizer:** a TSan-style race check is unavailable, so ASan/UBSan runs over the scheduler tests, plus an assertion that `Run` never touches SimPU state. A test step poisons its captured source after `Capture`.
- **Observability (Feature 4, 2026-11-12):**
  - A `DIA_PROFILE_SCOPE` inside `Run` appears in `profile.jsonl` on a worker `thread_id` with the channel as subject.
  - Each applied step writes exactly one summary record; cap-wait + worker-wait + run reconcile with first start and ready (within clock resolution).
  - A late step's summary has `stall ms > 0`, and the stall appears on the same Sim entry as the Apply.
  - With M = 1 and two channels kicked together, the second channel's ranges show cap wait > 0 and worker wait ≈ 0 on an idle pool.
- **Integration:** a CluicheTest or CoW stage with a synthetic 50 ms step at k = 7. SimPU frame time stays inside budget, and the step applies on the expected key at every speed.

## Open Design Questions

1. **Is a pending result cheap enough to serialize?** DFS-005 stores completed results, so every consumer's working copy must be serializable. For A4 that is the weather grid (~100 KB at Large, as JSON text), well inside `SaveContext::kBufferSize` (100 MB). If any consumer's result is large or awkward to serialize, the alternative is to apply immediately at drain time. That is simpler, but it changes the outcome versus the uninterrupted run. — *The pre-save hook half of this question is resolved (2026-11-13): `SaveManager::OnSaveStarted` already fires synchronously before any `Serialize`, so Feature 3 drains there and needs no DiaSaveGame change.*
2. **One `world` channel or several?** A1's bands (season, year, decade, century) run at different cadences from A4's weekly step. Putting the whole chain in one channel kicked weekly keeps ordering simple, but the rare Century step makes that one week's `Run` very long. Separate channels per band would need ordering between channels, which is out of scope today.
3. **Choosing k.** A larger k gives more slack but more latency between the world changing and gameplay seeing it. At Fastest, a week is 7 frames (~233 ms); at Slow it is minutes. The choice is CoW's, but it may need to vary with the speed preset (a GameSpeedController amendment).

## Status

`Approved` (2026-11-12), amended 2026-11-13. Interview complete 2026-11-10: home, late policy, save rule and worker cap decided by the user; Feature 4 observability added 2026-11-12. AD-001 sign-off for moving `IParallelExecutor` into Dia given 2026-11-12, and for `JobSystem::TrySubmit(fn, tag)` plus `Dia::Observation::Profile::StepSummaryRecord` given 2026-11-13.

All five feature specs written and `Approved` 2026-11-13. Plans written 2026-11-13; see
[diadeferredstep.plan.md](diadeferredstep.plan.md) for the system tracker and one plan per feature.
Next: `/implement` Feature 1 (scheduler core).
