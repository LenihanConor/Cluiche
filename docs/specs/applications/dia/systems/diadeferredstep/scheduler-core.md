# Feature Spec: Scheduler Core

**Parent:** @docs/specs/applications/dia/systems/diadeferredstep/diadeferredstep.md (Feature 1)
**Status:** Done (2026-11-17)
**Plan:** @docs/specs/applications/dia/systems/diadeferredstep/scheduler-core.plan.md

## Summary

The pure-logic heart of DiaDeferredStep: the `IDeferredStep` contract, the `IStepLauncher` seam that
gets `Run` off SimPU, and `DeferredStepScheduler`, which owns one step per channel through
Idle → Running → Complete → Applied. `Kick(channel, kickKey, applyKey)` captures inputs on SimPU and
launches; `Tick(currentKey)` applies every due step in kick order; a step that isn't complete at its
apply key stalls SimPU and the stall is counted. No threads, no job system, no config, no module —
everything is driven through `IStepLauncher`, so the whole feature is TDD'd against a manual launcher
that completes steps on demand.

## Goals

- A consumer can express "run this heavy step starting at key N, apply it at key N+k" in three calls.
- The apply point is exact and deterministic: never before `applyKey`, never silently skipped, never
  out of kick order.
- A late step is a reported stall, not a corrupted state.
- Feature 1 has no dependency on DiaThreading, DiaApplicationFlow or DiaSaveGame, so the lifecycle is
  testable with no threads and no timing races.

## Design

### The launch seam

The scheduler holds exactly one seam, `IStepLauncher` (user decision 2026-11-13). The launcher owns
the `IParallelExecutor` it hands to `Run`, so the scheduler never sees an executor and never names
DiaThreading:

```
SimPU, key N                        launcher's thread                SimPU, key N+k
Kick ─► Capture() ─► Launch(step) ─► step.Run(executor) ─────────────► Tick: Apply()
                                     (SimPU keeps simulating)          if !IsComplete: Wait + count stall
```

`Launch` never fails. A launcher that cannot go asynchronous (no job system, a failed submit, a
shut-down pool) runs `Run` synchronously on the calling thread and returns a token that reads
complete immediately. The scheduler therefore has no fallback path of its own — it only ever asks
"is this token complete yet?".

### Why `IParallelExecutor` lands here

`IDeferredStep::Run(IParallelExecutor&)` cannot compile without the interface, and the manual
launcher needs something to pass into `Run`. So this feature ships the pure `IParallelExecutor`
interface plus `SerialExecutor` (one range, no threads). Feature 2 adds the capped, JobSystem-backed
implementation. This is a small boundary change against the system spec's feature table, which put
both in Feature 2.

### States

Externally a channel is `kIdle`, `kRunning` or `kComplete`. `Capture` runs synchronously inside
`Kick`, so "Captured" is never observable, and "Applied" returns the channel to `kIdle`.

## Public API

### `Dia/DiaDeferredStep/DeferredStepTypes.h`

```cpp
namespace Dia::DeferredStep {

    using StepKey = uint64_t;

    // Opaque handle to one launched step. 0 is invalid.
    struct LaunchToken
    {
        uint32_t value = 0;
        bool IsValid() const { return value != 0; }
    };

    // Attribution passed to the launcher; Feature 5 turns this into a JobTag.
    struct LaunchInfo
    {
        Dia::Core::StringCRC channelId;
        StepKey              kickKey = 0;
    };

    enum class StepState : uint8_t { kIdle, kRunning, kComplete };

    enum class KickResult : uint8_t
    {
        kLaunched,
        kUnknownChannel,     // no step registered on that channel
        kAlreadyInFlight,    // one step per channel (system spec Design)
        kKeyNotMonotonic,    // kickKey < this channel's previous kickKey
        kApplyBeforeKick,    // applyKey < kickKey
    };
}
```

### `Dia/DiaDeferredStep/IParallelExecutor.h`

