# Feature Spec: Capped Executor and Job Launcher

**Parent:** @docs/specs/applications/dia/systems/diadeferredstep/diadeferredstep.md (Feature 2)
**Status:** Done
**Plan:** @docs/specs/applications/dia/systems/diadeferredstep/capped-executor.plan.md
**Depends on:** Feature 1 (`IStepLauncher`, `IParallelExecutor`, `SerialExecutor`, `LaunchToken`)

## Summary

The real launcher. `JobStepLauncher` implements `IStepLauncher` over a `Dia::Threading::JobSystem`:
it submits one coordinator job per step, which runs `IDeferredStep::Run` against a
`CappedParallelExecutor`. A single `CapGate` enforces DFS-004 strictly — every deferred-step job,
coordinators included, occupies at most **M** workers at once, so an 8-worker pool with M = 2 never
gives world simulation more than a quarter of the pool no matter how many channels are in flight.

The deadlock that strict accounting invites is avoided by making the coordinator work instead of
wait: `ParallelFor` splits into ranges, hands as many as the gate allows to other workers, and runs
the rest on the coordinator thread itself. At M = 1 a step is fully serial on one worker, and still
correct. A failed `TrySubmit` runs the range inline and is counted.

## Goals

- A step in flight never costs more than M workers, measured, not asserted.
- Progress is guaranteed at any M ≥ 1 and any worker count ≥ 1, with no nested-wait deadlock.
- The result is bit-identical to `SerialExecutor` at every M and every range count.
- Every range carries the timing a Feature 5 summary record needs: cap wait, worker wait, run time,
  inline flag.

## Design

### Strict M, with a working coordinator

```
Kick ──► JobStepLauncher::Launch
           gate.Admit(coordinator item)            // counts against M
             │ slot free  ──► JobSystem::TrySubmit(coordinator)
             └ gate full  ──► queued; admitted when some other item releases a slot
                                (the releasing thread pumps the queue)

worker: coordinator ──► step.Run(CappedParallelExecutor)
                          ParallelFor(count, minRange, fn)
                            R = clamp(ceil(count / minRange), 1, M * rangesPerWorker)
                            enqueue ranges 1..R-1 on the gate
                            run range 0 on this thread
                            steal any still-unadmitted range and run it here
                            wait for admitted ranges to finish
```

The coordinator already holds a gate slot, so the ranges it runs itself need no admission — the
invariant "at most M deferred-step jobs are executing" holds throughout. Because the coordinator
steals every range the gate has not admitted, no range is ever stranded behind a cap the coordinator
itself is filling. That is the whole deadlock-freedom argument, and AC 11 tests it at M = 1 on a
one-worker pool.

### Why queue rather than run inline

The system spec is explicit that ranges beyond the cap *queue inside DiaDeferredStep*. Queuing is
what makes **cap wait** a real number (enqueue → admission), which is in turn what makes the Perf
Inspector able to say "this channel was slow because the other channel held the cap" rather than just
"this was slow". Inline execution is reserved for two cases: the coordinator stealing its own
unadmitted ranges (normal, cheap, flagged) and a `TrySubmit` that fails (exceptional, flagged).

### Timing captured per range

| Field | Meaning |
|---|---|
| `enqueueNs` | the coordinator put the range on the gate |
| `admitNs` | the gate released a slot to it and submitted it (0 if stolen by the coordinator) |
| `startNs` | `fn(begin, end)` began |
| `endNs` | `fn` returned |
| `capWaitNs` | `admitNs - enqueueNs`, or `startNs - enqueueNs` for a stolen range |
| `flags` | `kStolen`, `kTrySubmitFailed` |

Worker wait (submit → start) comes from the job's own `JobTimingRecord` and is Feature 5's business;
cap wait is measured here because only DiaDeferredStep can see it.

## Public API

### `Dia/DiaDeferredStep/CapConfig.h`

```cpp
namespace Dia::DeferredStep {

    struct CapConfig
    {
        // 0 means derive: max(1, workerCount / 4) (system spec Worker cap).
        uint32_t workerCap       = 0;
        // Upper bound on ranges per ParallelFor, as a multiple of workerCap.
        uint32_t rangesPerWorker = 4;
    };
}
```

### `Dia/DiaDeferredStep/CapGate.h`

