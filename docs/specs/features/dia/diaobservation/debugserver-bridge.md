# Feature Spec: DebugServer Observation Bridge

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diaobservation.md | **debugserver-bridge** |

**Status:** `Approved` — 2026-05-17

**Research:** @docs/research/observ_telemetry/summary.md

**Depends on:** Feature #2 (foundation — `ObservationFileSink`/`ISink`), Feature #4 (trace-spans — `ITraceSink`), Feature #5 (metrics-registry — `Dia::Metric::IMetricSink` from `DiaMetrics`), Feature #6 (health-reporting — `IHealthTransitionSink`). Must be last — depends on all four pillars. Implementation is serial.

---

## Problem Statement

After Features #1–#6, all four observation pillars write to files in the session directory, but nothing forwards live data to the editor. `DiaDebugServer::BroadcastCoreMetrics` hand-serialises four hand-rolled metrics over WebSocket — it has no awareness of logs, traces, or health. This feature delivers `ObservationBridge`, which subscribes to all four pillar sink interfaces and forwards records as WebSocket subscription topics (`observation.log`, `observation.trace`, `observation.metric`, `observation.health`), replacing `BroadcastCoreMetrics` entirely.

---

## Solution Overview

`ObservationBridge` is a class in `Dia/DiaDebugServer/` (not in `DiaObservation`). It implements four sink interfaces:

| Sink interface | Source module | Pillar | WebSocket topic |
|----------------|--------------|--------|-----------------|
| `Dia::Observation::Log::ISink` | `DiaObservation` | Logs | `observation.log` |
| `Dia::Observation::Trace::ITraceSink` | `DiaObservation` | Traces | `observation.trace` |
| `Dia::Metric::IMetricSink` | `DiaMetrics` | Metrics | `observation.metric` |
| `Dia::Observation::Health::IHealthTransitionSink` | `DiaObservation` | Health | `observation.health` |

