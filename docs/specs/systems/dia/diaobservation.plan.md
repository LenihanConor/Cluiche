# Implementation Plan: DiaObservation System

**Spec:** [diaobservation.md](diaobservation.md)
**Created:** 2026-05-18
**Updated:** 2026-05-19

---

## Session Notes

### Spec Decisions Summary

DiaObservation is a 5-pillar observability system (logs, traces, metrics, health, profiling) living in a single `DiaObservation` module depending only on DiaCore (SD-O01 amended 2026-05-19 — metrics folded in, no standalone DiaMetrics sibling module). Session directory at `Cluiche/out/<App>/sessions/<id>/` (SD-O07). Wire format is OTel-aligned without SDK dependency (SD-O06). `session.json` + all `.jsonl` files are public contracts (SD-O08). Logger drain runs on a dedicated thread (SD-O03). Traces and profiling default OFF; logs and metrics default ON (SD-O27). Categories use `uint32_t` bitmasks for traces/profiling (SD-O28/29). Profiling is frame-structured, distinct from traces (SD-O24/25). `DIA_PROFILE_SCOPE_METRIC` bridges profiling and metrics at hot sites (SD-O30). Test utilities in `Dia/DiaObservation/Testing/` (SD-O20). DiaCore cannot include observation headers (SD-O22).

### Key Amendments

- **2026-05-19:** Metrics folded into DiaObservation (SD-O01 reversed from standalone DiaMetrics). The existing `Dia/DiaMetrics/` code (tasks 1–10, 14–16 of the metrics-registry plan) must be physically moved into `Dia/DiaObservation/Metric/` as part of implementation. The DiaMetrics vcxproj will be removed.
- **2026-05-19:** Profiling added as 5th pillar. Features #8 (infrastructure) and #9 (domain instrumentation) added. Old features 8–11 renumbered to 10–13.

---

## Task Table

This is a **system plan** — each row is a feature spec. Individual features have their own `*.plan.md` files with detailed task breakdowns.

| # | Feature | Spec | Status | Notes |
|---|---------|------|--------|-------|
| 1 | DiaObservation Skeleton + DiaLogger Fold + Async Drain | [skeleton-and-logger-fold.md](../../features/dia/diaobservation/skeleton-and-logger-fold.md) | Done | Landed 2026-05-18. DiaLogger deleted, all callers migrated, async drain thread running. [Plan](../../features/dia/diaobservation/skeleton-and-logger-fold.plan.md) |
| 2 | DiaObservation Foundation | [foundation.md](../../features/dia/diaobservation/foundation.md) | Done | SessionManager, session directory, ObservationFileSink, timestamps, retention ring, exit-reason capture, crash auto-dump, session.json writer. Landed 2026-05-19. |
| 3 | DiaObservation Config | [config.md](../../features/dia/diaobservation/config.md) | Done | `.diagame` observation block parser, CLI overrides, per-channel level threshold. Landed 2026-05-19. |
| 4 | DiaTrace Spans | [trace-spans.md](../../features/dia/diaobservation/trace-spans.md) | Done | Tracer singleton, ScopedZone RAII, TraceFileSink → trace.jsonl, DIA_TRACE_ZONE macros, ITraceSink interface. Landed 2026-05-19. |
| 5 | Metrics Registry | [metrics-registry.md](../../features/dia/diaobservation/metrics-registry.md) | Done | Core primitives + MetricsFileSink + SessionManager 100ms snapshot wiring + metrics-final.json. Physical fold into `DiaObservation/Metric/` done. DiaMetrics.vcxproj removed. Deferred: editor metrics (AC21–AC27). Landed 2026-05-19. [Plan](../../features/dia/diaobservation/metrics-registry.plan.md) |
| 6 | DiaHealth Reporting | [health-reporting.md](../../features/dia/diaobservation/health-reporting.md) | Done | IHealthReporter, HealthReporterBase, HealthRegistry, DIA_OBSERVATION_ASSERT/FAIL macros, health.json, 500ms health polling in Tick, session.json modules populated. IHealthTransitionSink interface. Landed 2026-05-19. |
| 7 | DebugServer Observation Bridge | [debugserver-bridge.md](../../features/dia/diaobservation/debugserver-bridge.md) | Done | ObservationBridge implements ISink + ITraceSink + IMetricSink + IHealthTransitionSink; BroadcastCoreMetrics deleted; WebSocket topics observation.log/.trace/.metric/.health. Landed 2026-05-19. |
| 8 | DiaProfiling — Frame Instrumentation | [profiling-frame-instrumentation.md](../../features/dia/diaobservation/profiling-frame-instrumentation.md) | Done | Profiler singleton, ScopedZone, ProfileFileSink, ProfilerModule, profile.jsonl, DIA_PROFILE_SCOPE macros, 11 unit tests passing. Landed 2026-05-19. |
| 9 | DiaProfiling — Domain Instrumentation | [profiling-domain-instrumentation.md](../../features/dia/diaobservation/profiling-domain-instrumentation.md) | Done | ProcessingUnit::Update, Module::FrameTick kActive, EventStreamStore::Send/Consume, AssetRuntime::RequestStageLoad/DispatchLoad. 4632 tests passing. Landed 2026-05-19. |
| 10 | Domain-Level Log Instrumentation | [domain-log-instrumentation.md](../../features/dia/diaobservation/domain-log-instrumentation.md) | Done | Module lifecycle transitions, configure/connect_streams, stream reader/tap events, overflow, asset type+record logs. Landed 2026-05-19. |
| 11 | Domain-Level Trace Instrumentation | [domain-trace-instrumentation.md](../../features/dia/diaobservation/domain-trace-instrumentation.md) | Done | DIA_TRACE_ZONE on pu.update, module.update, stream.send/consume, asset.catalog.load/asset.load. Landed 2026-05-19. |
| 12 | Domain-Level Health Reporting | [domain-health-reporting.md](../../features/dia/diaobservation/domain-health-reporting.md) | Done | Tier 1: LifecycleReporter in base Module + AssetHealthReporter in AssetServiceModule. Tier 2/3 deferred. Landed 2026-05-19. |
| 13 | Domain-Level Metric Registration | [domain-metric-registration.md](../../features/dia/diaobservation/domain-metric-registration.md) | Done | dia.assets.* in AssetServiceModule, dia.debugserver.* in DebugServerHostModule. ServerStats deletion deferred. Landed 2026-05-19. |