```cpp
namespace Dia::DeferredStep {

    struct CapStats
    {
        uint32_t slotsHeld        = 0;   // now
        uint32_t peakSlotsHeld    = 0;
        uint32_t workerCap        = 0;   // resolved M
        uint64_t admitted         = 0;
        uint64_t queuedAtLeastOnce= 0;
        uint64_t stolen           = 0;
        uint64_t trySubmitFailed  = 0;
        uint64_t totalCapWaitNs   = 0;
        uint64_t peakQueueDepth   = 0;
    };

    // Admission control shared by every channel. Thread-safe: workers release
    // slots and pump the queue; SimPU admits coordinators.
    class CapGate
    {
    public:
        CapGate(Dia::Threading::JobSystem& jobs, const CapConfig& config);
        ~CapGate();                                  // asserts the gate is empty

        uint32_t GetWorkerCap() const;               // resolved M
        const CapStats& GetStats() const;            // snapshot-safe: plain atomics
        void     ResetStats();
    };
}
```

`CapGate`'s admission API (`Admit`, `Release`, `Steal`) is internal to the module — consumers only
ever see `IParallelExecutor` and `IStepLauncher` — so it is declared but documented as not part of
the public contract.

### `Dia/DiaDeferredStep/CappedParallelExecutor.h`

```cpp
namespace Dia::DeferredStep {

    // One per in-flight step; created by JobStepLauncher, handed to IDeferredStep::Run.
    // Not thread-safe: only the coordinator thread calls ParallelFor on it.
    class CappedParallelExecutor final : public IParallelExecutor
    {
    public:
        explicit CappedParallelExecutor(CapGate& gate, const LaunchInfo& info);

        void     ParallelFor(uint32_t count, uint32_t minRange, const RangeFn& fn) override;
        uint32_t GetLastRangeCount() const override;

        // Feature 5 reads these to build the step summary.
        uint32_t GetRangeTimingCount() const;
        const RangeTiming& GetRangeTimingAt(uint32_t index) const;
    };
}
```

### `Dia/DiaDeferredStep/JobStepLauncher.h`

```cpp
namespace Dia::DeferredStep {

    class JobStepLauncher final : public IStepLauncher
    {
    public:
        static const unsigned int kMaxInFlight = DeferredStepScheduler::kMaxChannels;

        JobStepLauncher(Dia::Threading::JobSystem& jobs, const CapConfig& config);
        ~JobStepLauncher();                          // asserts nothing in flight

        LaunchToken Launch    (IDeferredStep& step, const LaunchInfo& info) override;
        bool        IsComplete(LaunchToken token) const override;
        void        Wait      (LaunchToken token) override;
        void        Release   (LaunchToken token) override;

        CapGate&       GetCapGate();
        const CapGate& GetCapGate() const;
    };
}
```

## Acceptance Criteria

### Range splitting

1. `ParallelFor(count, minRange, fn)` covers `[0, count)` exactly once: ranges are contiguous,
   non-overlapping, half-open, and every index is visited once.
2. The range count is `clamp(ceil(count / minRange), 1, workerCap * rangesPerWorker)`;
   `GetLastRangeCount()` reports it. `count <= minRange` gives one range.
3. `count == 0` calls `fn` zero times, reports one range, and returns immediately.
4. `ParallelFor` returns only after every range has finished — no range is still running when it
   returns, including ranges that other workers took.