```cpp
namespace Dia::DeferredStep {

    // fn(begin, end) over a half-open sub-range of [0, count).
    using RangeFn = std::function<void(uint32_t begin, uint32_t end)>;

    class IParallelExecutor
    {
    public:
        virtual ~IParallelExecutor() = default;

        // Splits [0, count) into ranges of at least minRange and calls fn on each.
        // Returns only when every range has finished. May run ranges on the calling
        // thread. Implementations must not reorder the observable result: callers
        // guarantee ranges are independent (system spec Design, Run purity).
        virtual void ParallelFor(uint32_t count, uint32_t minRange, const RangeFn& fn) = 0;

        // Ranges the last ParallelFor split into; for tests and metrics.
        virtual uint32_t GetLastRangeCount() const = 0;
    };

    // One range, on the calling thread. Tests, and the fallback when no job system exists.
    class SerialExecutor final : public IParallelExecutor
    {
    public:
        void     ParallelFor(uint32_t count, uint32_t minRange, const RangeFn& fn) override;
        uint32_t GetLastRangeCount() const override;
    };
}
```

### `Dia/DiaDeferredStep/IDeferredStep.h`

```cpp
namespace Dia::DeferredStep {

    class IDeferredStep
    {
    public:
        virtual ~IDeferredStep() = default;

        // SimPU, inside Kick. Copy every input the step needs into the working copy.
        // After this returns, Run must not read any SimPU-owned state (DFS-002).
        virtual void Capture() = 0;

        // Launcher's thread. Pure function of the captured inputs; touches the working
        // copy only. The result must be identical at any range count.
        virtual void Run(IParallelExecutor& executor) = 0;

        // SimPU, at the apply key. Commit the working copy into the state readers see.
        virtual void Apply() = 0;

        // Feature 3. A completed-but-unapplied result; version is the step's own.
        // Feature 3 amendment (2026-11-16, World Step Feature 3). Called by the save participant's
        // Validate for each saved channel; must not mutate anything. false aborts the load.
        virtual bool     ValidatePending(Dia::SaveGame::LoadContext& ctx) const { return true; }
        virtual uint32_t GetPendingVersion() const = 0;
        virtual void     SerializePending(Dia::SaveGame::SaveContext& ctx) const = 0;
        virtual void     DeserializePending(Dia::SaveGame::LoadContext& ctx) = 0;
    };
}
```

### `Dia/DiaDeferredStep/IStepLauncher.h`

```cpp
namespace Dia::DeferredStep {

    class IStepLauncher
    {
    public:
        virtual ~IStepLauncher() = default;

        // Must always return a valid token. A launcher that cannot go async runs
        // step.Run(...) synchronously and returns an already-complete token.
        virtual LaunchToken Launch(IDeferredStep& step, const LaunchInfo& info) = 0;

        // Cheap, non-blocking, callable every Tick.
        virtual bool IsComplete(LaunchToken token) const = 0;

        // Blocks until the step's Run has returned.
        virtual void Wait(LaunchToken token) = 0;

        // The scheduler is finished with the token; the launcher may recycle it.
        virtual void Release(LaunchToken token) = 0;
    };
}
```

### `Dia/DiaDeferredStep/DeferredStepScheduler.h`

```cpp
namespace Dia::DeferredStep {

    struct ChannelStats
    {
        uint64_t kicks            = 0;
        uint64_t kicksRejected    = 0;
        uint64_t applies          = 0;
        uint64_t stalls           = 0;
        uint64_t lastRunMicros    = 0;   // Kick -> completion first observed, on SimPU's clock
        uint64_t lastStallMicros  = 0;
        uint64_t totalStallMicros = 0;
        int64_t  lastSlackKeys    = 0;   // applyKey - key at which completion was first seen
    };

    struct SchedulerStats
    {
        uint32_t inFlight         = 0;
        uint64_t kicks            = 0;
        uint64_t kicksRejected    = 0;
        uint64_t applies          = 0;
        uint64_t stalls           = 0;
        uint64_t totalStallMicros = 0;
        uint64_t drains           = 0;
        uint64_t drainedSteps     = 0;
    };

    class DeferredStepScheduler
    {
    public:
        static const unsigned int kMaxChannels = 8;

        explicit DeferredStepScheduler(IStepLauncher& launcher);
        ~DeferredStepScheduler();              // DrainAll, then asserts no channel is registered

        bool Register  (Dia::Core::StringCRC channelId, IDeferredStep& step);
        void Unregister(Dia::Core::StringCRC channelId);   // drains that channel, discards unapplied

        KickResult Kick(Dia::Core::StringCRC channelId, StepKey kickKey, StepKey applyKey);

        // SimPU, once per drained sim step. Polls completion, then applies every step whose
        // applyKey <= currentKey in kick order. Stalls on any that is not complete.
        void Tick(StepKey currentKey);

        // Waits for every in-flight step. Does NOT apply (Feature 3 relies on this).
        void DrainAll();

        StepState GetState       (Dia::Core::StringCRC channelId) const;
        bool      GetApplyKey    (Dia::Core::StringCRC channelId, StepKey& out) const;
        uint32_t  GetChannelCount() const;

        const SchedulerStats& GetStats() const;
        bool  GetChannelStats(Dia::Core::StringCRC channelId, ChannelStats& out) const;
    };
}
```

