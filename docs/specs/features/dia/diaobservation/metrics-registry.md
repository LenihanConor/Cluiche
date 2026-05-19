# Feature Spec: DiaMetrics Registry

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diaobservation.md | **metrics-registry** |

**Status:** `In Progress (partial)` — 2026-05-18. Tasks 1–10, 14–16 Done; Tasks 11–13 Deferred pending DiaObservation Features #1+#2. Steps 3 (Binding Decisions) and 4 (AI Review Questions) complete and confirmed. **Amended 2026-05-17** — `MetricRegistry`/primitives extracted to standalone `DiaMetrics` module; `MetricsFileSink` stays in `DiaObservation`.

**Plan:** [metrics-registry.plan.md](metrics-registry.plan.md)

**Research:** @docs/research/observ_telemetry/summary.md

**Depends on:** Feature #2 (foundation) — `SessionManager::Start` adds `MetricsFileSink` and starts snapshot timer via `Tick`. Feature #3 (config) — `sinks.metrics_file` added to `ObservationConfig`. Implementation is serial.

---

## Problem Statement

After Features #1–#4 the engine can log and trace, but has no way to track counts, live values, or distributions over time. `MetricsCollectorModule` hand-rolls FPS/frame-time/memory/uptime with no external query interface and no file output. This feature delivers `DiaMetrics` — a standalone, general-purpose module (`Counter`, `Gauge`, `Histogram`, `MetricRegistry`) usable by any system for profiling, gameplay stats, or live editor panels, with no observation dependency. `DiaObservation` adds `MetricsFileSink` on top to route snapshots to `metric.jsonl`.

---

## Solution Overview

**`DiaMetrics`** (`Dia/DiaMetrics/`) is a new standalone module depending only on `DiaCore`. It owns:
- `MetricRegistry` singleton — register-once, update-anywhere
- `Counter`, `Gauge`, `Histogram` primitives
- `MetricSnapshot`, `IMetricSink` interfaces

**`DiaObservation`** depends on `DiaMetrics` and adds `MetricsFileSink` — the session-aware sink that stamps records with `session_id` + epoch offset and writes `metric.jsonl` / `metrics-final.json`. `SessionManager::Start` registers `MetricsFileSink` with `MetricRegistry`. `SessionManager::Tick` drives snapshots at 100ms intervals. `SessionManager::Stop` fires a final snapshot and writes `metrics-final.json`.

