# System Spec: DiaMetrics

## Parent Application
@docs/specs/applications/dia.md

**Research:** @docs/research/observ_telemetry/summary.md

## Summary

DiaMetrics is a standalone, general-purpose metric primitive library for the Dia engine. It provides `Counter`, `Gauge`, and `Histogram` primitives registered by `StringCRC` name with a `MetricRegistry` singleton. Any engine system, module, or game code can register and update metrics without any observation or session dependency.

DiaMetrics was extracted from the original DiaObservation design (which planned one module for all four pillars). Logs, traces, and health are intrinsically session-aware — a span without a session ID has no `trace_id`. Metrics are not: a frame-time counter is useful for profiling, gameplay stats, and editor live panels whether or not an observation session is running. The separation keeps `DiaMetrics` as a lightweight primitive layer (`DiaCore` only dep) reusable by any system.

**Dependency chain:** `DiaMetrics → DiaCore`

`DiaObservation` depends on `DiaMetrics` and adds `MetricsFileSink` — the session-aware sink that stamps snapshots with `session_id` + epoch offset and writes `metric.jsonl` / `metrics-final.json`.

`DiaDebugServer` depends on `DiaMetrics` directly for live WebSocket forwarding via `ObservationBridge`.

## Responsibilities

**Owns:**
- `MetricRegistry` singleton — register-once, update-anywhere; available before any session starts
- `Counter` — monotonic, per-thread shard incremented lock-free; reduced on snapshot
- `Gauge` — last-written value, atomic store/load
- `Histogram` — fixed-bucket distribution; p50/p95/p99 computed at snapshot time via linear interpolation
- `MetricSnapshot` — fixed-array snapshot struct carrying all registered primitives at a point in time
- `IMetricSink` — interface for snapshot consumers (file, WebSocket, future OTLP exporter)
- `Testing/MetricFixture.h` — test utilities

**Does NOT own:**
- Session identity, timestamps, `session_id` stamping — `DiaObservation` adds these in `MetricsFileSink`
- File output (`metric.jsonl`, `metrics-final.json`) — `DiaObservation`
- Snapshot scheduling / timer — `SessionManager::Tick` in `DiaObservation`
- WebSocket forwarding — `DiaDebugServer::ObservationBridge`
- Per-component or per-entity metrics aggregation — future work

## Features

| # | Feature | Description | Spec | Status |
|---|---------|-------------|------|--------|
| 1 | DiaMetrics Registry | `MetricRegistry`, `Counter` (per-thread shard + reduce), `Gauge` (atomic), `Histogram` (fixed buckets, p50/p95/p99 at snapshot), `MetricSnapshot`, `IMetricSink`. Subsumes `MetricsCollectorModule` internals (FPS, frame-time, memory, uptime become registered gauges). | [metrics-registry.md](../../features/dia/diaobservation/metrics-registry.md) | Approved |

## Dependencies

| Dependency | What DiaMetrics uses from it |
|------------|------------------------------|
| DiaCore | `StringCRC` (metric names), `DynamicArrayC` (registry lists) |

No dependencies on `DiaObservation`, `DiaApplicationFlow`, `DiaDebugServer`, or any other Dia module.

## Dependents

| Dependent | What they use |
|-----------|---------------|
| DiaObservation | `MetricRegistry`, `IMetricSink` — adds `MetricsFileSink` for session file output |
| DiaApplicationFlow | `MetricRegistry` — `MetricsCollectorModule` registers FPS/frame-time/memory/uptime gauges |
| DiaDebugServer | `MetricRegistry`, `IMetricSink` — `ObservationBridge` implements `IMetricSink` for live WebSocket forwarding |
| Any future system | `MetricRegistry` — profiling counters, gameplay stats, budget gauges |