5. A range that throws propagates after all other ranges have finished, leaving no job orphaned.
   (DiaThreading's `Wait` was fixed for throwing jobs in 6b81ca52; this feature must not regress it.)

### The cap

6. With M = 2 on an 8-worker pool and one channel running a step split into 16 ranges, a concurrency
   counter incremented on entry to each range body and to the coordinator never exceeds 2.
7. With M = 2 and three channels launched together, the same counter still never exceeds 2 — the cap
   is global across channels, not per channel.
8. `CapStats::peakSlotsHeld` never exceeds `workerCap`, and `slotsHeld` is 0 once every step has
   completed.
9. `workerCap == 0` in the config resolves to `max(1, jobs.GetWorkerCount() / 4)`;
   `GetWorkerCap()` reports the resolved value. A configured cap larger than the worker count is
   clamped to the worker count and logs one Warning.
10. Gameplay jobs are not starved: with M = 1 and a step running 16 ranges of 10 ms, a job submitted
    straight to the `JobSystem` afterwards completes while the step is still running (it waits behind
    at most one deferred-step range, not the whole step).

### Progress guarantees

11. M = 1 on a **one-worker** pool completes a 16-range step: the coordinator steals every range and
    runs it. No deadlock, no timeout. `CapStats::stolen == 16`.
12. M = 1 with two channels launched together completes both. The second channel's coordinator waits
    in the gate queue, is admitted when the first releases, and `queuedAtLeastOnce >= 1`.
13. A coordinator never blocks on a range that no thread will run: after `ParallelFor` has handed out
    what the gate allows, every remaining range is run by the coordinator.
14. `Launch` never blocks SimPU. With the gate full, `Launch` returns a valid token immediately and
    the coordinator starts later; `IsComplete` is false in the meantime.

### Fallbacks

15. `TrySubmit` returning an invalid handle for a **range** runs that range on the coordinator
    thread, flags it `kTrySubmitFailed`, and increments `trySubmitFailed`. The result is unchanged.
16. `TrySubmit` returning an invalid handle for the **coordinator** (pool not initialised or already
    shut down) makes `Launch` run `step.Run(serialExecutor)` synchronously on the calling thread and
    return a token that `IsComplete` reports true for immediately — honouring Feature 1's "`Launch`
    never fails" contract. One Warning is logged.
17. A `JobStepLauncher` constructed over a `JobSystem` that is never initialised runs every step
    inline per AC 16, so a game with no job system still works, serially.

### Determinism

18. A reference step (grid gather plus an order-sensitive-looking reduction) produces bit-identical
    output under: `SerialExecutor`; the capped executor at M = 1, 2 and 8; range counts forced to 1,
    3 and 17; and `ReorderingExecutor`, which runs the same ranges in reverse order. All runs are
    compared byte-wise against the serial result.
19. `GetLastRangeCount()` differing between two runs never changes the output (AC 18 is the proof;
    this is stated as the contract `IDeferredStep::Run` implementers rely on).

### Tokens

20. `Launch` returns a distinct valid token per in-flight step, up to `kMaxInFlight` (8). An
    attempt beyond that asserts in Debug and runs the step inline in Release.
21. `IsComplete` on a released token, a zero token or a token from a previous generation returns
    true and asserts in Debug — a stale token must never read as "still running".
22. `Wait` on an already-complete token returns immediately without touching the `JobSystem`.
23. `Release` returns the slot and bumps its generation, so the token cannot be reused.
24. The destructor asserts that nothing is in flight (the scheduler's `DrainAll` guarantees this) and
    does not block.

### Timing

25. Every range produces one `RangeTiming` with `enqueueNs <= startNs <= endNs`, non-zero durations
    on real work, and `capWaitNs` consistent with the definition table above.
26. With M = 1 and two channels launched together on an otherwise idle pool, the second channel's
    ranges report `capWaitNs > 0`. On an idle pool with M equal to the worker count, `capWaitNs` is 0
    for every range.
27. `CapStats::totalCapWaitNs` equals the sum of the per-range `capWaitNs` values.

### Observation

28. Logs, channel `DeferredStep`: Warning on a clamped cap (AC 9), on a coordinator `TrySubmit`
    failure (AC 16) and the first time a range `TrySubmit` fails per step (AC 15, rate-limited to one
    per step so a shut-down pool cannot flood the log). `DIA_PROFILE_SCOPE` and `DIA_TRACE_ZONE`
    around the coordinator body and each range body, named for the channel. Metric registration is
    Feature 4; `JobTag` attribution is Feature 5.

## Files Touched

| File | Change |
|---|---|
| `Dia/DiaDeferredStep/CapConfig.h` | New |
| `Dia/DiaDeferredStep/RangeTiming.h` | New — the per-range record (plain struct, no DiaObservation dependency) |
| `Dia/DiaDeferredStep/CapGate.h/.cpp` | New — admission, queue, pump-on-release, stats |
| `Dia/DiaDeferredStep/CappedParallelExecutor.h/.cpp` | New |
| `Dia/DiaDeferredStep/JobStepLauncher.h/.cpp` | New — token table, coordinator submit, inline fallback |
| `Dia/DiaDeferredStep/DiaDeferredStep.vcxproj(.filters)` | New files (`dia docs vcxproj-add`); add the DiaThreading reference |
| `Dia/DiaDeferredStep/dia.diadeferredstep.architecture.module.md` | Add `DiaThreading` to `dependent_modules` (`dia check deps`) |
| `Dia/DiaDeferredStep/Testing/ReorderingExecutor.h` | New — runs ranges in reverse for AC 18 |
| `Dia/DiaDeferredStep/Testing/ConcurrencyProbe.h` | New — the counter AC 6/7 assert on |
| `Dia/DiaDeferredStep/Testing/ReferenceGatherStep.h` | New — the bit-identity step for AC 18 |
| `Cluiche/Tests/GoogleTests/DiaDeferredStep/TestCapGate.cpp` | New |
| `Cluiche/Tests/GoogleTests/DiaDeferredStep/TestCappedParallelExecutor.cpp` | New |
| `Cluiche/Tests/GoogleTests/DiaDeferredStep/TestJobStepLauncher.cpp` | New |

## Binding Decisions

| ID | Decision | How this feature complies |
|---|---|---|
| DFS-004 | The cap lives in DiaDeferredStep's executor; the JobSystem stays FIFO with no priorities | `CapGate` is wholly inside this module. No DiaThreading change in this feature (the only one in the system is Feature 5's `TrySubmit(fn, tag)` overload). Strict M, coordinators included (user decision 2026-11-13) |
| DFS-002 | `Run` reads only the captured copy | Ranges are independent by contract; AC 18 proves the result does not depend on range count or order |
| PD-004 / AD-002 | No STL in public APIs | `CapConfig`, `CapStats`, `RangeTiming` are plain structs; the gate queue is a `DynamicArrayC`; `std::mutex`/`std::atomic` and `steady_clock` are implementation-only |
| PU-boundaries-are-streams | No mutex-guarded cross-PU reads | `CapGate`'s mutex guards state shared between **worker threads inside one job pool**, not state shared across PUs. SimPU reads only `CapStats` atomics. No PU boundary is crossed: `Run` touches the working copy only |
| AD-001 | Sign-off for engine API changes | New Dia files only, inside the module this system owns; no change to DiaThreading's public API here |
| no-singletons | No statics | `CapGate` is owned by `JobStepLauncher`, which is owned by the module (Feature 4) |

## Test Plan

TDD for the splitting and admission logic; the concurrency ACs are written as tests first and will be
RED by construction (the gate does not exist).

- **GoogleTests** (`--filter=DeferredStepCap*:DeferredStepLauncher*`, `--shards 2`):
  - splitting: coverage, range count formula at the boundaries, `count == 0`, `minRange > count`
  - cap: concurrency probe at M = 1, 2, 4 with 1, 3 and 16 channels' worth of ranges; peak never
    exceeds M; `slotsHeld` returns to 0
  - progress: one-worker pool at M = 1; two channels at M = 1; a 16-range step with M = 1
  - fallbacks: uninitialised `JobSystem`; `JobSystem` shut down mid-step (coordinator submit fails);
    range submit forced to fail through a test `JobSystem` wrapper
  - determinism: the AC 18 matrix, byte-compared
  - tokens: exhaustion, stale token, double release, wait on complete
  - timing: field ordering, cap wait at M = 1 vs M = workers, stats reconciliation
- **Sanitizer:** `dia check sanitizer --filter=DeferredStepCap*:DeferredStepLauncher*`. This is the
  one feature with real concurrency, so ASan/UBSan here matters most. TSan is unavailable on this
  toolchain (system spec Test Strategy), so the race argument rests on the concurrency probe, the
  poisoned-source test from the system spec, and review of the gate's atomics.
- **Timing-sensitive tests**: the cap ACs use a latch/counter, not sleeps, so they do not flake under
  `--shards 2`. The 10 ms work in AC 10 is the only duration-based test and is given generous bounds.
- **Gap audit** as the final task: every `CapStats` counter, every `RangeTiming` flag and every
  fallback branch has a test.

## Open Design Questions

1. **Should `rangesPerWorker` scale with M or with the worker count?** The formula caps ranges at
   `M * rangesPerWorker`, so M = 1 gives 4 ranges and all of them are stolen by the coordinator —
   fine, but it means a small M also reduces granularity, which is only useful if range overhead
   matters. Capping at `workerCount * rangesPerWorker` instead would keep granularity constant and
   let a future cap increase reuse the same split. Measure with CoW's real step before fixing.
2. **Is a single global `CapGate` the right granularity?** It gives the DFS-004 guarantee exactly,
   but it also means a long weather step can delay an ecology step that would otherwise have had
   spare capacity. A per-channel reservation (each channel guaranteed one slot, the rest shared)
   would soften that at the cost of a weaker global bound. Revisit when CoW runs more than one
   channel — today the whole chain is one channel.
3. **Should the coordinator hold a slot at all while it is purely waiting?** Once it has handed out
   every range and has none left to steal, it sits in `Wait` holding a slot that no longer does work.
   Releasing it would let another channel in sooner but would briefly break the strict-M invariant
   the tests assert. Left strict for v1; revisit if cap utilisation measurements show it idling.

## Resolved Decisions

1. **Strict M, coordinator participates** (user decision 2026-11-13). Every deferred-step job counts
   against the cap, matching DFS-004's wording. The coordinator runs ranges itself rather than idling
   inside `ParallelFor`, which both prevents deadlock at M = 1 and stops a worker being spent purely
   on waiting.
2. **Ranges queue; inline is the exception.** Queuing is what makes cap wait measurable, which
   Feature 5 and the Perf Inspector Workers view depend on. Inline execution happens only when the
   coordinator steals an unadmitted range or `TrySubmit` fails, and both are flagged.
3. **`Launch` never fails** (inherited from Feature 1). A dead or missing `JobSystem` degrades to
   synchronous execution on SimPU, not to an error the scheduler has to handle.