**`DiaDebugServer`** depends on `DiaMetrics` directly — `ObservationBridge` (Feature #7) implements `IMetricSink` and forwards snapshots as `observation.metric` WebSocket topic without pulling all of `DiaObservation`.

`MetricsCollectorModule` (in `DiaApplicationFlow`) is rewritten to register FPS, frame-time, memory, and uptime as named `Gauge` primitives with `MetricRegistry`. Module ID and `DoUpdate` signature unchanged.

**Amendment to SD-O01:** The original decision ("one module for all four pillars") is amended. Logs, traces, and health remain in `DiaObservation` because they are intrinsically session-aware. Metrics are extracted to `DiaMetrics` because `Counter`/`Gauge`/`Histogram` are general-purpose primitives useful outside observation (profiling, gameplay stats, editor panels). The amended dependency graph is: `DiaMetrics → DiaCore`; `DiaObservation → DiaMetrics + DiaCore`.

---

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | `MetricRegistry::Instance().RegisterCounter("frames")` succeeds before `SessionManager::Start` is called | Unit test |
| AC2 | `Counter::Inc()` is lock-free on the hot path (per-thread shard, no mutex) | Code review + unit test: concurrent Inc from 4 threads, assert total == expected |
| AC3 | `Counter::Value()` reduces all per-thread shards and returns the total | Unit test |
| AC4 | `Gauge::Set(v)` and `Gauge::Value()` are atomic; last writer wins | Unit test: concurrent Set from 2 threads, assert Value() is one of the written values |
| AC5 | `Histogram::Observe(v)` places `v` in the correct bucket | Unit test: known bucket bounds, assert bucket counts after Observe calls |
| AC6 | Snapshot writes a `record_type: "metric_snapshot"` JSON-line record to `metric.jsonl` containing: `schema_version`, `session_id`, `ts_unix_nano`, `interval_ms`, and a `metrics` array with one entry per registered primitive | Unit test: register 2 counters + 1 gauge, trigger snapshot, parse record |
| AC7 | Counter entry in snapshot contains: `name`, `kind: "counter"`, `value` (cumulative total since session start) | Unit test |
| AC8 | Gauge entry in snapshot contains: `name`, `kind: "gauge"`, `value` (last set value) | Unit test |
| AC9 | Histogram entry in snapshot contains: `name`, `kind: "histogram"`, `count`, `sum`, `buckets` (array of `{le, count}`), `p50`, `p95`, `p99` | Unit test: known distribution, assert percentile values within tolerance |
| AC10 | Snapshot interval defaults to 100ms; `SessionManager::Tick` accumulates dt and fires when elapsed >= interval | Unit test: mock Tick calls summing to >100ms, assert snapshot fired |
| AC11 | Counter values in snapshot are cumulative since session start (not reset per snapshot) | Unit test: Inc 5 in snapshot 1, Inc 3 in snapshot 2; assert snapshot 2 value == 8 |
| AC12 | `metrics-final.json` is written on `SessionManager::Stop`; contains the same schema as a snapshot record but with `record_type: "metrics_final"` | Unit test: start + stop session, assert file exists + correct type field |
| AC13 | `metrics-final.json` is listed in `session.json` `files` array | Unit test |
| AC14 | `MetricsFileSink` is registered on `SessionManager::Start` when `sinks.metrics_file: true` (default); not registered when `false` | Unit test for both cases |
| AC15 | `MetricRegistry::FindCounter` / `FindGauge` / `FindHistogram` return `nullptr` for unregistered names | Unit test |
| AC16 | Registering the same name twice for the same kind returns the existing primitive (idempotent) | Unit test |
| AC17 | Registering the same name for a different kind (e.g. counter then gauge) asserts in Debug | Unit test |
| AC18 | `DiaMetrics` builds independently without `DiaObservation` in the dependency chain | `dia pipeline --target diametrics` green |
| AC19 | `MetricsCollectorModule` rewritten: FPS, frame-time, memory, uptime registered as named gauges with `MetricRegistry`; module ID and `DoUpdate` signature unchanged | Build + run: `dia pipeline --target cluichetest` green; existing consumers unaffected |
| AC20 | `dia pipeline --target cluichetest` green in Debug + Release | Build + run verification |

---

## Public API

### `MetricRegistry` singleton (lives in `Dia/DiaMetrics/`)

```cpp
namespace Dia::Metric {

class MetricRegistry {
public:
    static MetricRegistry& Instance();

    // Registration — safe to call before SessionManager::Start
    Counter*   RegisterCounter  (const Dia::Core::StringCRC& name);
    Gauge*     RegisterGauge    (const Dia::Core::StringCRC& name);
    Histogram* RegisterHistogram(const Dia::Core::StringCRC& name,
                                  const float* bucketBounds,
                                  unsigned int bucketCount);  // copies up to 16 bounds

    // Lookup — returns nullptr if not registered
    Counter*   FindCounter  (const Dia::Core::StringCRC& name) const;
    Gauge*     FindGauge    (const Dia::Core::StringCRC& name) const;
    Histogram* FindHistogram(const Dia::Core::StringCRC& name) const;

    // Snapshot — called by SessionManager::Tick and ::Stop, or any consumer
    void Snapshot(MetricSnapshot& out) const;

    // Sink registration — called by SessionManager::Start / Stop, DiaDebugServer, etc.
    void RegisterSink  (IMetricSink* sink);
    void UnregisterSink(IMetricSink* sink);
};

} // namespace Dia::Metric
```

### Primitives (lives in `Dia/DiaMetrics/`)

```cpp
namespace Dia::Metric {

class Counter {
public:
    void     Inc(uint64_t delta = 1);  // per-thread shard, lock-free
    uint64_t Value() const;            // reduce-on-read
};

class Gauge {
public:
    void   Set(double value);   // atomic store
    double Value() const;       // atomic load
};

class Histogram {
public:
    void Observe(double value);
    // Read via MetricRegistry::Snapshot()
};

} // namespace Dia::Metric
```

### `MetricSnapshot` / `IMetricSink` (lives in `Dia/DiaMetrics/`)

```cpp
namespace Dia::Metric {

struct BucketEntry { float le; uint64_t count; };

struct MetricEntry {
    Dia::Core::StringCRC name;
    enum class Kind : uint8_t { kCounter, kGauge, kHistogram } kind;
    uint64_t    counterValue;
    double      gaugeValue;
    uint64_t    histCount;
    double      histSum;
    BucketEntry buckets[17];     // 16 user bounds + implicit +Inf
    unsigned int bucketCount;
    double p50, p95, p99;
};

struct MetricSnapshot {
    uint64_t     timestampSteadyNs;  // consumers apply epoch offset for unix time
    uint32_t     intervalMs;
    MetricEntry  entries[128];
    unsigned int entryCount;
};

class IMetricSink {
public:
    virtual ~IMetricSink() = default;
    virtual void OnSnapshot(const MetricSnapshot& snapshot) = 0;
    virtual void OnFinal   (const MetricSnapshot& snapshot) = 0;
};

} // namespace Dia::Metric
```

### `MetricsFileSink` (lives in `Dia/DiaObservation/Metric/` — session-aware wrapper)

```cpp
namespace Dia::Observation {

class MetricsFileSink : public Dia::Metric::IMetricSink {
public:
    explicit MetricsFileSink(const char* metricJsonlPath,
                              const char* metricsFinalPath,
                              const char* sessionId,
                              int64_t     epochOffsetNs);
    ~MetricsFileSink() override;

    void OnSnapshot(const Dia::Metric::MetricSnapshot& snapshot) override;
    void OnFinal   (const Dia::Metric::MetricSnapshot& snapshot) override;
};

} // namespace Dia::Observation
```

---

## Wire Format

### `metric.jsonl` snapshot record

```json
{
  "schema_version": "1.0",
  "record_type":    "metric_snapshot",
  "session_id":     "20260517-143022-a3f2c1b9",
  "ts_unix_nano":   1747484400200000000,
  "interval_ms":    100,
  "metrics": [
    { "name": "frames",      "kind": "counter",   "value": 1800 },
    { "name": "dia.fps",     "kind": "gauge",     "value": 29.97 },
    { "name": "frame_time",  "kind": "histogram", "count": 1800, "sum": 60124.5,
      "buckets": [{"le": 16.0, "count": 1750}, {"le": 33.0, "count": 1799}, {"le": "+Inf", "count": 1800}],
      "p50": 15.2, "p95": 22.1, "p99": 31.4 }
  ]
}
```

### `metrics-final.json`

Same shape but `record_type: "metrics_final"`, written as a standalone pretty-printed JSON file.

---

## `MetricsCollectorModule` Rewrite (in `Dia/DiaApplicationFlow/Metrics/`)

Four gauges registered in `DoInit` via `Dia::Metric::MetricRegistry::Instance()`:

| Gauge name | Populated by |
|------------|-------------|
| `"dia.fps"` | `DoUpdate` — `1.0f / deltaTime` |
| `"dia.frame_time_ms"` | `DoUpdate` — `deltaTime * 1000.0f` |
| `"dia.memory_bytes"` | `DoUpdate` — `GetProcessMemoryInfo` |
| `"dia.uptime_s"` | `DoUpdate` — accumulated seconds since `DoStart` |

Module ID, `DoStart`, `DoStop`, and PU wiring unchanged. Internal hand-rolled storage deleted.

---

## Files Touched

| File | Change |
|------|--------|
| `Dia/DiaMetrics/MetricRegistry.h/.cpp` | New |
| `Dia/DiaMetrics/Counter.h/.cpp` | New |
| `Dia/DiaMetrics/Gauge.h/.cpp` | New |
| `Dia/DiaMetrics/Histogram.h/.cpp` | New |
| `Dia/DiaMetrics/MetricSnapshot.h` | New |
| `Dia/DiaMetrics/IMetricSink.h` | New |
| `Dia/DiaMetrics/DiaMetrics.vcxproj` | New — standalone module |
| `Dia/DiaMetrics/DiaMetrics.vcxproj.filters` | New |
| `Dia/DiaMetrics/dia.dia.metrics.architecture.module.md` | New — YAML module doc |
| `Dia/DiaMetrics/Testing/MetricFixture.h` | New test utility |
| `Dia/DiaObservation/Metric/MetricsFileSink.h/.cpp` | New — session-aware sink |
| `Dia/DiaObservation/Config/ObservationConfig.h` | Add `enableMetricsFileSink` |
| `Dia/DiaObservation/Config/ObservationConfigLoader.cpp` | Parse `sinks.metrics_file` |
| `Dia/DiaObservation/Session/SessionManager.h/.cpp` | `Start` registers `MetricsFileSink`; `Tick` drives snapshots; `Stop` fires final |
| `Dia/DiaObservation/DiaObservation.vcxproj` | Add `DiaMetrics` as project reference; add `MetricsFileSink` files |
| `Dia/DiaApplicationFlow/Metrics/MetricsCollectorModule.h/.cpp` | Rewrite internals to use `Dia::Metric::MetricRegistry` |
| `Dia/DiaApplicationFlow/DiaApplicationFlow.vcxproj` | Add `DiaMetrics` project reference |
| `Cluiche/Cluiche.sln` | Add `DiaMetrics` project |
| `docs/reference/registry/module-registry.md` | Register `DiaMetrics` |
| CluicheTest `.diagame` | Add `sinks.metrics_file: true` |

---

## Binding Decisions Compliance

| ID | Decision (summary) | Compliance |
|----|--------------------|------------|
| PD-001 | StringCRC for all identifiers | Metric names are `StringCRC`. `MetricEntry::name` is `StringCRC`. No raw string maps in public API. |
| PD-002 | ProcessingUnit/Phase/Module architecture | `MetricRegistry` singleton callable from any thread/module at any time. `MetricsCollectorModule` retains its `IModule` shape. |
| PD-003 | Component-based entities | Orthogonal. |
| PD-004 | No STL containers in public APIs | `MetricSnapshot` uses fixed arrays. Public API uses only `StringCRC`, `const float*`, `unsigned int`, primitive pointers. Internal `<atomic>`, `<mutex>`, `<thread>` permitted. |
| PD-005 | x64 only | `std::atomic<double>`, `thread_local` counter shards — x64-native. |
| PD-006 | VS project files source of truth | `DiaMetrics.vcxproj` and `DiaObservation.vcxproj` updated manually; registered in `Cluiche.sln`. |
| PD-007 | C++20 required | No new C++20 features; existing baseline applies. |
| PD-008 | Directory.Build.props owns build settings | No overrides added to either vcxproj. |
| PD-009 | Generated output under `Cluiche/out/<AppName>/` | `metric.jsonl` and `metrics-final.json` written to session directory by `MetricsFileSink`. |
| PD-010 | `.diagame` is project root | `sinks.metrics_file` slots into `config.observation.sinks`. |
| AD-001 | Module system with YAML frontmatter | `dia.dia.metrics.architecture.module.md` created for `DiaMetrics`. `DiaObservation` module doc updated to declare `Metric/` subsystem and `DiaMetrics` dependency. |
| AD-002 | No STL containers in public APIs | Reinforces PD-004. |
| AD-003 | Namespace `Dia::<Module>::` | `DiaMetrics` code in `Dia::Metric::`. `MetricsFileSink` in `Dia::Observation::`. |
| SD-O01 | One module for all four pillars — **AMENDED** | Logs, traces, health remain in `DiaObservation` (intrinsically session-aware). Metrics extracted to standalone `DiaMetrics` (general-purpose, usable without observation). Amended decision: `DiaObservation → DiaMetrics + DiaCore`. |
| SD-O05 | `schema_version: "1.0"` on every record | `MetricsFileSink::OnSnapshot` emits `"schema_version":"1.0"` on every line. |
| SD-O06 | OTel wire-format, no SDK | Field names align with OTel metrics conventions. No `opentelemetry-cpp` dependency. |
| SD-O14 | Monotonic timestamp at producer side | `MetricSnapshot::timestampSteadyNs` captured in `SessionManager::Tick`; `MetricsFileSink` applies epoch offset. |
| SD-O16 | Session ID on every record | `MetricsFileSink` writes `session_id` from value received at construction. `DiaMetrics` primitives carry no session ID — correct, as they are session-agnostic. |
| SD-O17 | Counter uses per-thread shards, reduced on snapshot | `Counter::Inc` increments `thread_local` shard; `Value()` / `Snapshot()` reduce. |
| SD-O20 | Test utilities in module Testing/ | `MetricFixture.h` placed in `Dia/DiaMetrics/Testing/`. |
| SD-O21 | DiaCore is only required dependency for DiaObservation | `DiaObservation` now also depends on `DiaMetrics`. `DiaMetrics` depends only on `DiaCore`. The spirit — observation primitives callable from anywhere without heavy dependencies — is preserved: `DiaMetrics` is the lightweight primitive layer. |

---

## Open Questions

| # | Question | Resolution |
|---|----------|------------|
| OQ1 | `MetricEntry[128]` fixed cap — is 128 enough? | Engine today has ~4 gauges. 128 ample for v1; internal struct change if needed. |
| OQ2 | `std::atomic<double>` for `Gauge` — available in MSVC C++20? | Yes — MSVC 19.29+ (VS 2019 16.9+), lock-free on x64. Confirmed by PD-007. |
| OQ3 | `MetricsCollectorModule` touches `DiaApplicationFlow` — conflict with "DiaApplicationFlow is blocked" backlog item? | No — rewrite is internal to `MetricsCollectorModule` only; does not touch v1/v2 manifest, stage, or stream types. |

---

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Threading | Per-thread counter shards — dynamic count? | Dynamic: each thread that calls `Counter::Inc` registers a `thread_local` shard lazily. `Value()` / `Snapshot()` iterate via a mutex-protected list. Max threads ~4–8 in this engine. |
| 2 | Threading | Counter shard lazy registration — same mutex pattern as Logger? | Yes. `thread_local` RAII guard unregisters on thread exit. Mutex held only for list modification, not for `Inc`. |
| 3 | Snapshot | Is snapshot atomic across all metrics? | No — each primitive read independently. Nanosecond-level races are irrelevant for engine telemetry. No global lock needed. |
| 4 | Histogram | p50/p95/p99 computation? | Linear interpolation within the target-rank bucket, same as Prometheus client. Formula in implementation notes. |
| 5 | Histogram | Value above all bucket bounds? | Falls into implicit `+Inf` bucket; counted in `histCount` + `histSum`; raises high percentiles to `+Inf` if prevalent. Documented behaviour. |
| 6 | Lifecycle | Metrics registered before session starts? | Accumulate normally; visible in first post-session snapshot. No data lost. |
| 7 | Lifecycle | Session stops before first snapshot fires? | `Stop` always fires a final snapshot regardless of elapsed time. |
| 8 | Config | Snapshot interval configurable in v1? | No — compile-time constant `kDefaultSnapshotIntervalMs = 100`. Format does not foreclose future `observation.metrics.snapshot_interval_ms`. |
| 9 | MetricsCollectorModule | `GetProcessMemoryInfo` already present? | Verify at implementation time. If not, add via `<psapi.h>` `WorkingSetSize`. Windows-only, consistent with PD-005. |
| 10 | Wire | Counter cumulative only — no per-snapshot delta? | Cumulative only in v1. Consumer subtracts consecutive values for delta. OTel counter convention is cumulative. |
| 11 | Architecture | Does extracting `DiaMetrics` mean `DiaDebugServer` no longer needs to depend on `DiaObservation` for metrics? | Yes — `DiaDebugServer::ObservationBridge` (Feature #7) implements `Dia::Metric::IMetricSink` and depends on `DiaMetrics` directly for metric forwarding. It still depends on `DiaObservation` for log/trace/health forwarding. Net result: smaller surface than depending on all of `DiaObservation` for metrics alone. |
