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
| 1 | Scheduler core | `IDeferredStep`, channel lifecycle, `Kick` / `Tick` / `DrainAll`, kick-order apply, late stall, double-kick rejection, `Testing/` helpers. Pure logic, TDD with the manual executor | Draft (not written) | — |
| 2 | Capped executor | `IParallelExecutor` + `SerialExecutor` moved into Dia; JobSystem-backed capped executor; `TrySubmit` inline fallback; serial-vs-parallel bit-identity tests | Draft (not written) | — |
| 3 | Save / load | Drain on save; pending result + apply key serialized via `ISaveable`; restore applies at the key; load mid-run equals the uninterrupted run | Draft (not written) | — |
| 4 | DeferredStepModule + observability | SimModule on SimPU, `.diaapp` worker cap, tick after DiaSimTime, shutdown drain, logs, metrics, health, trace zones; `JobTag` per range, step summary records, cap-wait measurement (2026-11-12) | Draft (not written) | — |

**Build order:** 1 → 2 → 3 → 4. Features 1 and 2 are independent and can run in parallel if needed.

**Consumer follow-ups (CoW, not in this system):**
- Amend A4, A1 and A3 to run as one `world` channel.
- Turn S-3.5's spike check into a SimPU-frame gate.
- Amend E1's save to call `DrainAll`.
- Choose k in game days for each speed preset.

## Dependencies

| Dependency | Use | Status |
|---|---|---|
| DiaThreading | `JobSystem`, `TrySubmit`, `GetWorkerCount` | Done |
| DiaApplicationFlow | `SimModule`, `.diaapp` module config | Done |
| DiaSimTime | `SimTimeContext`; module ordering after `DiaSimTimeModule` | Done |
| DiaSaveGame | `ISaveable`, `SaveContext` / `LoadContext` | Done |
| DiaObservation | Logs, `MetricRegistry`, `IHealthReporter`, trace and profile macros | Done |
| DiaThreading job-observation | `JobTag` on `Submit`, worker-thread profile registration, `JobTimingRecord` (Feature 4) | Approved 2026-11-12; needed by Feature 4 only |
| DiaObservation `JobTimingTable` | Step summary records (perf-inspector Revision 2026-11-12) | Approved 2026-11-12; needed by Feature 4 only |

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

1. **Is a pending result cheap enough to serialize?** DFS-005 stores completed results, so every consumer's working copy must be serializable. For A4 that is the weather grid (~100 KB at Large, as JSON text). If any consumer's result is large or awkward to serialize, the alternative is to apply immediately at drain time. That is simpler, but it changes the outcome versus the uninterrupted run. Also, `ISaveable::Serialize` is `const`, so the drain has to happen in a pre-save hook, not inside `Serialize`.
2. **One `world` channel or several?** A1's bands (season, year, decade, century) run at different cadences from A4's weekly step. Putting the whole chain in one channel kicked weekly keeps ordering simple, but the rare Century step makes that one week's `Run` very long. Separate channels per band would need ordering between channels, which is out of scope today.
3. **Choosing k.** A larger k gives more slack but more latency between the world changing and gameplay seeing it. At Fastest, a week is 7 frames (~233 ms); at Slow it is minutes. The choice is CoW's, but it may need to vary with the speed preset (a GameSpeedController amendment).

## Status

`Approved` (2026-11-12). Interview complete 2026-11-10: home, late policy, save rule and worker cap decided by the user; Feature 4 observability added 2026-11-12. AD-001 sign-off for moving `IParallelExecutor` into Dia given 2026-11-12. Next: `/spec-feature` for Feature 1.
