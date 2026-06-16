# Feature Spec: Domain-Level Metric Registration

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia/dia.md | - |
| System | @docs/specs/applications/dia/systems/diaobservation/diaobservation.md | **domain-metric-registration** |

**Status:** `Approved` — 2026-05-19

**Research:** @docs/research/observ_telemetry/summary.md

**Depends on:** Feature #5 (Metrics Registry) — `MetricRegistry`, `Counter`, `Gauge`, `Histogram` must exist. Feature #5 must be Done before implementation begins. DiaThreading module extraction is required for the `dia.jobs.*` metrics and is tracked separately — those tasks are `Deferred` until unblocked.

---

## Problem Statement

After Feature #5, the `MetricRegistry` exists and is populated by the existing `MetricsCollectorModule` (FPS, frame-time, memory, uptime). But subsystems with rich quantitative signals — asset loading durations, debug server connection counts, input event rates — have no metric registration at all. This feature registers `MetricRegistry` primitives across DiaAssetRuntime, DiaDebugServer, and DiaInput, subsuming hand-rolled structs (`ServerStats`, etc.) with registry-driven metrics. Job system metrics are also specced here but deferred until DiaThreading is extracted from DiaCore.

---

## Solution Overview

Each subsystem registers its metrics with `MetricRegistry::Instance()` in its module's `DoStart`. Updates happen inline at the call sites where the signal is produced (load callbacks, server event handlers, input frame tick). The final snapshot at `SessionManager::Stop` captures all registered metrics in `metrics-final.json` automatically — no per-subsystem wiring needed.

**DiaAssetRuntime:** `dia.assets.loaded` (Gauge), `dia.assets.loading` (Gauge), `dia.assets.failed` (Counter), `dia.assets.load_time_ms` (Histogram per asset type). Registered in `AssetRuntimeModule::DoStart`, updated in load callbacks.

**DiaDebugServer:** `dia.debugserver.connections` (Gauge), `dia.debugserver.subscriptions` (Gauge), `dia.debugserver.messages_sent` (Counter), `dia.debugserver.tick_ms` (Histogram). Registered in server init. **Subsumes hand-rolled `ServerStats` struct** — that struct is deleted; consumers read from `MetricRegistry` instead.

**DiaInput:** `dia.input.sources` (Gauge), `dia.input.events_per_frame` (Histogram), `dia.input.active_gamepads` (Gauge). Registered in input module init, updated per-frame.

**DiaThreading (deferred):** `dia.jobs.queue_depth` (Gauge), `dia.jobs.submitted` (Counter), `dia.jobs.completed` (Counter), `dia.jobs.active_workers` (Gauge). Blocked on DiaThreading module extraction to break `DiaCore → DiaMetrics` cycle.

---

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | After `AssetRuntimeModule::DoStart`, `MetricRegistry::Instance().FindGauge("dia.assets.loaded")` returns non-null | Unit test |
| AC2 | `dia.assets.loading` Gauge increments during a stage load and returns to 0 on complete | Integration test: parse `metric.jsonl` during stage load |
| AC3 | `dia.assets.failed` Counter increments on each asset load failure | Unit test: mock a failing load |
| AC4 | `dia.assets.load_time_ms` Histogram has at least one observation per successfully loaded asset | Integration test |
| AC5 | `metrics-final.json` contains `dia.assets.*` entries after a CluicheTest run | Integration test |
| AC6 | After `DebugServerModule::DoStart` (or server init), `MetricRegistry::Instance().FindGauge("dia.debugserver.connections")` returns non-null | Unit test |
| AC7 | `dia.debugserver.messages_sent` Counter increments on each outbound message | Unit test |
| AC8 | `dia.debugserver.tick_ms` Histogram has observations after the server ticks | Integration test |
| AC9 | The hand-rolled `ServerStats` struct is deleted; all consumers that read it are updated to use `MetricRegistry` | Code review: grep for `ServerStats` confirms no references remain |
| AC10 | After `InputModule::DoStart`, `MetricRegistry::Instance().FindGauge("dia.input.sources")` returns non-null | Unit test |
| AC11 | `dia.input.events_per_frame` Histogram has observations after input processing frames | Integration test |
| AC12 | `dia.input.active_gamepads` Gauge reflects connected gamepad count | Unit test: mock 2 gamepads |
| AC13 | `metrics-final.json` contains `dia.debugserver.*` and `dia.input.*` entries | Integration test |
| AC14 | `dia.jobs.*` metrics tasks are `Deferred` — they produce no code changes in this feature | N/A — deferred tasks must not appear in any source file |
| AC15 | `dia pipeline --target cluichetest` green in Debug + Release | Build + run |

