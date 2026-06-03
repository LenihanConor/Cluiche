---
schema: dia.module.v1
id: dia.observation
layer: foundation/services
display_name: DiaObservation
status: active
maturity: dev
path: Dia/DiaObservation/
vcxproj: Dia/DiaObservation/DiaObservation.vcxproj

summary: >
  Four-pillar observability module for the Dia engine: logging (Log/),
  distributed tracing (Trace/), metrics (Metric/), and health reporting
  (Health/). Folds DiaLogger into Log/ as the first landed subsystem.
  Session management (Session/) drives output files and snapshot timers.

public_api:
  headers:
    - DiaObservation/Log/Logger.h
    - DiaObservation/Log/LogEntry.h
    - DiaObservation/Log/LogLevel.h
    - DiaObservation/Log/ISink.h
    - DiaObservation/Log/ThreadLogBuffer.h
    - DiaObservation/Log/StdOutSink.h
    - DiaObservation/Log/DebugOutputSink.h
    - DiaObservation/Log/AssertSinkBridge.h
    - DiaObservation/Log/DiaLog.h
    - DiaObservation/Testing/MockSink.h
  namespaces:
    - Dia::Observation::Log
  entry_points:
    - Dia::Observation::Log::Logger::Instance()

dependencies:
  required:
    - dia.core

subsystems:
  log:
    status: active
    path: Dia/DiaObservation/Log/
    description: >
      Folded from DiaLogger. Async drain thread (1ms tick). Ring-buffer
      per-thread producer. StdOutSink + DebugOutputSink built-in.
  session:
    status: placeholder
    path: Dia/DiaObservation/Session/
    description: SessionManager, session directory, output file lifecycle. Lands in Feature #2.
  trace:
    status: placeholder
    path: Dia/DiaObservation/Trace/
    description: DIA_TRACE_ZONE macros, Tracer singleton, trace.jsonl. Lands in Feature #4.
  metric:
    status: placeholder
    path: Dia/DiaObservation/Metric/
    description: MetricsFileSink, snapshot timer wiring. Lands in Feature #5.
  health:
    status: placeholder
    path: Dia/DiaObservation/Health/
    description: HealthRegistry, IHealthReporter, health.json. Lands in Feature #6.
  config:
    status: placeholder
    path: Dia/DiaObservation/Config/
    description: ObservationConfigLoader, .diagame block. Lands in Feature #3.

responsibilities:
  owns:
    - Dia::Observation::Log::Logger singleton — async drain, ring-buffer per-thread producers
    - Log/ subsystem — moved verbatim from DiaLogger; namespace hard-switched to Dia::Observation::Log
    - DIA_LOG_TRACE/DEBUG/INFO/WARNING/ERROR macros — preserved verbatim
    - Testing/MockSink.h — test utility (SD-O20)
  does_not_own:
    - Metric primitives (Counter/Gauge/Histogram) — DiaMetrics owns those
    - DiaCore output line logging (Dia::Core::Log::OutputLine) — DiaCore internal, never includes observation headers

feature_spec: docs/specs/features/dia/diaobservation/skeleton-and-logger-fold.md
---

# DiaObservation

Four-pillar observability: logs, traces, metrics, health. This feature delivers the `Log/` subsystem (folded from `DiaLogger`) and the module skeleton. All other subsystems are populated by Features #2–#6.

## Key Decisions

- `Logger` uses Meyer's singleton — available before any explicit Create().
- Async drain thread: lazy start on first `RegisterSink` via `std::call_once`. Wakes every 1 ms. `Stop()` signals exit and joins.
- `Dia::Logger::` namespace ceases to exist. All callers use `Dia::Observation::Log::`.
- `DiaCore` is the only dependency — DiaObservation can be included from any system without pulling in session or metrics infrastructure.
