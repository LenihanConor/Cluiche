# Plan: Capped Executor and Job Launcher (DiaDeferredStep Feature 2)

**Spec:** @docs/specs/applications/dia/systems/diadeferredstep/capped-executor.md
**Status:** Not Started
**System plan:** @docs/specs/applications/dia/systems/diadeferredstep/diadeferredstep.plan.md (row 2)

Starts once Feature 1 **Task 1** is Done (it needs `IParallelExecutor`, `IStepLauncher`,
`LaunchToken`, `SerialExecutor`). Can run in parallel with the rest of Feature 1 and with Feature 3 —
different files, and the only shared file is the vcxproj, which `dia docs vcxproj-add` handles
idempotently.

This is the one feature with real concurrency. Tasks 3 and 4 are the hardest work in the system; they
stay on **sonnet** unless the work shows it needs more, in which case **ask before escalating**
(feedback_confirm_opus).

## Implementation Patterns

### Shape
```
JobStepLauncher                  (SimPU-facing; token table)
  └─ CapGate                     (shared by every channel; the only M in the system)
       └─ CappedParallelExecutor (one per in-flight step, created at Launch, handed to Run)
```
`CapGate` is constructed once with the `JobSystem&` and `CapConfig`; `CappedParallelExecutor` holds a
`CapGate&` and is a short-lived per-step object, so `GetLastRangeCount()` and the range-timing array
are per-step state with no sharing.

### CapGate (Task 3)
- One `std::mutex` guarding the queue plus the slot count, and `std::atomic` counters for everything
  `CapStats` exposes, so SimPU can read stats without taking the lock. This mutex guards state shared
  between **worker threads inside one job pool**, never state across a PU boundary — the
  PU-boundaries-are-streams rule is not in play (spec Binding Decisions).
- Queue is `DynamicArrayC<GateItem, kMaxQueued>` with `kMaxQueued = kMaxChannels * maxRangesPerStep`
  bounded at construction from the resolved config. An item that cannot be queued (table full) is a
  `DIA_ASSERT` plus an inline run — never a drop.
- `GateItem` is `{ kind (kCoordinator | kRange), payload, enqueueNs }`. Coordinators and ranges share
  one queue so the strict-M invariant is one counter, not two.
- **Pump on release**: the thread that finishes an item decrements the slot count and, still holding
  the lock, pops the next queued item and submits it. No condition variable, no polling, no
  dedicated thread — admission only ever happens on a thread that just freed capacity or on the
  SimPU thread inside `Launch`.
- `Admit` never blocks. It either submits immediately (slot free) or queues and returns.
- Resolved M: `workerCap == 0` → `max(1, jobs.GetWorkerCount() / 4)`; a configured value above the
  worker count is clamped with one Warning. Resolution happens **once**, in the constructor, so
  `GetWorkerCap()` is stable.
- `~CapGate` asserts `slotsHeld == 0` and the queue is empty. The scheduler's `DrainAll` guarantees
  it; the assert catches a wiring mistake.

### CappedParallelExecutor (Task 4)
- `R = clamp(ceil(count / minRange), 1, workerCap * rangesPerWorker)`. Integer ceiling —
  `(count + minRange - 1) / minRange`, with `minRange` clamped to ≥ 1 first.
- Range boundaries are computed once into a stack array so every range's `[begin, end)` is fixed
  before anything is submitted — the split must not depend on execution order (AC 1, AC 18).
- **Claim protocol**: `std::atomic<uint32_t> mNextUnclaimed`. A helper job and the coordinator both
  claim by `fetch_add`. The coordinator loops claiming and running until the index passes R, then
  waits on the handles of the ranges that were admitted. This is what guarantees no range is
  stranded (AC 11, AC 13) and it is the core correctness argument of the feature — write the test
  first and name it for the invariant.
- Per-range `RangeTiming` written by whichever thread runs the range, into a slot indexed by range
  number, so there is no contention on the timing array and no need for a lock.
- A range body is wrapped in `DIA_PROFILE_SCOPE` + `DIA_TRACE_ZONE` named for the channel
  (`LaunchInfo` carries it). Feature 5 adds the `JobTag`; leave the submit call in one place so
  Feature 5 is a one-line change.
- `fn` throwing: catch per range, record the first exception, finish the remaining ranges, then
  rethrow after the wait (AC 5). DiaThreading's throwing-job fix (6b81ca52) must not be regressed —
  the test for it is in the DiaThreading suite and runs as part of the scoped filter.

