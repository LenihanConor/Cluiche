---
schema: dia.module.v1
id: dia.metrics
display_name: DiaMetrics
status: active
maturity: dev
path: Dia/DiaMetrics/
vcxproj: Dia/DiaMetrics/DiaMetrics.vcxproj

summary: >
  Standalone, general-purpose metric primitive library. Provides Counter,
  Gauge, and Histogram primitives registered by StringCRC name with a
  MetricRegistry singleton. Available before any observation session starts.

public_api:
  headers:
    - DiaMetrics/MetricRegistry.h
    - DiaMetrics/Counter.h
    - DiaMetrics/Gauge.h
    - DiaMetrics/Histogram.h
    - DiaMetrics/MetricSnapshot.h
    - DiaMetrics/IMetricSink.h
  namespaces:
    - Dia::Metric
  entry_points:
    - Dia::Metric::MetricRegistry::Instance()

dependencies:
  required:
    - dia.core

dependent_modules:
  - dia.observation      # adds MetricsFileSink
  - dia.application      # MetricsCollectorModule registers engine gauges
  - dia.debugserver      # ObservationBridge implements IMetricSink

responsibilities:
  owns:
    - MetricRegistry singleton — register-once, update-anywhere; available before any session
    - Counter — monotonic, per-thread shard incremented lock-free; reduced on snapshot
    - Gauge — last-written value, atomic store/load (std::atomic<double>)
    - Histogram — fixed-bucket distribution; p50/p95/p99 computed at snapshot via linear interpolation
    - MetricSnapshot — fixed-array snapshot struct (MetricEntry[128])
    - IMetricSink — interface for snapshot consumers (file, WebSocket, future OTLP exporter)
    - Testing/MetricFixture.h — test utilities
  does_not_own:
    - Session identity, timestamps, session_id stamping (DiaObservation adds these)
    - File output (metric.jsonl, metrics-final.json) — DiaObservation::MetricsFileSink
    - Snapshot scheduling / timer — SessionManager::Tick in DiaObservation
    - WebSocket forwarding — DiaDebugServer::ObservationBridge
    - Per-component or per-entity metrics aggregation — future work

spec: docs/specs/systems/dia/diametrics.md
feature_spec: docs/specs/features/dia/diaobservation/metrics-registry.md
---

# DiaMetrics

Lightweight metric primitive layer for the Dia engine. Any engine system, module, or game code can register and update metrics without any observation or session dependency. DiaObservation and DiaDebugServer consume the registry via `IMetricSink`.

## Design Rationale

Extracted from the original DiaObservation design because metrics are not intrinsically session-aware — a frame-time counter is useful for profiling, gameplay stats, and editor live panels whether or not an observation session is running. Keeping `DiaMetrics` as `DiaCore`-only ensures it is reusable from any system without pulling observation infrastructure.

## Key Decisions

- `MetricRegistry` uses Meyer's singleton (not `Dia::Core::Singleton<T>`) — must be available before any explicit Create() call since modules register in DoInit.
- `Counter` per-thread shards: lock-free Inc (~5 ns), reduced on snapshot. Shard registered lazily on first Inc call.
- `Gauge` uses `std::atomic<double>` — lock-free on x64 MSVC C++20.
- `Histogram` copies up to 16 bucket bounds at registration. Implicit +Inf bucket always appended.
- Snapshot is not atomic across all primitives — nanosecond-level races irrelevant for engine telemetry.
- No STL in public APIs — `MetricSnapshot` uses fixed arrays (`MetricEntry[128]`, `BucketEntry[17]`).
