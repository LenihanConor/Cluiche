# Feature Spec: Job Observation

**Parent:** @docs/specs/applications/dia/systems/diathreading/diathreading.md
**Consumed by:** @docs/specs/applications/dia/systems/diaobservation/perf-inspector.md (Revision 2026-11-12, Workers view), @docs/specs/applications/dia/systems/diaobservation/profiling-frame-instrumentation.md (Amendment 2026-11-12), @docs/specs/applications/dia/systems/diadeferredstep/diadeferredstep.md (Feature 4)
**Mockup:** @docs/mockups/perf-inspector-workers-mockup.html
**Status:** `Approved` — 2026-11-12.

## Summary

Job work is invisible today: worker threads never register with the profile or trace pillars, so `DIA_PROFILE_SCOPE` inside a job is silently dropped. A job also has no owner, and its timing is never recorded. This feature adds three things to the `JobSystem`, without making DiaThreading depend on DiaObservation:

1. **An observer hook**, `IJobObserver`, passed to `JobSystem::Initialize` in the same way as `IJobMetrics`. It is called when a worker starts and stops, on the submitting thread to capture context, and on the worker around each job.
2. **A job context** captured at `Submit`: owner (the submitting module or deferred-step channel), trace parent, an optional tag (for example a sim key) and the range index. It is restored on the worker before the job runs.
3. **A fixed-size timing record per job** (submit, start, end, worker, owner, tag, range). It goes back to the submitting PU's inbox, so the data reaches that PU through a single-consumer queue and never through a shared read.

DiaObservation provides the implementation (`JobObserver`, beside `JobSystemMetricsAdapter`), and `JobSystemModule` wires it up.

## Problem

- DiaDeferredStep (weather, ecology) and in-frame parallel ranges (A4) move cost onto workers. The Perf Inspector shows only PU lanes, so that cost and its contention with gameplay jobs can't be seen.
- The DiaDeferredStep spec puts `DIA_PROFILE_SCOPE` on `Run`. Without worker registration those scopes are dropped by the profiler's unregistered-thread rule.
- Even with registration, a scope on a worker has no enclosing module scope, so its subject would be zero ("unattributed").

## Acceptance Criteria

| ID | Criterion | Verification |
|----|-----------|--------------|
| AC1 | With an observer installed, `OnWorkerStart(i)` runs once on each worker thread before it takes a job, and `OnWorkerStop(i)` runs before the thread exits | Unit test with a recording observer |
| AC2 | `CaptureContext()` runs on the submitting thread, and `OnJobBegin` / `OnJobEnd` run on the worker with the same context | Unit test (thread ids recorded) |
| AC3 | `Submit(fn, JobTag{owner, tag, range, rangeCount})` overrides the captured owner and tag; plain `Submit(fn)` uses the captured context unchanged | Unit test |
| AC4 | Each completed job produces exactly one `JobTimingRecord` with `submitNs <= startNs <= endNs`, the worker index, owner, tag and range | Unit test |
| AC5 | With no observer installed (`nullptr`), `Submit` and job execution cost one branch more than today and capture no context | Benchmark (reported) + code review |
| AC6 | Observer overhead in Release: capture + begin + end + record write under 100 ns per job, measured with a 10,000 jobs/s synthetic flood | Benchmark (gating at 2x) |
| AC7 | A full inbox drops the newest record and counts the drop; it never blocks a worker | Unit test |
| AC8 | `DIA_PROFILE_SCOPE` inside a job is recorded (worker registered) and inherits the job owner as its subject | Integration test with DiaObservation's `JobObserver` |
| AC9 | DiaThreading still includes no DiaObservation header | `dia check deps` |

## API Design