## Acceptance Criteria

### Registration

1. `Register` returns true for a free channel id and makes `GetState` return `kIdle`; it returns
   false (and logs a Warning) on a duplicate id or when `kMaxChannels` (8) are already registered.
2. `Unregister` on an idle channel removes it; `GetState` then reports `kIdle` and `GetChannelCount`
   drops by one.
3. `Unregister` on a channel with a step in flight waits for `Run` to return before removing it, so
   no worker can touch a destroyed step. The result is discarded without `Apply`, and a Warning is
   logged when the discarded result had completed.
4. The destructor drains every in-flight step. Destroying a scheduler that still has registered
   channels asserts in Debug (consumers unregister in `DoStop`).

### Kick

5. `Kick` on a registered idle channel calls `Capture()` exactly once, then `Launch` exactly once
   with `LaunchInfo{channelId, kickKey}`, and returns `kLaunched`. `GetState` becomes `kRunning`.
6. `Capture` is called before `Launch`, always on the calling (SimPU) thread.
7. A second `Kick` on a channel whose state is not `kIdle` returns `kAlreadyInFlight`, does not call
   `Capture` or `Launch`, logs a Warning, and increments `kicksRejected`.
8. `Kick` with `kickKey` lower than that channel's previous `kickKey` returns `kKeyNotMonotonic` and
   does nothing (DFS-002: keys are monotonic).
9. `Kick` with `applyKey < kickKey` returns `kApplyBeforeKick` and does nothing. `applyKey == kickKey`
   is accepted and documented as "synchronous: it will stall at the next `Tick`".
10. `Kick` on an unregistered channel returns `kUnknownChannel` and does nothing.
11. A `Launch` that returns an already-complete token is fine: the state is `kRunning`, the next
    `Tick` observes completion, and the step still applies at `applyKey`, never earlier.

### Tick and apply

12. `Tick(currentKey)` with `currentKey < applyKey` does not call `Apply`, even when the step has
    completed. A step is never applied early.
13. `Tick(currentKey)` with `currentKey >= applyKey` and a complete step calls `Apply()` exactly
    once, then `Release(token)`, and returns the channel to `kIdle`.
14. `Tick` applies every due step in **kick order** — the order `Kick` was called in, not channel
    registration order and not apply-key order. Two channels kicked in order A then B with the same
    apply key apply A then B.
15. A step whose `applyKey` is several keys in the past (because `Tick` was called with a jump, or
    several sim steps drained in one real frame) still applies, on the first `Tick` at or after its
    key. `Tick` never skips a due step.
16. `Tick` called twice with the same `currentKey` applies each due step only once.
17. `Tick` with a `currentKey` lower than the previous call logs a Warning, asserts in Debug and
    returns without applying anything.
18. `Tick` on a scheduler with no registered channels, or with all channels idle, does no work and
    allocates nothing.

### Stall

19. `Tick(currentKey >= applyKey)` on an incomplete step calls `Wait(token)`, then `Apply()`, in that
    order, on the calling thread.
20. A stall increments `stalls` on both `SchedulerStats` and the channel, and records
    `lastStallMicros` / `totalStallMicros` measured around the `Wait`.
21. A stall logs one `DIA_LOG_WARNING` naming the channel, the apply key and the stall duration. A
    step that was already complete logs nothing.
22. No stall is recorded when the step completed before its apply key, however narrowly.

### Slack and run time

23. `Tick` polls `IsComplete` for in-flight channels that are not yet due, and records the first key
    at which completion was observed. `lastSlackKeys` is `applyKey - thatKey` (0 when completion is
    first seen on the apply key itself, negative never — a late step records its slack as 0 and a
    stall instead).
24. `lastRunMicros` spans `Kick` to the first observation of completion. It is documented as an
    upper bound measured on SimPU, not the step's CPU time — Feature 5 provides the real CPU figure.

### Drain

25. `DrainAll` with nothing in flight is a no-op: no `Wait`, no log, `drains` unchanged.
26. `DrainAll` with steps in flight calls `Wait` on each, leaves each channel in `kComplete` with its
    `applyKey` intact, and calls neither `Apply` nor `Release`. It logs one `DIA_LOG_INFO` with the
    number of steps drained and increments `drains` / `drainedSteps`.