### JobStepLauncher (Task 5)
- Token table `DynamicArrayC<LaunchSlot, kMaxInFlight>` with `kMaxInFlight == DeferredStepScheduler::kMaxChannels`.
  `LaunchSlot` is `{ JobHandle handle, bool complete, uint8_t generation, CappedParallelExecutor executor }`.
- Token encoding: `value = (index + 1) | (generation << 8)`. Index 0 can therefore never produce the
  invalid token 0. `Release` bumps the generation so a stale token is detectable (AC 21, 23).
- `Launch` submits the coordinator through the gate. The coordinator lambda captures
  `IDeferredStep*` and the slot index by value — never a reference to a temporary. Step lifetime is
  guaranteed by the scheduler (it drains before `Unregister` and before destruction), which is worth
  a comment at the capture site.
- `TrySubmit` failing for the coordinator → run `step.Run(serialExecutor)` **synchronously on the
  calling thread**, mark the slot complete, log one Warning, return the token. This is the
  "`Launch` never fails" contract from Feature 1 (AC 16, 17).
- `IsComplete` / `Wait` on a complete-by-inline slot never touch the `JobSystem` (AC 22).
- Only SimPU calls `Launch`/`IsComplete`/`Wait`/`Release`; that is documented on the class and is why
  the token table needs no lock.

### Testing
- `Testing/ConcurrencyProbe.h`: an `std::atomic<int>` incremented on entry and decremented on exit,
  tracking its own peak. This is how AC 6–8 are asserted — a **counter, never a sleep**, so the tests
  do not flake under `--shards 2` (feedback_dia_run_gotchas).
- `Testing/ReorderingExecutor.h`: same split, ranges run in reverse order, single-threaded. For
  AC 18's bit-identity matrix.
- `Testing/ReferenceGatherStep.h`: a grid gather plus a reduction that *looks* order-sensitive but is
  not (fixed-point accumulate or a per-range partial reduced in range order), so a genuine ordering
  bug would show up as a byte difference.
- `Testing/FailingJobSystem.h`: wraps a real `JobSystem` and forces `TrySubmit` to fail on the Nth
  call, for AC 15.
- AC 10 is the only duration-based test (10 ms ranges). Give it generous bounds and mark it `SLOW_`
  so it can be filtered out of fast iteration.

### Verification cadence
- Per task: `dia run googletest --filter="DeferredStepCap*:DeferredStepLauncher*:JobSystem*" --shards 2`.
- `JobSystem*` is in the filter deliberately: this feature leans on `TrySubmit`, `Wait` and the
  throwing-job behaviour, so a regression there must surface immediately.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 0 | Add the DiaThreading reference to `DiaDeferredStep.vcxproj`, add `DiaThreading` to the module doc's `dependent_modules`, `dia check deps`. Confirm `JobSystem::GetWorkerCount()` on this machine and record it in notes (the cap maths is relative to it) | Project compiles with `<DiaThreading/JobSystem.h>` included | Done | haiku | Mechanical, TDD-exempt; DiaThreading ProjectReference + dia.threading dependency added; dia check deps OK. Machine has 64 logical processors, so JobSystem(0) gives ~64 workers and default M = 16. Include compile confirmed by task 1 build. |
