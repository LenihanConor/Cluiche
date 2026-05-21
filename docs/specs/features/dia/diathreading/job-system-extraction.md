# Feature Spec: Job System Extraction

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diathreading.md | **job-system-extraction** |

**Status:** `Approved` — 2026-05-20

---

## Problem Statement

`JobSystem` and `JobHandle` live in `Dia/DiaCore/Threading/`. `DiaCore` cannot depend on `DiaObservation` (circular: both depend on `DiaCore`). This means `dia.jobs.*` metrics can never be registered from inside `DiaCore`. Extracting `JobSystem` to a new `DiaThreading` module — which has `DiaCore` as its only required dependency — unblocks `JobSystemModule` from wiring metrics via `DiaObservation`.

The extraction also adds the four metric accessors that Feature 2 (job-metrics) needs: `GetQueueDepth()`, `GetActiveJobCount()`, `GetSubmittedCount()`, `GetCompletedCount()`.

---

## Solution Overview

1. Create `Dia/DiaThreading/` as a new static library with its own `.vcxproj`
2. Move `JobSystem.h/.cpp` and `JobHandle` into `Dia/DiaThreading/`, changing namespace to `Dia::Threading::`
3. Add `std::atomic<uint64_t> mSubmittedCount` and `mCompletedCount` to `ThreadPool`; increment in `Enqueue()` and the worker lambda respectively; expose `GetSubmittedCount()`, `GetCompletedCount()`, `GetActiveJobCount()`
4. Add `GetQueueDepth()` as a public alias of `GetPendingTaskCount()` on `ThreadPool`; expose via `JobSystem`
5. Leave a forwarding header at `Dia/DiaCore/Threading/JobSystem.h` (`#include <DiaThreading/JobSystem.h>`) for one release
6. Update all direct consumers: `DiaSFML/TextureHandler.cpp`, `CluicheGameBaseline/JobSystemModule.h/.cpp`
7. Create `Dia/DiaThreading/dia.threading.architecture.module.md`
8. Register `DiaThreading` in `Dia/dia.md` systems table

---

## Acceptance Criteria

| ID | Criterion | Verification |
|----|-----------|-------------|
| AC1 | `Dia/DiaThreading/JobSystem.h` exists; namespace is `Dia::Threading::` | Code review |
| AC2 | `Dia/DiaCore/Threading/JobSystem.h` forwards to `<DiaThreading/JobSystem.h>` | Code review |
| AC3 | `JobSystem::GetQueueDepth()` returns current pending task count | Unit test |
| AC4 | `JobSystem::GetActiveJobCount()` returns count of tasks currently executing | Unit test |
| AC5 | `JobSystem::GetSubmittedCount()` is monotonically increasing; increments on each `Submit` | Unit test |
| AC6 | `JobSystem::GetCompletedCount()` increments when a job finishes | Unit test |
| AC7 | `DiaCore.vcxproj` no longer references `JobSystem.h/.cpp` | Code review |
| AC8 | `DiaThreading.vcxproj` compiles cleanly; `DiaCore` is its only required reference | Build |
| AC9 | `dia run googletest` passes (4818+) | Test run |

---

## Files Touched

| File | Change |
|------|--------|
| `Dia/DiaCore/Threading/JobSystem.h` | Replace body with forwarding `#include <DiaThreading/JobSystem.h>` + `using` alias |
| `Dia/DiaCore/Threading/JobSystem.cpp` | Delete — moved to DiaThreading |
| `Dia/DiaCore/DiaCore.vcxproj` | Remove JobSystem .h/.cpp ClInclude/ClCompile entries |
| `Dia/DiaCore/DiaCore.vcxproj.filters` | Remove JobSystem filter entries |
| `Dia/DiaThreading/JobSystem.h` | New — `Dia::Threading::JobSystem`, `JobHandle`, `JobFn` |
| `Dia/DiaThreading/JobSystem.cpp` | New — implementation, namespace updated |
| `Dia/DiaCore/Threading/ThreadPool.h` | Add `mSubmittedCount`, `mCompletedCount` atomics; expose `GetSubmittedCount()`, `GetCompletedCount()`, `GetActiveJobCount()`, `GetQueueDepth()` |
| `Dia/DiaCore/Threading/ThreadPool.cpp` | Increment `mSubmittedCount` in `Enqueue`; increment `mCompletedCount` in worker after task runs |
| `Dia/DiaThreading/DiaThreading.vcxproj` | New — static lib; references DiaCore |
| `Dia/DiaThreading/DiaThreading.vcxproj.filters` | New |
| `Dia/DiaThreading/dia.threading.architecture.module.md` | New |
| `Dia/DiaSFML/TextureHandler.cpp` | Update include path to `<DiaThreading/JobSystem.h>` |
| `Cluiche/CluicheGameBaseline/Modules/JobSystemModule.h` | Update include + type reference to `Dia::Threading::JobSystem` |
| `Cluiche/CluicheGameBaseline/Modules/JobSystemModule.cpp` | Update namespace references |
| `Cluiche/CluicheGameBaseline/CluicheGameBaseline.vcxproj` | Add DiaThreading reference |
| `Cluiche/Tests/GoogleTests/Core/Threading/TestJobSystem.cpp` | Update include + namespace |
| `docs/specs/applications/dia.md` | Add DiaThreading row to systems table |

---

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for identifiers | No new identifiers in this feature. |
| PD-006 | VS project files source of truth | New `DiaThreading.vcxproj` created; all references updated manually. |
| AD-003 | Namespace `Dia::<Module>::` | New namespace `Dia::Threading::`. Compatibility alias at old include path. |

---

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Add metric accessors to `ThreadPool` (`GetSubmittedCount`, `GetCompletedCount`, `GetActiveJobCount`, `GetQueueDepth`) | AC3–AC6 | Planned | sonnet | Atomic counters; increment in Enqueue + worker |
| 2 | Create `Dia/DiaThreading/` — new .vcxproj, .vcxproj.filters, module doc | AC8 | Planned | haiku | Static lib; DiaCore-only reference |
| 3 | Move JobSystem.h/.cpp → DiaThreading; update namespace to `Dia::Threading::` | AC1, AC7, AC8 | Planned | sonnet | Leave forwarding header at old path |
| 4 | Update all consumers (TextureHandler, JobSystemModule, TestJobSystem) | AC2, AC9 | Planned | haiku | Update includes + namespace references |
| 5 | Build verification (`dia run googletest`) | AC9 | Planned | haiku | |