## Decisions

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| SD-MET-001 | `MetricRegistry` is a singleton available before any session starts | Modules register metrics in `DoInit`; session starts later in `DoStart`. A registry that requires session setup would force awkward two-phase registration. | All features | Accepted | Yes |
| SD-MET-002 | `Counter` uses per-thread shards, reduced on snapshot | Per-thread increment is lock-free (~5 ns); a shared atomic on a 10 kHz hot path would cause contention. Snapshot reduction happens at most every 100ms — cost amortised to zero on the write path. | All features | Accepted | Yes |
| SD-MET-003 | `Gauge` uses `std::atomic<double>` — lock-free on x64 MSVC C++20 | Last-writer-wins semantics; no history needed. Atomic store/load is ~5 ns. | All features | Accepted | Yes |
| SD-MET-004 | `Histogram` copies up to 16 bucket bounds at registration (caller-supplied array) | Caller can pass a stack-local `float[]`; registry stores its own copy. Max 16 buckets sufficient for engine histograms (frame-time, asset-load-time). Avoids any pointer lifetime dependency on caller memory. | All features | Accepted | Yes |
| SD-MET-005 | `MetricSnapshot` uses fixed arrays (`MetricEntry[128]`, `BucketEntry[17]`); no STL containers | Consistent with PD-004 (no STL in public APIs). 128 metric entries is ample for v1 — engine today has ~4. | All features | Accepted | Yes |
| SD-MET-006 | Counter shard registration is lazy (first `Inc()` call registers the shard) | No explicit `RegisterThread` needed for metrics; reduces friction for producers. Thread exit unregisters via `thread_local` RAII guard. | All features | Accepted | Yes |
| SD-MET-007 | `DiaCore` is the only dependency | Ensures `DiaMetrics` is usable from any system without pulling observation infrastructure. A future OTLP exporter or profiling overlay depends on `DiaMetrics` directly. | All features | Accepted | Yes |
| SD-MET-008 | Snapshot is not atomic across all primitives | Each primitive (counter, gauge, histogram) is read independently. Nanosecond-level cross-metric races are irrelevant for engine telemetry. A global snapshot lock would be a correctness theatre at real cost. | All features | Accepted | Yes |

**Status values:** `Proposed` · `Accepted` · `Rejected` · `Superseded`
**Binding:** `Yes` = enforced constraint on all child features · `No` = guidance only

## Inherited Binding Decisions

| ID | Source | Decision | Implication for this system |
|----|--------|----------|----------------------------|
| PD-001 | Platform | StringCRC for all entity/component IDs | Metric names are `StringCRC`. No raw string maps in public API. |
| PD-004 | Platform | No STL containers in public APIs | `MetricSnapshot` uses fixed arrays. `MetricRegistry` public API uses only `StringCRC`, `const float*`, `unsigned int`, primitive pointers. |
| PD-005 | Platform | x64 only | `std::atomic<double>`, `thread_local` — both lock-free and well-supported on x64 MSVC. |
| PD-006 | Platform | Visual Studio project files source of truth | `DiaMetrics.vcxproj` + `.vcxproj.filters` created manually; registered in `Cluiche.sln`. |
| PD-007 | Platform | C++20 required | `std::atomic<double>` lock-free guarantee is a C++20 / MSVC 19.29+ commitment. |
| PD-008 | Platform | Directory.Build.props owns OutDir/IntDir/toolchain | `DiaMetrics.vcxproj` must NOT override `OutDir`, `IntDir`, `PlatformToolset`, `LanguageStandard`. |
| AD-001 | Dia App | Module system with YAML frontmatter | `dia.dia.metrics.architecture.module.md` required before implementation. |
| AD-002 | Dia App | No STL containers in public APIs | Reinforces PD-004. |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | All code in `Dia::Metric::`. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Scope | Is `DiaMetrics` a system spec or just a module doc? | System spec — it has a feature table, decisions, and dependents. The single feature (metrics-registry.md) is already Approved as part of DiaObservation speccing. |
| 2 | Naming | Namespace `Dia::Metric::` vs `Dia::Metrics::` — which? | `Dia::Metric::` — singular, consistent with `Dia::Observation::`, `Dia::Observation::Trace::`, etc. |
| 3 | Future | Could `DiaMetrics` gain more features (e.g. named metric groups, per-frame histogram reset, rate counters)? | Yes — the spec is deliberately minimal for v1. Future features would be added as new rows in the Features table. SD-MET-005 (fixed arrays) may need revisiting if the metric count grows significantly. |
| 4 | Relationship | Does `DiaMetrics` need a feature spec of its own, or does it point at `metrics-registry.md` in DiaObservation? | Points at `metrics-registry.md` — the feature was written under DiaObservation speccing but the implementation delivers `DiaMetrics`. The spec file path is a convention; the content is the contract. |

## Status

`Approved` — 2026-05-17. Extracted from DiaObservation system design (amendment to SD-O01). All inherited binding decisions and AI review questions resolved. Single feature (metrics-registry.md) already Approved.

**Next:** Create `dia.dia.metrics.architecture.module.md` and `Dia/DiaMetrics/DiaMetrics.vcxproj` as part of Feature #5 implementation.