```cpp
// Dia/DiaThreading/JobContext.h
namespace Dia::Threading
{
    struct JobTag                       // explicit attribution, e.g. a deferred-step channel
    {
        uint32_t owner      = 0;        // CRC value of the owner (module instance id or channel id)
        uint32_t tag        = 0;        // free-form, e.g. kick sim key
        uint16_t range      = 0;
        uint16_t rangeCount = 0;
    };

    struct JobContext
    {
        JobTag   attribution;
        uint64_t traceParent = 0;       // span id of the submitting scope (0 = none)
        uint32_t submitThread = 0;
        void*    inbox = nullptr;       // opaque to DiaThreading; the observer's per-PU inbox
    };

    struct JobTimingRecord              // 40 bytes in memory; the wire form is packed fixed point (SD-PERF-7)
    {
        uint64_t submitNs;
        uint64_t startNs;
        uint64_t endNs;
        uint32_t owner;
        uint32_t tag;
        uint16_t range;
        uint16_t rangeCount;
        uint16_t worker;
        uint16_t flags;                 // e.g. ran inline (TrySubmit fallback)
    };
}

// Dia/DiaThreading/IJobObserver.h
namespace Dia::Threading
{
    class IJobObserver
    {
    public:
        virtual ~IJobObserver() = default;
        virtual void       OnWorkerStart(unsigned workerIndex) = 0;      // worker thread
        virtual void       OnWorkerStop (unsigned workerIndex) = 0;      // worker thread
        virtual JobContext CaptureContext() = 0;                          // submitting thread
        virtual void       OnJobBegin(const JobContext& ctx) = 0;         // worker thread, before fn
        virtual void       OnJobEnd  (const JobContext& ctx,
                                      const JobTimingRecord& rec) = 0;    // worker thread, after fn
    };
}

// JobSystem changes
void      JobSystem::Initialize(unsigned numThreads, IJobMetrics* metrics, IJobObserver* observer = nullptr);
JobHandle JobSystem::Submit(JobFn fn);                    // context captured from the submitting thread
JobHandle JobSystem::Submit(JobFn fn, const JobTag& tag); // explicit owner/tag/range
```

**DiaObservation side (not in DiaThreading):** `Dia::Observation::Job::JobObserver` implements the hooks:
- `OnWorkerStart` registers the log, trace and profile thread buffers and names the thread `Worker N` in the profiler's thread-name table.
- `CaptureContext` reads the calling thread's current subject (the profiler's open-scope stack), its active trace span, and the inbox the PU installed for its thread.
- `OnJobBegin` pushes a `job.run` scope with the owner as subject, so sections inherit it.
- `OnJobEnd` writes the record to `ctx.inbox`. When profiling is on, it also writes the record to the worker's profile ring as a `profile_job` record.

`JobInbox` is a fixed-capacity multi-producer, single-consumer ring owned by one PU. It is drained by that PU once per `Update()` (see perf-inspector `JobTimingTable`). A thread with no inbox (for example a test thread) submits with `inbox = nullptr`, and its records are counted but not kept.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | `ThreadPool` (DiaCore) gains optional thread start/stop callbacks | Unit test | Planned | sonnet | DiaCore change; AD-001 sign-off given 2026-11-12 |
| 2 | `JobContext`, `JobTag`, `JobTimingRecord`, `IJobObserver`; `JobSystem` Initialize/Submit overloads; timing captured around `fn` | AC1-AC5, AC9 | Planned | sonnet | DiaThreading |
| 3 | `JobInbox` (MPSC, fixed capacity, drop + count) | AC7 | Planned | sonnet | DiaObservation, pure, DiaCore only (SD-PERF-3) |
| 4 | `JobObserver` in DiaObservation + wiring in `JobSystemModule`; PU installs its inbox for its thread at start | AC8 | Planned | sonnet | |
| 5 | Benchmark + gap audit (submit from an unregistered thread, worker exit mid-job, inbox full, observer null) | AC5, AC6 | Planned | sonnet | Final verification |

## Binding Decisions

| ID | Decision | How it applies |
|----|----------|----------------|
| (DiaThreading purpose) | DiaThreading depends on DiaCore only; observation is injected (`IJobMetrics` precedent) | `IJobObserver` is an interface; the implementation lives in DiaObservation |
| DFS-004 | The JobSystem stays first-in-first-out, with no priorities | The observer only observes; it never reorders or delays jobs |
| SD-PERF-2 | Telemetry never writes at tick rate | Records go to rings and inboxes, never to logs or files on the worker |
| SD-PERF-7 | Telemetry cost budgets (perf-inspector Revision 2026-11-12) | AC6; detached cost counts toward the < 0.02 ms per frame budget |
| (PU boundaries are streams) | No cross-PU shared reads | Records reach the submitting PU through its single-consumer inbox |

## Open Design Questions

1. **Fire-and-forget jobs submitted from a non-PU thread** (the DebugServer thread, asset loaders) have no inbox. Should there be a shared "unowned" inbox drained by Main, or are counts alone enough? Proposal: counts only for now.
2. **Nested submits.** A job that submits more jobs captures its own `job.run` context, so the child's owner is the parent job's owner. Is that the right attribution, or should children name the parent job? Proposal: inherit the owner, and record the parent job via `traceParent`.