---

## Dependency Graph

```
Feature #1 (Skeleton + Fold) ─── DONE
    │
    ▼
Feature #2 (Foundation) ────────────────────────────────────────────────┐
    │                                                                    │
    ▼                                                                    │
Feature #3 (Config) ─────────────────────────┐                          │
    │                                         │                          │
    ├──────────────┬──────────────┐           │                          │
    ▼              ▼              ▼           ▼                          │
Feature #4     Feature #5     Feature #6   Feature #8                   │
(Traces)       (Metrics†)     (Health)     (Profiling Infra)            │
    │              │              │           │                          │
    │              │              │           ▼                          │
    │              │              │        Feature #9                    │
    │              │              │        (Prof. Domain)                │
    ▼              ▼              ▼           │                          │
Feature #11    Feature #13    Feature #12    │                          │
(Trace Dom.)   (Metric Dom.)  (Health Dom.)  │                          │
    │              │              │           │                          │
    └──────────────┴──────────────┴───────────┘                         │
                        │                                                │
                        ▼                                                │
                  Feature #7 (DebugServer Bridge) ◄─────────────────────┘
                        │
                        ▼
                  Feature #10 (Log Domain Instrumentation)
```

† Feature #5 partial: DiaMetrics primitives folded into `Dia/DiaObservation/Metric/` (2026-05-19). Remaining: MetricsFileSink, SessionManager wiring, editor metrics.

### Parallel Opportunities

- **After #3:** Features #4, #5 (remainder), #6, #8 can partially overlap — they touch separate subdirectories and have no shared headers.
- **After infrastructure features (#4–#6, #8):** Domain instrumentation features (#9–#13) are highly parallelizable — each touches different subsystem code.
- **Feature #10 (log instrumentation)** has no hard dependency on #3/#4/#5/#6 — can start any time after #1. Placed last because it's lowest value-add (most subsystems already have some logging). Can be reordered if needed.

### Blocking Issues

| Issue | Blocks | Resolution |
|-------|--------|------------|
| ~~DiaMetrics standalone → fold into DiaObservation~~ | ~~Feature #5 completion~~ | ~~Resolved 2026-05-19. Files moved to `Dia/DiaObservation/Metric/`, namespace `Dia::Observation::Metric::`, DiaMetrics.vcxproj removed from solution, consumers updated.~~ |
| ~~Feature #2 spec is Draft~~ | ~~Feature #2 implementation~~ | ~~Resolved — Approved 2026-05-17.~~ |
| Features #8/#9 specs not written | Profiling implementation | Need `/spec-feature` for each before implementation. |
| Features #10–#13 specs not written | Domain instrumentation | Need `/spec-feature` for each. Lower priority — these are polish. |

---

## Recommended Execution Order

### Phase 1 — Foundation (serial)
1. **Implement** Feature #2 (Foundation) — spec Approved, ready to go
2. **Fold DiaMetrics into DiaObservation/Metric/** (part of Feature #5 completion)

### Phase 2 — Config
3. **Implement** Feature #3 (Config) — spec Approved

### Phase 3 — Core Pillars (parallelizable after #3)
4. **Implement** Feature #4 (Traces) — spec Approved; can overlap with #5 remainder and #6
5. **Complete** Feature #5 (Metrics) — MetricsFileSink, SessionManager wiring, editor metrics
6. **Implement** Feature #6 (Health) — spec Approved
7. **Write spec + Approve + Implement** Feature #8 (Profiling Infrastructure)

### Phase 4 — Domain Instrumentation (parallelizable)
8. **Write spec + Approve + Implement** Feature #9 (Profiling Domain)
9. **Write spec + Approve + Implement** Feature #10 (Log Domain)
10. **Write spec + Approve + Implement** Feature #11 (Trace Domain)
11. **Write spec + Approve + Implement** Feature #12 (Health Domain)
12. **Write spec + Approve + Implement** Feature #13 (Metric Domain)

### Phase 5 — Bridge (last)
13. **Implement** Feature #7 (DebugServer Bridge) — spec Approved; needs all pillars functional

---

## Notes

- The metrics-registry.plan.md references a now-superseded `docs/specs/systems/dia/diametrics.md` — this pointer is stale since SD-O01 amendment folded DiaMetrics into DiaObservation. The plan itself remains valid; deferred tasks (11–13, 17–21) will execute in the context of DiaObservation, not a standalone DiaMetrics module.
- Feature #1 is fully Done and verified — the DiaObservation module exists on disk with the Log/ subsystem functional and all callers migrated.
- The `MetricsCollectorModule` rewrite (plan task 14) is Done — it now registers gauges via `Dia::Metric::MetricRegistry`. The namespace will change to `Dia::Observation::Metric::` during the fold.