27. After a `DrainAll`, a later `Tick` at or past each apply key applies those steps normally, in
    kick order, with no stall recorded.

### Executor interface

28. `SerialExecutor::ParallelFor(count, minRange, fn)` calls `fn(0, count)` exactly once on the
    calling thread and reports `GetLastRangeCount() == 1`. `count == 0` calls `fn` zero times.

### Test helpers (`Dia/DiaDeferredStep/Testing/`)

29. `ManualStepLauncher` implements `IStepLauncher` without threads: `Launch` records the step and
    returns a fresh token, `IsComplete` is false until the test calls `CompleteNext()` /
    `CompleteAll()`, and `Wait` runs `step.Run(serialExecutor)` inline if the test has not completed
    it yet (so the stall path is exercised deterministically, with no sleeps).
30. `ManualStepLauncher` records the call order (`Launch`, `IsComplete`, `Wait`, `Release` per token)
    so tests can assert the scheduler's sequencing, and asserts on `Release` of an unknown or
    already-released token.
31. `ScriptedKeyDriver` drives a scheduler through a scripted list of keys, calling `Tick` once per
    entry, so key jumps and repeats are expressed as data.
32. `RecordingStep` implements `IDeferredStep` counting `Capture` / `Run` / `Apply` calls and
    recording the thread id of each, so AC 6 and the "no SimPU state in `Run`" rule are assertable.

### Observation

33. Logs, channel `DeferredStep`: Warning on a stall (AC 21), on a rejected kick (AC 7–10), on a
    non-monotonic `Tick` (AC 17) and on a discarded completed result (AC 3); Info on a drain (AC 26).
    `DIA_TRACE_ZONE` and `DIA_PROFILE_SCOPE` around `Capture`, `Apply`, the stall `Wait` and
    `DrainAll`. Metric registration is Feature 4's job — this feature only maintains the counters.

## Files Touched

| File | Change |
|---|---|
| `Dia/DiaDeferredStep/DiaDeferredStep.vcxproj(.filters)` | New project, registered in `Cluiche.sln` (`dia check sln-sync`) |
| `Dia/DiaDeferredStep/dia.diadeferredstep.architecture.module.md` | New module doc (`dia.module.v1`); note the filename follows the `dia.dia<module>.` convention used by DiaCellHistory, not the system spec's `dia.deferredstep.` |
| `Dia/DiaDeferredStep/DeferredStepTypes.h` | New — `StepKey`, `LaunchToken`, `LaunchInfo`, `StepState`, `KickResult` |
| `Dia/DiaDeferredStep/IParallelExecutor.h` | New — interface + `RangeFn` |
| `Dia/DiaDeferredStep/SerialExecutor.h/.cpp` | New |
| `Dia/DiaDeferredStep/IDeferredStep.h` | New |
| `Dia/DiaDeferredStep/IStepLauncher.h` | New |
| `Dia/DiaDeferredStep/DeferredStepScheduler.h/.cpp` | New — the lifecycle |
| `Dia/DiaDeferredStep/DeferredStepObservation.h/.cpp` | New — log channel id and the shared `StringCRC` constants |
| `Dia/DiaDeferredStep/Testing/ManualStepLauncher.h`, `ScriptedKeyDriver.h`, `RecordingStep.h` | New |
| `Cluiche/Tests/GoogleTests/DiaDeferredStep/TestDeferredStepScheduler.cpp` | New |
| `Cluiche/Tests/GoogleTests/DiaDeferredStep/TestSerialExecutor.cpp` | New |
| `Cluiche/Tests/GoogleTests/GoogleTests.vcxproj(.filters)` | New test files; link `DiaDeferredStep` |
| `docs/reference/registry/module-registry.md` | `dia docs registry` |

Forward declarations only for `SaveGame::SaveContext` / `LoadContext` — this feature does not link
DiaSaveGame; Feature 3 adds the dependency.

## Binding Decisions