| 1 | `CapConfig.h`, `RangeTiming.h` (plain POD, no DiaObservation dependency). RED → GREEN on cap resolution only: `workerCap == 0` → `max(1, workers/4)`, explicit value, over-cap clamp + one Warning | AC 9 | Done | sonnet | Resolution is pure arithmetic — test it before the gate exists; 6 tests green (AC 9 arithmetic). No separate RED run: headers+test written together. Flag constants named kRangeStolen/kRangeTrySubmitFailed. |
| 2 | RED → GREEN: the range-split maths, extracted as a free function so it is testable without threads | AC 1–3: full coverage of `[0,count)`, contiguity, the `clamp(ceil(...))` formula at its boundaries, `count == 0`, `minRange > count`, `minRange == 0` clamped to 1 | Done | sonnet | Pure logic. Keep it a free function so AC 18's matrix can reuse it; 12 DeferredStepCap tests green; RED = missing RangeSplit.h. GetRangeBounds returns empty [count,count) for index>=rangeCount. |
| 3 | RED → GREEN: `CapGate` — admission, bounded queue, pump-on-release, stats, destructor assert | AC 6–8 with `ConcurrencyProbe` (M = 1, 2, 4 against 1, 3 and 16 channels' worth of items); `slotsHeld` returns to 0; `peakSlotsHeld <= M`; `queuedAtLeastOnce` and `peakQueueDepth` | Done | sonnet | **Hardest task in the system.** Ask before escalating past sonnet. The mutex + atomics split (lock for queue, atomics for stats) is the pattern to validate; 10 gate tests green x4 runs; contention loop 50 iters at M=1/2/4. Deviations: GetStats by value, ring buffer in std::vector (private), no public Release (Finish inside job), ConcurrencyProbe in Testing namespace, GateItem+SetSubmitFailureHookForTest added. Lock held across TrySubmit (accepted). |
| 4 | RED → GREEN: `CappedParallelExecutor` — claim protocol, coordinator stealing, wait-for-admitted, per-range timing, throwing range | AC 4, 5, 11, 13, 25: nothing still running on return; a throwing range propagates after the others finish; M = 1 on a one-worker pool completes 16 ranges with `stolen == 16`; timing field ordering | Done | sonnet | The claim protocol is the correctness core — name the test after the invariant. Ask before escalating; 15 executor tests green x3. Spec discrepancy: 16 ranges => stolen=15 (range 0 runs directly on coordinator). AC6 probe asserts <=3 (nesting); TIGHTEN in task 8 to count range bodies only (<=M). Test helper std::exit(1) on 10s hang. |
| 5 | RED → GREEN: `JobStepLauncher` — token table, generation encoding, coordinator submit through the gate, inline fallback for a failed coordinator submit, `IsComplete`/`Wait`/`Release` | AC 14, 16, 17, 20–24: `Launch` returns immediately with a full gate; uninitialised `JobSystem` runs inline and reads complete; table exhaustion; stale/zero/released token; `Wait` on complete does not touch the pool; destructor assert | Done | sonnet | Uses `Testing/FailingJobSystem.h` from task 7 — write the minimal version here if needed; 17 launcher tests green x3 (counted OK lines; summary line unreliable). Fixed std::array<Slot,8> not DynamicArrayC; overflow token idx 9 inert; throwing step logged Error + counts complete; pump-time submit failure degrades to inline on SimPU in IsComplete/Wait. Untested: worker-thread Error log, Release-build overflow. |
| 6 | RED → GREEN: determinism matrix | AC 18–19: `ReferenceGatherStep` byte-compared across `SerialExecutor`, capped at M = 1/2/8, forced range counts 1/3/17, and `ReorderingExecutor` | Done | sonnet | If any pair differs, stop — it is a real ordering bug in the executor or the reference step, not a tolerance to widen; 5 determinism tests green; M=1/2/8 x ranges 1/3/17/natural x grids 1/7/1000/4099, 20 iters each, byte-identical. No RED run (tests+headers written together). |
| 7 | RED → GREEN: range-level fallback and the remaining stats — `TrySubmit` failure runs inline and flags it, one Warning per step (rate-limited), `inline` vs `stolen` never conflated, `totalCapWaitNs` reconciliation. Finish `Testing/FailingJobSystem.h` | AC 15, 26, 27: forced failure on the Nth submit; cap wait > 0 for the second channel at M = 1; cap wait 0 at M = workers on an idle pool; stats sum to the per-range values | Done | sonnet | The rate limit matters — a shut-down pool must not flood the log; 5 characterization tests green (AC 15, 26, 27, 28 rate limit, timing ordering). No production change. FailingJobSystem.h not needed: CapGate::SetSubmitFailureHookForTest is the seam. |
| 8 | Exhaustive test pass: gap audit of every AC, every `CapStats` counter and every `RangeTiming` flag; edge cases — `JobSystem` shut down mid-step, M larger than the queue bound, `ParallelFor` called twice in one `Run` (sequential passes, the normal consumer shape), a step whose `Run` calls no `ParallelFor` at all, `count == 1` with M = 8; AC 10's starvation test | Gap list in notes; AC 10 green | Done | sonnet | No mutation check. Mark AC 10 `SLOW_`; 109/109 OK in scoped filter. All 28 ACs mapped. Fixed latent TrySteal missing notify_all (CapGate.cpp) and a 48-item test overflowing the queue. AC6 probe tightened to <=2. Real concurrent JobSystem::Shutdown races inside DiaThreading (mThreadPool raw pointer) - out of scope, modelled via hook. Spec AC 11 says stolen==16; real value 15. |
| 9 | Feature close-out: `dia run googletest --filter="DeferredStepCap*:DeferredStepLauncher*:JobSystem*" --shards 2`; `dia check sanitizer` over the same filter (the one feature where this really matters — TSan is unavailable, so ASan/UBSan plus the concurrency probe plus a read of the gate's atomics is the race argument); `dia check deps`; observation scan in notes; update the system spec features table | All ACs, quoted | Not Started | sonnet | Also run the step a few hundred times in a loop to shake out rare interleavings; record the iteration count in notes |
