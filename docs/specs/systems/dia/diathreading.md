# System Spec: DiaThreading

## Parent Application
@docs/specs/applications/dia.md

## Purpose

DiaThreading is a new Dia engine module that extracts `JobSystem` (and its `JobHandle` type) out of `DiaCore/Threading/` into a standalone library with a clean, independently-versioned public API. The extraction removes the only high-level concern from `DiaCore/Threading/` — task-based parallelism — leaving only low-level primitives (`Thread`, `ThreadPool`, `Mutex`, `Atomic`) in `DiaCore` where they belong.

Once extracted, `DiaThreading` can take a dependency on `DiaObservation` without causing a cycle, unblocking the deferred `dia.jobs.*` metrics from Feature #11 (domain-metric-registration).

**Dependency chain before extraction:**
```
DiaCore/Threading/JobSystem
  ↓ (would need)
DiaObservation/Metric   ← also depends on DiaCore  [CYCLE]
```

**Dependency chain after extraction:**
```
Dia/DiaThreading/JobSystem  →  DiaCore (ThreadPool, Assert, Memory)
JobSystemModule             →  DiaThreading + DiaObservation (no cycle)
```

---

## Responsibilities

### JobSystem (moved from DiaCore)
- Own `JobSystem`, `JobHandle`, and `JobFn` in namespace `Dia::Threading::`
- Provide task-based parallelism: `Submit(fn)` → `JobHandle`, `Wait(h)`, `IsComplete(h)`
- Initialize a `ThreadPool` (owned internally) with a configurable worker count
- Expose `GetQueueDepth()`, `GetActiveJobCount()`, `GetSubmittedCount()`, `GetCompletedCount()`, `GetWorkerCount()` for metrics

### Metrics (registered by JobSystemModule)
- Register `dia.jobs.queue_depth` (Gauge), `dia.jobs.submitted` (Counter), `dia.jobs.completed` (Counter), `dia.jobs.active_workers` (Gauge) in `JobSystemModule::DoStart`
- Update all four per-frame in `JobSystemModule::DoUpdate`

---

## Non-Responsibilities

- Thread, ThreadPool, Mutex, Atomic — remain in `DiaCore/Threading/`; `DiaThreading` depends on them, not vice versa
- Scheduling policy beyond FIFO — no priority queues, work-stealing, or parent/child fan-out in this spec
- AsyncFileLoader — stays in `DiaCore/FilePath/`; it uses `ThreadPool` directly and does not need `JobSystem`

---

## Namespace

`Dia::Threading::` (new). The existing `Dia::Core::JobSystem` type alias or compatibility header is provided for the one-release transition; removed once all callers are updated.

---

## Features

| # | Feature | Spec | Status |
|---|---------|------|--------|
| 1 | job-system-extraction | [job-system-extraction.md](../../features/dia/diathreading/job-system-extraction.md) | Approved |
| 2 | job-metrics | [job-metrics.md](../../features/dia/diathreading/job-metrics.md) | Approved |

---

## Binding Decisions Compliance

| ID | Decision (summary) | Compliance |
|----|--------------------|------------|
| PD-001 | StringCRC for all identifiers | Metric names (`"dia.jobs.queue_depth"`, etc.) are `StringCRC` values. No raw-string maps in public API. |
| PD-004 | No STL containers in public APIs | `JobHandle` uses `std::shared_ptr` internally but its public surface is value-type copy/move semantics. No `std::vector` in public headers. |
| PD-005 | x64 only | No platform-specific code added. |
| PD-006 | VS project files source of truth | New `Dia/DiaThreading/DiaThreading.vcxproj` created; `JobSystemModule` vcxproj updated to reference it. |
| PD-007 | C++20 required | No new C++20 features beyond existing baseline. |
| PD-008 | Directory.Build.props owns build settings | No per-project overrides. |
| AD-003 | Namespace `Dia::<Module>::` | New namespace is `Dia::Threading::`. Compatibility alias `Dia::Core::JobSystem` = `Dia::Threading::JobSystem` provided during migration window. |
| AD-001 | Module docs with YAML frontmatter | `dia.threading.architecture.module.md` created. |

---

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Circular dep | Does the extraction actually break the cycle? | Yes. After extraction, `DiaCore` does not include any `DiaThreading` header. `DiaThreading` includes `DiaCore` (ThreadPool, Assert, Memory). `DiaObservation` includes `DiaCore`. `JobSystemModule` includes both `DiaThreading` and `DiaObservation`. No cycle. |
| 2 | Compatibility | What breaks when `Dia::Core::JobSystem` moves namespace? | DiaSFML/TextureHandler.cpp and JobSystemModule.cpp both include `DiaCore/Threading/JobSystem.h`. A forwarding header at the old path (`#include "DiaThreading/JobSystem.h"` + `using Dia::Core::JobSystem = Dia::Threading::JobSystem`) covers the transition. Both callers are updated in Feature 1 anyway. |
| 3 | Metrics counter accuracy | `mSubmittedCount` / `mCompletedCount` are incremented inside `ThreadPool::Enqueue` and the worker lambda. Are they thread-safe? | Both use `std::atomic<uint64_t>` with relaxed ordering on increment (sequential consistency not needed for per-frame read). |
| 4 | Active job count | `mActiveTasks` in `ThreadPool` is already `std::atomic<int>`. Does exposing it as `GetActiveJobCount()` require any locking? | No. Atomic load with acquire ordering is sufficient. |
| 5 | Metrics update cadence | Per-frame update in `DoUpdate` vs. real-time. Is per-frame good enough? | Yes — these are diagnostic gauges, not control signals. A 16ms lag on queue depth is acceptable. Per-frame matches the pattern used by all other metrics in the engine. |

---

## Status

`Done` — 2026-05-20. Both features complete. 4818 tests pass.
