# Feature Spec: Job Metrics

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diathreading.md | **job-metrics** |

**Status:** `Approved` — 2026-05-20

**Depends on:** Feature 1 (job-system-extraction) — `JobSystem::GetQueueDepth()`, `GetActiveJobCount()`, `GetSubmittedCount()`, `GetCompletedCount()` must exist.

---

## Problem Statement

`dia.jobs.*` metrics were specced in DiaObservation Feature #11 (domain-metric-registration) but deferred because `JobSystem` lived in `DiaCore`, making it impossible for `JobSystemModule` to read `DiaCore` stats and register them via `DiaObservation` without a circular dependency. Feature 1 removes that blocker.

---

## Solution Overview

`JobSystemModule::DoStart` registers four metrics with `MetricRegistry::Instance()`. `DoUpdate` reads the four accessors from `mJobSystem` and updates them. `DoStop` nulls the pointers. No changes to `DiaThreading` or `DiaObservation` themselves.

---

## Metric Definitions

| Metric | Type | Registration | Update | Buckets |
|--------|------|-------------|--------|---------|
| `dia.jobs.queue_depth` | Gauge | `DoStart` | Per-frame: `GetQueueDepth()` | — |
| `dia.jobs.submitted` | Counter | `DoStart` | Per-frame: delta from `GetSubmittedCount()` | — |
| `dia.jobs.completed` | Counter | `DoStart` | Per-frame: delta from `GetCompletedCount()` | — |
| `dia.jobs.active_workers` | Gauge | `DoStart` | Per-frame: `GetActiveJobCount()` | — |

---

## Acceptance Criteria

| ID | Criterion | Verification |
|----|-----------|-------------|
| AC1 | After `JobSystemModule::DoStart`, `MetricRegistry::Instance().FindGauge("dia.jobs.queue_depth")` returns non-null | Unit test |
| AC2 | `dia.jobs.submitted` Counter increments when jobs are submitted | Unit test |
| AC3 | `dia.jobs.completed` Counter increments when jobs finish | Unit test |
| AC4 | `dia.jobs.active_workers` Gauge reflects the current active job count | Unit test |
| AC5 | `metrics-final.json` contains all four `dia.jobs.*` entries after a CluicheTest run | Integration test |
| AC6 | `dia run googletest` passes | Build + test |

---

## Files Touched

| File | Change |
|------|--------|
| `Cluiche/CluicheGameBaseline/Modules/JobSystemModule.h` | Add metric field declarations (Gauge×2, Counter×2); add `mPrevSubmitted`, `mPrevCompleted` delta tracking |
| `Cluiche/CluicheGameBaseline/Modules/JobSystemModule.cpp` | Add `#include <DiaObservation/Metric/MetricRegistry.h>` etc.; register in `DoStart`, update in `DoUpdate`, null in `DoStop` |
| `docs/specs/systems/dia/diaobservation.md` | Note Feature #11 `dia.jobs.*` tasks now unblocked and completed here |
| `docs/BACKLOG.md` | Remove DiaThreading metrics loose end |

---

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for identifiers | Metric names are `StringCRC` values. |
| SD-O17 | Counter uses per-thread shards | `Counter::Inc()` already implements this. Delta tracking (prev/current) on main thread; correct. |

---

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Add metric fields + delta vars to `JobSystemModule.h` | — | Planned | haiku | |
| 2 | Register + update + null metrics in `JobSystemModule.cpp` | AC1–AC4 | Planned | haiku | Pattern identical to KernelModule/AssetServiceModule |
| 3 | Build + test verification | AC6 | Planned | haiku | |
