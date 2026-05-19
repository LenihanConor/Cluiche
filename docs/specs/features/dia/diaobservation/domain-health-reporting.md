# Feature Spec: Domain-Level Health Reporting

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diaobservation.md | **domain-health-reporting** |

**Status:** `Approved` — 2026-05-19

**Research:** @docs/research/observ_telemetry/summary.md

**Depends on:** Feature #6 (DiaHealth Reporting) — `IHealthReporter`, `HealthReporterBase`, `HealthRegistry`, and `DIA_OBSERVATION_ASSERT`/`FAIL` macros must exist. Feature #6 must be Done before implementation begins.

---

## Problem Statement

After Feature #6, the health infrastructure exists — `IHealthReporter`, `HealthRegistry`, `health.json`, `session.json` modules array — but only the modules that explicitly implement `IHealthReporter` and register with `HealthRegistry` will appear in health reports. Currently no subsystem does this. A session may exit with `overall_status: "ok"` even if the render canvas is null, the asset catalog failed to load, or a stream is permanently dropped. This feature adds `IHealthReporter` implementations to the subsystems that have observable degradation/failure modes: module lifecycle, render, asset runtime, streams, debug server, frames, and application-level.

---

## Solution Overview

Each reporter subclasses `HealthReporterBase` (from Feature #6) rather than `IHealthReporter` directly — `SetFailing`/`SetDegraded`/`SetOK` are already thread-safe. Reporters are registered with `HealthRegistry::Instance()` in their owning module's `DoStart` and unregistered in `DoStop`.

**Tier 1 (critical — must ship with this feature):**
- `ModuleLifecycleReporter` — generic reporter embedded in base `Module`; Degraded if `kStarting` > 50% of `startTimeoutMs`; Failing if `kFailed` or timeout exceeded. This is the "free" reporter every module gets.
- `RenderHealthReporter` — canvas availability and frame flow.
- `AssetHealthReporter` — stage load state and handler registration.
- `StreamHealthReporter` — per-EventStreamStore dropped event ratio.

**Tier 2 (important — ship with this feature):**
- `DebugServerReporter` — connection count and message drop rate.
- `FrameStreamReporter` — frame staleness detection.
- `ApplicationReporter` — module failure cascade and stuck transitions.

**Tier 3 (best effort — defer if unblocked by missing subsystems):**
- `TimeServerReporter` — deltaTime stability.
- `JobSystemReporter` — queue depth (blocked on DiaThreading extraction).
- `AnimationReporter` — evaluator/clip state (blocked on DiaAnimation2D).

**Threshold configuration:** Thresholds (e.g. Degraded after >3 frames with null canvas pointer, Failing after >5s stuck transition) are compile-time constants in v1. The `.diagame` `observation.health` block will carry them in v2. The field names must not foreclose this.

---

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | `ModuleLifecycleReporter` is registered by every `Module` subclass automatically; it appears in `health.json` `modules` array after a CluicheTest run | Integration test: run cluichetest, parse `health.json` |
| AC2 | `ModuleLifecycleReporter::Report()` returns `kDegraded` if a module has been in `kStarting` state for >50% of `startTimeoutMs` | Unit test: mock module stuck in kStarting |
| AC3 | `ModuleLifecycleReporter::Report()` returns `kFailing` if module is in `kFailed` or `kStarting` past full `startTimeoutMs` | Unit test |
| AC4 | `RenderHealthReporter` is registered by the render module; appears in `health.json` | Integration test |
| AC5 | `RenderHealthReporter::Report()` returns `kDegraded` when `FetchLatest()` returns nullptr for >3 consecutive frames | Unit test: mock 4 consecutive null frames |
| AC6 | `RenderHealthReporter::Report()` returns `kFailing` when `mCanvas == nullptr` after initialization timeout | Unit test |
| AC7 | `AssetHealthReporter` is registered by the asset runtime module; appears in `health.json` | Integration test |
| AC8 | `AssetHealthReporter::Report()` returns `kDegraded` when loading exceeds expected duration by >2s | Unit test |
| AC9 | `AssetHealthReporter::Report()` returns `kFailing` on `StageLoadState::kFailed` or manifest parse failure | Unit test |
| AC10 | `StreamHealthReporter` is registered per-EventStreamStore; appears in `health.json` for each stream | Integration test |
| AC11 | `StreamHealthReporter::Report()` returns `kDegraded` when dropped event ratio >10% over last 100 events | Unit test |
| AC12 | `StreamHealthReporter::Report()` returns `kFailing` when `kFailLoudRejected` policy fires or `reader_count == 0` with active writers | Unit test |
| AC13 | `DebugServerReporter` reports `kDegraded` when message drop rate exceeds threshold | Unit test |
| AC14 | `ApplicationReporter` reports `kFailing` if any module reaches `kFailed` (cascade indicator) | Unit test: mock one module failing |
| AC15 | `ApplicationReporter` reports `kDegraded` if any lifecycle transition is stuck >5s | Unit test |
| AC16 | All reporters unregister from `HealthRegistry` in `DoStop`; `health.json` at stop captures final state | Integration test |
| AC17 | `health.json` `overall_status` reflects worst-case across all reporters (Tier 1 + 2) | Integration test |
| AC18 | `DIA_OBSERVATION_ASSERT` used in stream overflow path flips `StreamHealthReporter` to `kFailing` and logs an error record | Unit test |
| AC19 | `dia pipeline --target cluichetest` green in Debug + Release | Build + run |

---

## Reporter Definitions

| Reporter | Tier | Owner Module | Signals | Degraded Threshold | Failing Threshold |
|----------|------|-------------|---------|-------------------|-------------------|
| `ModuleLifecycleReporter` | 1 | Base `Module` (all modules get one) | `ModuleState`, `startElapsedMs` | kStarting > 50% of `startTimeoutMs` | `kFailed` or kStarting > `startTimeoutMs` |
| `RenderHealthReporter` | 1 | Render module | `FetchLatest()` null count, `mCanvas` null | `FetchLatest` nullptr > 3 consecutive frames | `mCanvas` nullptr after timeout, GL fence stuck > 5s |
| `AssetHealthReporter` | 1 | Asset runtime module | `StageLoadState`, load elapsed | kLoading > 2s beyond expected | `kFailed`, manifest parse failure |
| `StreamHealthReporter` | 1 | Per-EventStreamStore | dropped event count / total sent | dropped ratio > 10% over last 100 | `kFailLoudRejected` or reader_count == 0 with active writers |
| `DebugServerReporter` | 2 | Debug server module | connection count, message drop rate | message drop rate > threshold | — |
| `FrameStreamReporter` | 2 | Frame stream module | `FetchLatest` staleness | frame > N frames stale | frame stall > 5s |
| `ApplicationReporter` | 2 | Application root | module failure cascade, stuck transitions | any transition stuck > 5s | any module `kFailed` |
| `TimeServerReporter` | 3 | Time server module | deltaTime variance | deltaTime > 2× expected | — |
| `JobSystemReporter` | 3 | Job system module | queue depth | queue depth > 80% capacity | queue full for > 1s |
| `AnimationReporter` | 3 | Animation module | evaluator/clip state | clip missing from manifest | evaluator in error state |

---

## Files Touched

| File | Change |
|------|--------|
| `Dia/DiaApplicationFlow/Module.h/.cpp` (or equiv) | Add `ModuleLifecycleReporter` embedded reporter; register/unregister in DoStart/DoStop |
| `Dia/DiaGraphics/RenderModule.h/.cpp` (or equiv) | Add `RenderHealthReporter`; register in DoStart |
| `Dia/DiaAssetRuntime/AssetRuntimeModule.h/.cpp` (or equiv) | Add `AssetHealthReporter`; register in DoStart |
| `Dia/DiaStream/EventStreamStore.h/.cpp` (or equiv) | Add `StreamHealthReporter`; register on construction |
| `Dia/DiaDebugServer/DebugServerModule.h/.cpp` (or equiv) | Add `DebugServerReporter`; register in DoStart |
| Application-level module file | Add `ApplicationReporter`; register in DoStart |
| All reporter header files | New files in owning module's directory, NOT in DiaObservation |
| All modified files | Add `#include <DiaObservation/Health/IHealthReporter.h>` and `<DiaObservation/Health/HealthReporterBase.h>` |

> Reporter implementation files live in their owning subsystem directory, not in `DiaObservation/`. `IHealthReporter` and `HealthReporterBase` headers are the only DiaObservation dependency.

---

## Binding Decisions Compliance

| ID | Decision (summary) | Compliance |
|----|--------------------|------------|
| PD-001 | StringCRC for all identifiers | Reporter names, reason codes (e.g. `kReasonCanvasNull`, `kReasonLoadTimeout`) are all `StringCRC`. |
| PD-002 | ProcessingUnit/Phase/Module architecture | Reporters are attached to `Module` subclasses; lifecycle (Register in DoStart, Unregister in DoStop) follows the Module pattern. |
| PD-004 | No STL containers in public APIs | `HealthReporterBase` already complies. Reporter implementations use only DiaCore containers and engine state references. |
| AD-003 | Namespace `Dia::<Module>::` | Reporters live in their owning module's namespace (e.g. `Dia::Graphics::`, `Dia::AssetRuntime::`), not in `Dia::Observation::`. |
| SD-O10 | Health polled at exit AND on crash | Handled by Feature #6's `SessionManager` — reporters registered here will automatically be polled. |
| SD-O11 | Health duplicated across three sinks | Handled by Feature #6's infrastructure — no per-reporter action needed. |
| SD-O12 | `DIA_OBSERVATION_ASSERT`/`FAIL` do not crash | `StreamHealthReporter` uses these macros for loud failure signalling. |
| SD-O20 | Test utilities in `DiaObservation/Testing/` | No new test utilities needed — `HealthFixture.h` from Feature #6 is sufficient. |

---

## Open Questions

| # | Question | Resolution |
|---|----------|------------|
| OQ1 | `ModuleLifecycleReporter` — does base `Module` currently have a `startTimeoutMs` field? | Confirm before Task 1. If not, add it as a field with a default (e.g. 5000ms). The reporter reads it via a reference or getter. |
| OQ2 | `RenderHealthReporter` — `FetchLatest()` returning nullptr 3+ consecutive frames. How does the reporter track "consecutive" without being called per-frame? | The reporter uses a `std::atomic<uint32_t>` null-frame counter. The render module increments it each frame `FetchLatest` returns nullptr and resets it when non-null. The reporter reads the counter atomically in `Report()`. |
| OQ3 | `StreamHealthReporter` — per-EventStreamStore instance. If there are 50 streams, there are 50 reporters in `health.json`. Is this acceptable? | Yes — `health.json` is a debugging artifact, not a dashboard. 50 reporters at ~100 bytes each = ~5KB. Acceptable. Future v2 can roll up per-stream health into a single `StreamsHealthReporter`. |
| OQ4 | Tier 3 reporters blocked on DiaThreading and DiaAnimation2D — how are they handled? | Tasks for Tier 3 reporters are `Deferred` until the blocking subsystems exist. This feature is complete once Tier 1 and Tier 2 reporters are done. |

---

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Threading | `Report()` is called from the `SessionManager::Tick` thread and the crash handler thread. Are all reporters thread-safe? | `HealthReporterBase` uses `std::atomic` for status, errors, warnings. Subclass reporters must not access non-atomic module state in `Report()` — they must cache observable signals in atomics updated by the owning thread. `mNullFrameCount` in `RenderHealthReporter` is `std::atomic<uint32_t>`, updated by the render thread, read by the poll thread. |
| 2 | `ModuleLifecycleReporter` | Every module gets one, potentially creating many reporters in `health.json`. Is this the right granularity? | Yes — per-module health is the correct granularity for E2E pass/fail diagnosis. `session.json` `modules` array already expects per-module entries. Future rollup is additive, not a redesign. |
| 3 | Cascades | `ApplicationReporter` reports `kFailing` if ANY module is `kFailed`. Doesn't `ModuleLifecycleReporter` already report that per-module? | Yes — both exist for different consumers. `ModuleLifecycleReporter` tells you WHICH module failed. `ApplicationReporter` tells you the application-level health in one check (useful for CI: "is this session healthy?" = one predicate). |
| 4 | Tier 3 | `JobSystemReporter` is blocked on DiaThreading extraction. Does this block the feature? | No — Tier 1 + 2 reporters are independent of DiaThreading. Tier 3 tasks are explicitly Deferred. The feature is marked Done when Tier 1 + 2 are complete and verified. |
| 5 | StreamHealthReporter | One reporter per EventStreamStore could mean dozens of reporters. How does `HealthRegistry` scale? | `HealthRegistry` uses a `DynamicArrayC` (DiaCore container). Up to 64 reporters is trivially fast to poll (linear scan). If stream count exceeds this, the registry capacity must be configured or a rolled-up reporter used. Document 64 as the v1 soft limit. |

---

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | `ModuleLifecycleReporter` — implement + embed in base Module; register/unregister in DoStart/DoStop | AC1, AC2, AC3 | Planned | sonnet | Confirm `startTimeoutMs` field per OQ1 |
| 2 | `RenderHealthReporter` — implement + register in render module | AC4, AC5, AC6 | Planned | sonnet | Null-frame counter per OQ2 |
| 3 | `AssetHealthReporter` — implement + register in asset runtime module | AC7, AC8, AC9 | Planned | sonnet | |
| 4 | `StreamHealthReporter` — implement + register per-EventStreamStore | AC10, AC11, AC12, AC18 | Planned | sonnet | One per store instance; DIA_OBSERVATION macros |
| 5 | `DebugServerReporter` — implement + register | AC13 | Planned | haiku | |
| 6 | `FrameStreamReporter` — implement + register | — | Planned | haiku | |
| 7 | `ApplicationReporter` — implement + register | AC14, AC15 | Planned | sonnet | |
| 8 | `TimeServerReporter` — Deferred (blocked on subsystem) | — | Deferred | — | |
| 9 | `JobSystemReporter` — Deferred (blocked on DiaThreading extraction) | — | Deferred | — | |
| 10 | `AnimationReporter` — Deferred (blocked on DiaAnimation2D) | — | Deferred | — | |
| 11 | Integration tests — AC1–AC18 | All ACs | Planned | sonnet | Requires CluicheTest run |
| 12 | Build verification | AC19 | Planned | haiku | |

---

## Status

`Approved` — 2026-05-19. Steps 3 (Binding Decisions) and 4 (AI Review Questions) complete and confirmed.