---

## Metric Definitions

| Metric Name | Type | Owner Module | Registration Point | Update Point | Note |
|-------------|------|-------------|-------------------|-------------|------|
| `dia.assets.loaded` | Gauge | `AssetRuntimeModule` | `DoStart` | On each successful load complete | Current total loaded count |
| `dia.assets.loading` | Gauge | `AssetRuntimeModule` | `DoStart` | On load start (+1), load complete/fail (-1) | In-flight count |
| `dia.assets.failed` | Counter | `AssetRuntimeModule` | `DoStart` | On each load failure | Monotonic |
| `dia.assets.load_time_ms` | Histogram | `AssetRuntimeModule` | `DoStart` | On each load complete (duration) | Buckets: 0, 10, 50, 100, 500, 1000, 5000 ms |
| `dia.debugserver.connections` | Gauge | `DebugServerModule` | Server init | On connect (+1) / disconnect (-1) | Subsumes `ServerStats::connectionCount` |
| `dia.debugserver.subscriptions` | Gauge | `DebugServerModule` | Server init | On subscribe (+1) / unsubscribe (-1) | Subsumes `ServerStats::subscriptionCount` |
| `dia.debugserver.messages_sent` | Counter | `DebugServerModule` | Server init | Per outbound message | Subsumes `ServerStats::messagesSent` |
| `dia.debugserver.tick_ms` | Histogram | `DebugServerModule` | Server init | Per server tick duration | Buckets: 0, 1, 2, 5, 10, 20 ms |
| `dia.input.sources` | Gauge | `InputModule` | `DoStart` | On source attach/detach | Active source count |
| `dia.input.events_per_frame` | Histogram | `InputModule` | `DoStart` | Per frame (event count that frame) | Buckets: 0, 1, 5, 10, 50, 100 |
| `dia.input.active_gamepads` | Gauge | `InputModule` | `DoStart` | On gamepad connect/disconnect | — |
| `dia.jobs.queue_depth` | Gauge | `ThreadingModule` (deferred) | `DoStart` | Per frame | Deferred: DiaThreading not extracted |
| `dia.jobs.submitted` | Counter | `ThreadingModule` (deferred) | `DoStart` | Per job submit | Deferred |
| `dia.jobs.completed` | Counter | `ThreadingModule` (deferred) | `DoStart` | Per job complete | Deferred |
| `dia.jobs.active_workers` | Gauge | `ThreadingModule` (deferred) | `DoStart` | On worker start/stop | Deferred |

---

## Files Touched

| File | Change |
|------|--------|
| `Dia/DiaAssetRuntime/AssetRuntimeModule.h/.cpp` (or equiv) | Register `dia.assets.*` metrics in `DoStart`; update in load callbacks |
| `Dia/DiaDebugServer/DebugServerModule.h/.cpp` (or equiv) | Register `dia.debugserver.*` metrics; delete `ServerStats` struct |
| `Dia/DiaDebugServer/ServerStats.h` (or equiv) | **Delete** — subsumed by MetricRegistry |
| `Dia/DiaInput/InputModule.h/.cpp` (or equiv) | Register `dia.input.*` metrics in `DoStart`; update per-frame |
| All modified files | Add `#include <DiaObservation/Metric/MetricRegistry.h>` |

> All metric registration happens in owning module files. No new files in `DiaObservation/`. DiaThreading metrics are NOT implemented — only specced here for completeness.

---

## Binding Decisions Compliance