| ID | Decision | How this feature complies |
|---|---|---|
| DFS-002 | Caller-supplied monotonic `StepKey`; `Run` reads only the captured copy | AC 8, 9, 17 enforce monotonicity; AC 32's `RecordingStep` proves `Run` runs off the SimPU thread with no SimPU reads |
| DFS-003 | A late step stalls at its apply key; the key never slips | AC 12–22. `Tick` waits that frame and reports; `applyKey` is never rewritten |
| DFS-004 | The worker cap lives in this system's executor | Only the `IParallelExecutor` interface lands here; the cap is Feature 2 |
| PD-001 | `StringCRC` for ids | Channel ids and the log channel are `StringCRC` |
| PD-004 / AD-002 | No STL in public APIs | Channel table is `DynamicArrayC<_, 8>`; `RangeFn` is `std::function`, following DiaThreading's existing `JobFn` precedent (AD-002 carve-out already in the system spec); `steady_clock` is used in the `.cpp` only |
| AD-001 | Module docs; sign-off for engine API changes | New module doc. `IStepLauncher` is new in Dia and was signed off in the interview (user decision 2026-11-13) |
| AD-003 | `Dia::<Module>::` namespace | `Dia::DeferredStep::` |
| no-singletons | `ModuleRef` or streams, no statics | The scheduler is a plain object constructed with its launcher; no `Instance()`, no file statics |
| test-utilities-ship-with-libraries | Helpers live in `<Module>/Testing/` | AC 29–32 land in `Dia/DiaDeferredStep/Testing/` |

## Test Plan

TDD (RED first, quoted output) — this is new public API and pure logic, so the tdd skill applies in
full.

- **GoogleTests** (`--filter=DeferredStep*`, `--shards 2`), one group per AC block:
  - registration: duplicate id, 9th channel, unregister idle, unregister in flight, destructor drain
  - kick: capture-then-launch order, double kick, backwards key, `applyKey < kickKey`, unknown
    channel, launch-returns-complete
  - apply: not before the key, exactly once, kick order across 3 channels sharing an apply key,
    key jump of 10, repeated `Tick`, backwards `Tick`
  - stall: `Wait`-then-`Apply` order, counters, one warning, no stall when complete
  - slack: completion seen 3 keys early records slack 3; completion seen on the key records 0
  - drain: no-op, drain-then-apply equals the uninterrupted sequence (same `Apply` order, same
    recorded values), `drainedSteps` count
  - `SerialExecutor`: one range, `count == 0`
- **Sanitizer:** `dia check sanitizer --filter=DeferredStep*` over the above. The manual launcher is
  single-threaded, so this is a use-after-free check on tokens and the channel table, particularly
  AC 3 (unregister while in flight) and AC 4.
- **Gap audit** (exhaustive-test-pass rule) as the final task: enumerate every `KickResult` value,
  every `StepState` transition and every early-return branch in `Tick`, and confirm a test covers it.

## Open Design Questions

1. **Should `Unregister` apply a completed result instead of discarding it?** AC 3 discards, which is
   the safe reading of "drain or abandon cleanly" — a shutting-down consumer may already have torn
   down the state `Apply` would write into. The cost is that a result finished microseconds before
   shutdown is thrown away. Applying it instead would make `Unregister` order-sensitive against the
   consumer's own `DoStop`. Revisit if a consumer turns out to want shutdown applies.
2. **Is one step per channel per kick enough, or will a consumer want to queue?** CoW chooses k so
   steps never overlap, so a rejected double-kick is a programming error today. A queue of depth 2
   would let a consumer kick every key with k = 2 and keep the pipeline full. Out of scope, but the
   `KickResult` enum leaves room to add `kQueued` without breaking callers.
3. **`lastRunMicros` measured on SimPU is a loose figure** — it includes queue time and is only
   sampled at `Tick` granularity. Feature 5 supersedes it with real per-range CPU time. Worth
   deciding then whether to delete it rather than keep two numbers that disagree.

## Resolved Decisions

1. **One seam, `IStepLauncher`** (user decision 2026-11-13). The scheduler never sees
   `IParallelExecutor`; the launcher owns it and calls `Run`. Feature 1 therefore has no DiaThreading
   dependency, and the manual launcher makes every lifecycle test deterministic.
2. **`Launch` cannot fail** (user decision 2026-11-13, follows from the single seam). Inline
   execution on a failed submit is the launcher's business, so the scheduler has exactly one code
   path.
3. **`IParallelExecutor` and `SerialExecutor` land in Feature 1, not Feature 2** — `IDeferredStep::Run`
   needs the interface to compile and the manual launcher needs an executor to pass in. The system
   spec's features table is amended to match.
4. **`DrainAll` does not apply** — applying at drain time would change the result versus the
   uninterrupted run, which is exactly what DFS-005 avoids. Feature 3 depends on this.