`ObservationBridge` is constructed and registered by `DiaDebugServer` at startup (same lifecycle as today's `BroadcastCoreMetrics` call site). It registers with `Logger`, `Tracer`, `MetricRegistry`, and `HealthRegistry` in `Start()`, unregisters in `Stop()`.

**`BroadcastCoreMetrics` is deleted.** The bridge's `IMetricSink::OnSnapshot` replaces it — the full `MetricSnapshot` is serialised as the `observation.metric` topic. No existing editor consumers of the old topic are live (DiaApplicationFlowEditor is blocked and unimplemented).

**Amendments to Feature #4 and Feature #6:**
- Feature #4 gains `ITraceSink` (additive interface in `DiaObservation/Trace/`)
- Feature #6 gains `IHealthTransitionSink` (additive interface in `DiaObservation/Health/`)

Both amendments are backward-compatible (additive interfaces, default implementations not required).

---

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | `ObservationBridge::Start()` registers with `Logger`, `Tracer`, `MetricRegistry`, and `HealthRegistry` | Unit test: assert all four registrations via mock registries |
| AC2 | `ObservationBridge::Stop()` unregisters from all four registries | Unit test |
| AC3 | A `DIA_LOG_INFO` call while the bridge is active results in an `observation.log` WebSocket broadcast containing the log record fields | Integration test: log entry → assert WebSocket message sent with correct topic + fields |
| AC4 | A closed `DIA_TRACE_ZONE` while the bridge is active results in an `observation.trace` WebSocket broadcast | Integration test |
| AC5 | A metric snapshot while the bridge is active results in an `observation.metric` WebSocket broadcast containing all registered metrics | Integration test |
| AC6 | A health status transition while the bridge is active results in an `observation.health` WebSocket broadcast | Integration test |
| AC7 | WebSocket payloads are JSON; field names match the corresponding `log.jsonl` / `trace.jsonl` / `metric.jsonl` / health transition record schemas | Assert field names in broadcast payload match wire format specs from Features #2, #4, #5, #6 |
| AC8 | `BroadcastCoreMetrics` is deleted from `DiaDebugServer`; no callers remain | `grep -r BroadcastCoreMetrics` returns no results |
| AC9 | Bridge forwards log records at or above the Logger's configured level threshold only — filtered records are not forwarded | Unit test: set level Warning, log Info, assert no broadcast |
| AC10 | `ObservationBridge` compiles cleanly against `DiaDebugServer`, `DiaObservation`, and `DiaMetrics` | `dia pipeline --target cluichetest` green in Debug + Release |
| AC11 | If no WebSocket clients are connected, `OnLog`/`OnSpan`/`OnSnapshot`/`OnTransition` are no-ops (no crash, no queuing) | Unit test: bridge active, zero clients, call all four sinks |
| AC12 | `ITraceSink` interface exists in `Dia/DiaObservation/Trace/`; `Tracer` dispatches to registered `ITraceSink` instances on each closed span | Unit test: register mock sink, close span, assert `OnSpan` called |
| AC13 | `IHealthTransitionSink` interface exists in `Dia/DiaObservation/Health/`; `HealthRegistry` dispatches to registered `IHealthTransitionSink` instances when `PollTransitions` detects a change | Unit test: register mock sink, flip health, tick, assert `OnTransition` called |

---

## Public API

### New sink interfaces (amendments to Features #4 and #6)

```cpp
// Dia/DiaObservation/Trace/ITraceSink.h  (amendment to Feature #4)
namespace Dia::Observation::Trace {
class ITraceSink {
public:
    virtual ~ITraceSink() = default;
    virtual void OnSpan(const SpanRecord& span) = 0;
};
} // namespace Dia::Observation::Trace

// Tracer gains:
void RegisterTraceSink  (ITraceSink* sink);
void UnregisterTraceSink(ITraceSink* sink);
```

```cpp
// Dia/DiaObservation/Health/IHealthTransitionSink.h  (amendment to Feature #6)
namespace Dia::Observation::Health {
class IHealthTransitionSink {
public:
    virtual ~IHealthTransitionSink() = default;
    virtual void OnTransition(const HealthRegistry::Transition& transition) = 0;
};
} // namespace Dia::Observation::Health

// HealthRegistry gains:
void RegisterTransitionSink  (IHealthTransitionSink* sink);
void UnregisterTransitionSink(IHealthTransitionSink* sink);
// Dispatches to registered sinks inside PollTransitions when a change is detected.
```

### `ObservationBridge` (lives in `Dia/DiaDebugServer/`)

```cpp
namespace Dia::DebugServer {

class ObservationBridge
    : public Dia::Observation::Log::ISink
    , public Dia::Observation::Trace::ITraceSink
    , public Dia::Metric::IMetricSink
    , public Dia::Observation::Health::IHealthTransitionSink
{
public:
    explicit ObservationBridge(IWebSocketServer* server);
    ~ObservationBridge() override;

    void Start();  // registers with all four registries
    void Stop();   // unregisters from all four registries

    // ISink
    void OnLog(const Dia::Observation::Log::LogEntry& entry) override;

    // ITraceSink
    void OnSpan(const Dia::Observation::Trace::SpanRecord& span) override;

    // IMetricSink
    void OnSnapshot(const Dia::Metric::MetricSnapshot& snapshot) override;
    void OnFinal   (const Dia::Metric::MetricSnapshot& snapshot) override;  // no-op for bridge

    // IHealthTransitionSink
    void OnTransition(const Dia::Observation::Health::HealthRegistry::Transition& t) override;

private:
    IWebSocketServer* mServer = nullptr;
};

} // namespace Dia::DebugServer
```

---

## WebSocket Message Shape

All payloads are JSON strings broadcast to the relevant topic. Field names match the corresponding file sink wire formats exactly.

### `observation.log`
```json
{ "schema_version":"1.0", "ts_unix_nano":..., "session_id":"...", "level":"info",
  "severity_number":9, "channel":"asset", "scenario_step":"", "thread_id":12345, "msg":"..." }
```

### `observation.trace`
```json
{ "schema_version":"1.0", "record_type":"span", "session_id":"...",
  "trace_id":"a3f2c1b9e4d50f12", "span_id":"7c3a1b2d9e4f0561", "parent_span_id":"0000000000000000",
  "name":"RenderFrame", "start_unix_nano":..., "end_unix_nano":...,
  "thread_id":12345, "scenario_step":"" }
```

### `observation.metric`
```json
{ "schema_version":"1.0", "record_type":"metric_snapshot", "session_id":"...",
  "ts_unix_nano":..., "interval_ms":100,
  "metrics": [ { "name":"dia.fps", "kind":"gauge", "value":29.97 } ] }
```

### `observation.health`
```json
{ "schema_version":"1.0", "record_type":"health", "session_id":"...",
  "ts_unix_nano":..., "reporter":"RenderModule",
  "old_status":"ok", "new_status":"degraded", "reason":"gpu_timeout" }
```

---

## Files Touched

| File | Change |
|------|--------|
| `Dia/DiaObservation/Trace/ITraceSink.h` | New (amendment to Feature #4) |
| `Dia/DiaObservation/Trace/Tracer.h/.cpp` | Add `RegisterTraceSink`/`UnregisterTraceSink`; dispatch in drain |
| `Dia/DiaObservation/Health/IHealthTransitionSink.h` | New (amendment to Feature #6) |
| `Dia/DiaObservation/Health/HealthRegistry.h/.cpp` | Add `RegisterTransitionSink`/`UnregisterTransitionSink`; dispatch in `PollTransitions` |
| `Dia/DiaObservation/DiaObservation.vcxproj` | Add new interface files |
| `Dia/DiaObservation/DiaObservation.vcxproj.filters` | Add new interface files |
| `Dia/DiaDebugServer/ObservationBridge.h/.cpp` | New |
| `Dia/DiaDebugServer/DebugServer.h/.cpp` | Add `ObservationBridge` member; call `Start`/`Stop`; delete `BroadcastCoreMetrics` |
| `Dia/DiaDebugServer/DiaDebugServer.vcxproj` | Add `DiaMetrics` + `DiaObservation` project references (if not already present); add `ObservationBridge` files |

---

## Binding Decisions Compliance

| ID | Decision (summary) | Compliance |
|----|--------------------|------------|
| PD-001 | StringCRC for all identifiers | `ObservationBridge` uses WebSocket topic strings as raw `const char*` literals (topic names are wire-format constants, not engine entity identifiers — not subject to PD-001). All pillar record field identifiers come from upstream sinks unchanged. |
| PD-002 | ProcessingUnit/Phase/Module architecture | `ObservationBridge` is owned by `DiaDebugServer`, which is already a module. No new module needed. |
| PD-003 | Component-based entities | Orthogonal. |
| PD-004 | No STL containers in public APIs | `ObservationBridge` public API uses only pillar sink types and `IWebSocketServer*`. No STL containers. |
| PD-005 | x64 only | No platform-specific concerns. |
| PD-006 | VS project files source of truth | `DiaDebugServer.vcxproj` and `DiaObservation.vcxproj` updated manually. |
| PD-007 | C++20 required | No new C++20 features. |
| PD-008 | Directory.Build.props owns build settings | No overrides added. |
| PD-009 | Generated output under `Cluiche/out/<AppName>/` | Bridge produces no file output — WebSocket only. |
| PD-010 | `.diagame` is project root | No config additions in this feature. |
| AD-001 | Module system with YAML frontmatter | `ObservationBridge` lives in `DiaDebugServer` — no new module doc needed. `DiaDebugServer`'s module doc should be updated to note `DiaObservation` + `DiaMetrics` dependencies. |
| AD-002 | No STL containers in public APIs | Reinforces PD-004. |
| AD-003 | Namespace `Dia::<Module>::` | `ObservationBridge` in `Dia::DebugServer::`. |
| SD-O01 | Logs/traces/health in `DiaObservation`; metrics in `DiaMetrics` | Bridge depends on both — correct. No new pillars added here. |
| SD-O05 | `schema_version: "1.0"` on every record | Bridge forwards records exactly as received from sinks — `schema_version` is already present upstream. |
| SD-O06 | OTel wire-format, no SDK | Bridge forwards fields verbatim — no field remapping. |
| SD-O16 | Session ID on every record | Records arrive from sinks with `session_id` already stamped. Bridge forwards unchanged. |
| SD-O21 | DiaCore is only required dep for DiaObservation | `ObservationBridge` lives in `DiaDebugServer`, not `DiaObservation`. `DiaObservation` does not depend on `DiaDebugServer`. Dependency direction is correct: `DiaDebugServer → DiaObservation + DiaMetrics`. |

---

## Open Questions

| # | Question | Resolution |
|---|----------|------------|
| OQ1 | `OnFinal` for the metric bridge — should the final snapshot be broadcast as `observation.metric`? | Yes, same topic, same payload. A consumer that cares about final-vs-periodic can inspect `record_type: "metrics_final"` vs `"metric_snapshot"`. Forwarding it is low-cost and gives the editor a complete picture at session end. |
| OQ2 | Does `DiaDebugServer` already reference `DiaObservation` or `DiaMetrics` as project dependencies? | Verify at implementation time. Feature #5's `MetricsCollectorModule` rewrite added `DiaMetrics` as a `DiaApplicationFlow` dependency; `DiaDebugServer` may not yet reference either. Add both to `DiaDebugServer.vcxproj` in this feature. |

---

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Threading | `OnLog` is called from the Logger drain thread. `OnSpan` from the Tracer drain thread. `OnSnapshot` from the main PU (`SessionManager::Tick`). `OnTransition` from the main PU (`SessionManager::Tick` → `PollTransitions`). Is `ObservationBridge` thread-safe? | `OnLog` and `OnSpan` are called from background drain threads; `OnSnapshot` and `OnTransition` from the main PU. Each `On*` method calls `IWebSocketServer::Broadcast(topic, json)`. If `Broadcast` is already thread-safe (check `DiaWebSocket` impl), no extra locking needed in the bridge. If not, a `std::mutex` in `ObservationBridge` guards all four `On*` methods. Verify at implementation time. |
| 2 | Threading | `Start`/`Stop` vs drain thread — what if a drain thread calls `OnLog` after `Stop` unregisters the bridge? | Same ordering as sink lifecycle in the Logger: `Stop` unregisters the bridge, then the drain thread's next sweep finds no sink registered and skips. Unregistration is atomic from the drain thread's perspective because sink list access is mutex-protected (established in Feature #1). No use-after-free. |
| 3 | Performance | `OnLog` is called from the drain thread on every log entry. Serialising to JSON + calling `Broadcast` on every log entry — is this too hot? | `Broadcast` on `DiaWebSocket` queues the message for async send; it is not a blocking write. JSON serialisation is ~1–5 µs per record on the drain thread (same budget as `ObservationFileSink::OnLog`). Acceptable. If a future profiling pass shows drain thread contention, the bridge can add a per-topic level threshold to drop trace/debug entries before serialisation. |
| 4 | Payload | WebSocket payload is a JSON string. Is it constructed via `DiaCore/Json` (`Json::Value` → `Json::FastWriter`) or a manual `snprintf` approach? | `snprintf` / manual construction — same approach as `ObservationFileSink`. `Json::Value` involves heap allocation (violates spirit of PD-004 on a hot path). A fixed `char[2048]` stack buffer per `On*` method is sufficient for all record types. |
| 5 | BroadcastCoreMetrics | Are there any callers of `BroadcastCoreMetrics` outside `DebugServer.cpp` itself? | Grep at implementation time. Likely only called from `DebugServer::Update` or similar. If external callers exist, they must be updated before the method is deleted. |
| 6 | Topics | Are `observation.log` / `observation.trace` / `observation.metric` / `observation.health` new topics or do any of them overlap with existing `DiaDebugServer` topics? | Verify against existing topic list in `DebugServer.h`. These are new topics — no existing `DiaDebugServer` topic uses the `observation.*` prefix. Safe to add. |
| 7 | Feature ordering | Feature #7 is last and depends on all four pillar features. Can it be implemented before all four are done? | Bridge can be built incrementally — implement `OnLog` first (Feature #2 available), add `OnSpan` when Feature #4 is implemented, etc. The spec covers the full bridge; implementation can be staged. |
| 8 | Health | `IHealthTransitionSink::OnTransition` is called inside `HealthRegistry::PollTransitions`, which runs on the main PU thread. Is this safe given that `OnTransition` calls `Broadcast` (potentially locking)? | `Broadcast` is a queue push — should not block. If `IWebSocketServer::Broadcast` holds a mutex, the main PU thread holds it briefly during `PollTransitions`. Acceptable given health transitions are rare (not per-frame). |

---

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | `ITraceSink` in `DiaObservation/Trace/`; `Tracer` gains `RegisterTraceSink`/`UnregisterTraceSink`; dispatch in drain | AC12 | Planned | haiku | Amendment to Feature #4 files |
| 2 | `IHealthTransitionSink` in `DiaObservation/Health/`; `HealthRegistry` gains `RegisterTransitionSink`/`UnregisterTransitionSink`; dispatch in `PollTransitions` | AC13 | Planned | haiku | Amendment to Feature #6 files |
| 3 | `ObservationBridge` — implements all four sink interfaces; `Start`/`Stop` register/unregister | AC1, AC2, AC11 | Planned | sonnet | Verify `Broadcast` thread-safety per AI-Q1 |
| 4 | `OnLog` — serialise `LogEntry` to `observation.log` JSON payload | AC3, AC7, AC9 | Planned | sonnet | snprintf per AI-Q4 |
| 5 | `OnSpan` — serialise `SpanRecord` to `observation.trace` JSON payload | AC4, AC7 | Planned | sonnet | |
| 6 | `OnSnapshot` — serialise `MetricSnapshot` to `observation.metric` JSON payload | AC5, AC7 | Planned | sonnet | Forward `metrics_final` too per OQ1 |
| 7 | `OnTransition` — serialise health transition to `observation.health` JSON payload | AC6, AC7 | Planned | sonnet | |
| 8 | Delete `BroadcastCoreMetrics` from `DebugServer.h/.cpp`; grep for callers | AC8 | Planned | haiku | |
| 9 | Wire `ObservationBridge` into `DebugServer` — construct, `Start` on server start, `Stop` on server stop | AC1, AC2, AC10 | Planned | haiku | |
| 10 | Update `DiaDebugServer.vcxproj` — add `DiaMetrics` + `DiaObservation` references | AC10 | Planned | haiku | |
| 11 | Update `DiaObservation.vcxproj` + filters — add `ITraceSink.h`, `IHealthTransitionSink.h` | AC10 | Planned | haiku | |
| 12 | GoogleTests — all AC1–AC13 | All ACs | Planned | sonnet | Mock `IWebSocketServer` for unit tests |

---

## Status

`Done` — 2026-05-20.