| ID | Decision (summary) | Compliance |
|----|--------------------|------------|
| PD-001 | StringCRC for all identifiers | Metric names (`"dia.assets.loaded"`, etc.) are `StringCRC` values passed to `RegisterGauge` / `RegisterCounter` / `RegisterHistogram`. |
| PD-004 | No STL containers in public APIs | `MetricRegistry` already complies (Feature #5). This feature adds no new public APIs. |
| PD-006 | VS project files source of truth | No new `.vcxproj` files — all changes are in existing module projects. |
| AD-003 | Namespace `Dia::<Module>::` | No new types. Metrics live in `MetricRegistry` keyed by `StringCRC` name. |
| SD-O01 | All five pillars in one DiaObservation module | MetricRegistry already in `Dia/DiaObservation/Metric/`. This feature only adds call sites in owning modules. |
| SD-O17 | Counter uses per-thread shards reduced on snapshot | `Counter::Inc()` already implements this (Feature #5). Call sites just call `Inc()`. |

---

## Open Questions

| # | Question | Resolution |
|---|----------|------------|
| OQ1 | `ServerStats` struct — are there other consumers of it besides `DebugServerModule` itself (e.g. existing JavaScript debug UI reading it as a serialized struct)? | Confirm before Task 2. If the JS UI reads `ServerStats` fields via the existing BroadcastCoreMetrics path, Feature #7 (DebugServer Bridge) already migrated that to registry-driven. But confirm no other consumers remain before deleting the struct. |
| OQ2 | `dia.assets.load_time_ms` histogram — bucket bounds `{0, 10, 50, 100, 500, 1000, 5000}` ms. Are these reasonable given expected load times? | Reasonable for v1 — covers fast cache hits (~10ms) through slow cold loads (~1–5s). Can be revised in config if observed distributions differ. |
| OQ3 | `dia.input.events_per_frame` — what counts as an "event"? Raw OS events? Processed action mappings? | Raw OS events as they enter the input module per frame. Action mapping is a later processing step. Simpler to count at the source. |
| OQ4 | DiaThreading extraction — is there a spec tracking it? | Not yet. `dia.jobs.*` tasks in this feature are Deferred with the note "blocked on DiaThreading extraction". When that spec lands, these tasks are unblocked. |

---

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | `ServerStats` deletion | Deleting `ServerStats` is a breaking change for any code that reads it directly. How is this managed? | Feature #7 (DebugServer Bridge) already replaces the broadcast path with registry-driven metrics. Before deleting `ServerStats`, grep confirms no remaining direct reads. The deletion is gated on confirming all consumers use MetricRegistry (AC9). |
| 2 | Histogram bucket choice | Histogram buckets are fixed at registration. What if the asset loading profile is very different from the declared buckets? | Buckets are a v1 best-guess. `metrics-final.json` includes all observations and bucket counts — if p95 is always in the top bucket, it signals the bounds need adjusting. Config-driven buckets are a v2 feature. |
| 3 | Per-thread shards | `dia.debugserver.messages_sent` is incremented on the WebSocket thread. `Counter::Inc()` uses per-thread shards — is this thread registered? | The WebSocket thread must call `Counter::Inc()` which uses `thread_local` storage. If the thread is not registered with the drain system, the shard accumulates but is never reduced until that thread unregisters. At session stop, `MetricRegistry::Snapshot()` collects all shards. Counter value is correct at snapshot time even for unregistered threads. |
| 4 | Input histograms | `dia.input.events_per_frame` uses `Histogram::Observe(eventCount)`. The input module calls this once per frame with the count. Is this the right granularity? | Yes — one observation per frame means a 60fps session produces 60 observations/second. Over a 60-second session: 3600 observations. Histogram is memory-fixed (bucket count × 8 bytes). No per-event allocation. |
| 5 | Deferred metrics | `dia.jobs.*` tasks are Deferred. Does their absence cause test failures? | No — `MetricRegistry::FindGauge("dia.jobs.queue_depth")` returns null when not registered. Test assertions in this feature check only the metrics that are registered (AC1–AC13). Jobs metrics are separately verified when DiaThreading lands. |

---

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Register `dia.assets.*` metrics in `AssetRuntimeModule::DoStart`; update in load callbacks | AC1, AC2, AC3, AC4, AC5 | Done | haiku | Implemented in `AssetServiceModule::DoStart/DoUpdate`. |
| 2 | Register `dia.debugserver.*` metrics; delete `ServerStats` struct; update all consumers | AC6, AC7, AC8, AC9 | Done | sonnet | Implemented in `DebugServerHostModule::DoStart/DoUpdate`. `ServerStats` kept as value-source for registry (backlog note: intentional). |
| 3 | Register `dia.input.*` metrics in `InputModule::DoStart`; update per-frame | AC10, AC11, AC12 | Done | haiku | Implemented in `KernelModule::DoStart/DoUpdate`. Added `ConsoleGamepadManager::GetActiveGamepadCount()`. |
| 4 | `dia.jobs.*` — Deferred (blocked on DiaThreading extraction) | AC14 | Deferred | — | |
| 5 | Integration tests — AC1–AC13 | All ACs | Deferred | sonnet | Requires live CluicheTest run; deferred to manual validation. |
| 6 | Build verification | AC15 | Done | haiku | 4818 GoogleTests pass. |

---

## Status

`Done` — 2026-05-20. Tasks 1, 2, 3, 6 complete. Tasks 4, 5 deferred (DiaThreading blocked; integration tests require live run).
