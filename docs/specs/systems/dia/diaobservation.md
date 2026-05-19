# System Spec: DiaObservation

## Parent Application
@docs/specs/applications/dia.md

**Research:** @docs/research/observ_telemetry/summary.md

## Purpose

DiaObservation is the engine-wide observability system for the Dia engine. It captures four pillars — **logs, traces, metrics, health** — into a single per-session directory under `Cluiche/out/<AppName>/sessions/<sessionId>/`, with a binding `session.json` manifest as the consumer's entry point.

The system is explicitly designed as the **substrate for E2E testing**. Today's signal of "did the engine work?" is `exit_code == 0` plus stdout grep — neither expressive enough nor deterministic enough for AI-driven testing. DiaObservation gives a future `DiaE2E` orchestrator a one-file pass/fail predicate, scenario step tagging, per-module health reporting, and self-contained crash artifacts. The terminal goal of this work is not observability for its own sake; observability is the contract the test framework will sit on.

DiaObservation **subsumes** the existing `DiaLogger` system — its files (Logger, LogEntry, ISink, ThreadLogBuffer, sinks, AssertSinkBridge) move into `Dia/DiaObservation/Log/` as part of feature #1, and `DiaLogger` is marked Superseded. It also subsumes the hand-rolled metrics in `DiaApplicationFlow::MetricsCollectorModule` and `DiaDebugServer::ServerStats` — those become consumers of the new `MetricRegistry`.

The system is wire-compatible with OpenTelemetry (matching `trace_id`, `span_id`, `parent_span_id`, `start_unix_nano`, `severity_text`, `severity_number` field names) but does **not** depend on `opentelemetry-cpp`. A future OTLP exporter ships as a single sink class, never as a producer-side refactor.

**Dependency chain:**
`DiaMetrics → DiaCore (containers, StringCRC)`
`DiaObservation → DiaMetrics + DiaCore (Json, String1024, Log)`

## Responsibilities

### Session Management
- Open a per-session directory `Cluiche/out/<AppName>/sessions/<sessionId>/` on application start
- Generate `<sessionId>` as `<UTC-timestamp>-<short-uuid>` (sortable, collision-free)
- Write `session.json` binding manifest on exit (or on crash via the auto-dump path), including a `files` array indexing every other artifact
- Capture exit reason (`normal_exit | assert | unhandled_exception | terminate`) via `SetUnhandledExceptionFilter` + `std::set_terminate` + `AssertSinkBridge`
- Maintain a never-drop retention ring (256 entries) for `Warning`/`Error`/`Fatal` records — survives flooding, dumped into `session.json` and on crash
- Maintain a scenario tag stack (push/pop StringCRC) — every observation record while a tag is active carries it
- Poll `IHealthReporter` instances at exit (and on crash) and write the rollup into `session.json` and standalone `health.json`

### Logs (folded from DiaLogger)
- Provide `DIA_LOG_TRACE / DEBUG / INFO / WARNING / ERROR(channel, fmt, ...)` macros — the existing public API is preserved verbatim; only the include path changes
- Define log levels: Trace, Debug, Info, Warning, Error
- Compile out Trace/Debug levels in Release builds (existing behaviour preserved)
- Tag every log entry with a `StringCRC` channel
- Add `timestampNs` (monotonic `std::chrono::steady_clock`) and `sessionId` to every `LogEntry` — captured at producer side
- Maintain per-thread ring buffers (1024 entries) with zero contention on the write path (existing producer-side design preserved)
- **Drain off the main PU thread** — `Logger`-internal drain thread consumes thread-local rings and dispatches to sinks; existing `LoggerModule::DoUpdate` becomes a no-op (or a wake hint)
- Define `ISink` interface; ship three built-in sinks:
  - `StdOutSink` — human-readable `[level][channel] msg` to stdout (existing, preserved)
  - `DebugOutputSink` — same to `OutputDebugStringA` for Visual Studio (existing, preserved)
  - `ObservationFileSink` — structured JSON-line records to `log.jsonl` (NEW, the "observation sink")
- Support multiple sinks registered simultaneously, each with independent level threshold and channel filter (existing)

### Traces (Spans)
- Provide `DIA_TRACE_ZONE("Name")` and `DIA_TRACE_ZONE_NAMED(handle, "Name")` macros — RAII span scopes
- Records carry `{trace_id, span_id, parent_span_id, name (StringCRC), start_unix_nano, end_unix_nano, thread_id, scenario_step}`
- Per-thread span ring (drop-oldest on overflow); `MaxOpenSpans` guard catches missing-end bugs at dev time
- Closed spans emitted into the observation sink stream as `record_type: "span"`
- Use `std::source_location` for an auto-name fallback when no name is provided

### Metrics
- Provide three primitives registered with a `MetricRegistry` by StringCRC:
  - `Counter` — monotonic, per-thread shard, reduced on snapshot
  - `Gauge` — last-sampled value, atomic write
  - `Histogram` — fixed-bucket; buckets declared at registration
- Periodic snapshot every N ms (default 100ms; configurable) emits a `record_type: "metric_snapshot"` record
- Final snapshot at exit dumped to `metrics-final.json`
- Subsume the existing `MetricsCollectorModule` — FPS, frame-time, memory, uptime become registered gauges populated by the same module that produces them today

### Health
- Provide `IHealthReporter` interface — modules optionally implement `Health Report() const` returning `{ status: OK | Degraded | Failing, errors, warnings, reason: StringCRC }`
- `SessionManager` polls reporters at exit and on crash; computes session-level rollup
- Write rollup to `health.json` (standalone) AND embed in `session.json` (consumer convenience — see SD-O11)
- Provide `DIA_OBSERVATION_ASSERT(cond, msg)` and `DIA_OBSERVATION_FAIL(msg)` for E2E scenario failure reporting — writes a `level: "error"` log record AND flips the calling module's health to `Failing` with a `reason` StringCRC; does NOT crash the process

### Configuration
- Read `observation` block from `.diagame` `config` (PD-010); shape forward-compatible with future `metrics`, `traces`, `health`, `live_reload`, `sidecar` sub-blocks
- CLI overrides win over file values: `dia run <app> --log-level=trace --log-channel=asset`
- v1 reads config at startup; live reload not implemented but format must not foreclose it

### DebugServer Bridge
- Provide a `ObservationBridge` class (lives in `DiaDebugServer`, not in this module) that subscribes to the observation stream and forwards records as WebSocket subscription topics: `observation.log`, `observation.trace`, `observation.metric`, `observation.health`
- Replaces hand-rolled metric serialization in `DebugServer::BroadcastCoreMetrics` with registry-driven version

### Schema and contract
- Every record carries `schema_version: "1.0"` (semver, breaking changes bump major)
- Field names mirror OpenTelemetry where reasonable (`trace_id`, `span_id`, `start_unix_nano`, `severity_text`, `severity_number`)
- Session-directory layout, `session.json` shape, and `log.jsonl` / `trace.jsonl` / `metric.jsonl` record shapes are **public contracts** — future E2E orchestration depends on them; changes bump `schema_version` major

### Module documentation and build
- Provide `dia.dia.observation.architecture.module.md` YAML module documentation
- Provide `Dia/DiaObservation/DiaObservation.vcxproj` and `.vcxproj.filters`; register in `Cluiche.sln`
- Test utilities ship in `Dia/DiaObservation/Testing/` (mock sink, session fixture, scenario harness)

## Non-Responsibilities

- **The E2E framework itself** (`dia e2e` CLI, suite manifest, scenario subprocess spawning, JUnit emitter, per-scenario summary writer) — future `DiaE2E` system, not in this scope
- **Sidecar process** — deferred (C7); reconsider when crash-survival of log stream is needed
- **OTLP exporter** — deferred (C8); ships as a future sink class when an aggregation backend exists. Wire format is OTel-shaped from v1 so this is non-refactor work later
- **Live config reload** — deferred (C9); v1 reads at startup, format must not foreclose live reload
- **GPU markers (PIX/RenderDoc bridge)** — out of scope for v1
- **Multi-dimensional metric labels** (`counter("http", method="GET")`) — out of scope for v1
- **Cross-process span propagation** — irrelevant until sidecar exists
- **Sampling** — every span and log record is captured; rate-limit via channel level
- **Long-term aggregation, persistent dashboards, historical trend analysis** — explicitly framed by user as future work, not v1
- **Application lifecycle for the observation system** — applications own a `SessionModule` that drives lifecycle (parallel to existing `LoggerModule` pattern)
- **Replacing `Dia::Core::Log`** — Assert and DiaCore internals continue using it (existing constraint preserved)
- **DiaCore consumption** — DiaCore cannot include observation headers (would create a cycle); DiaCore continues with `Dia::Core::Log::OutputLine`

## Public Interfaces

### Session

```cpp
namespace Dia::Observation {
    class SessionManager {
    public:
        static SessionManager& Instance();

        // Lifecycle (driven by a host SessionModule)
        bool Start(const SessionConfig& config);
        void Tick(float deltaTime);
        void Stop();

        // Identity
        const Dia::Core::StringCRC& GetSessionId() const;
        const char* GetSessionDirectory() const;

        // Scenario tagging
        void PushScenarioStep(const Dia::Core::StringCRC& step);
        void PopScenarioStep();
        Dia::Core::StringCRC GetCurrentScenarioStep() const;

        // Health reporting
        void RegisterHealthReporter(IHealthReporter* reporter);
        void UnregisterHealthReporter(IHealthReporter* reporter);

    private:
        SessionManager();
        ~SessionManager();
    };

    struct SessionConfig {
        char appName[64];
        char buildVersion[64];
        char buildConfig[16];
        bool autoStartObservationSink;
    };
}
```

### Logs (preserved API, new home)

```cpp
namespace Dia::Observation::Log {
    enum class LogLevel : unsigned char { kTrace, kDebug, kInfo, kWarning, kError };

    struct LogEntry {
        LogLevel              level;
        Dia::Core::StringCRC  channel;
        char                  message[1024];
        uint64_t              timestampNs;       // NEW — added in v1
        uint32_t              threadId;          // NEW — added in v1
        Dia::Core::StringCRC  scenarioStep;      // NEW — added in v1
        // sessionId is implicit; SessionManager::Instance() owns it
    };

    class ISink { /* unchanged from DiaLogger */ };

    class Logger {
    public:
        static Logger& Instance();
        void RegisterSink(ISink* sink);
        void UnregisterSink(ISink* sink);
        void RegisterThreadBuffer();
        void UnregisterThreadBuffer();
        void Log(LogLevel level, const Dia::Core::StringCRC& channel,
                 const char* fmt, ...);
        // FlushBuffers is now a no-op for callers; drain runs on Logger's own thread
    };
}

// Macros — public API preserved verbatim from DiaLogger
#define DIA_LOG_INFO(channel, fmt, ...)    /* same as DiaLogger */
#define DIA_LOG_WARNING(channel, fmt, ...) /* same */
#define DIA_LOG_ERROR(channel, fmt, ...)   /* same */
// ... etc

// Built-in sinks
namespace Dia::Observation::Log {
    class StdOutSink      : public ISink { /* preserved */ };
    class DebugOutputSink : public ISink { /* preserved */ };
    class ObservationFileSink    : public ISink { /* NEW — writes log.jsonl */ };
}
```

### Traces

```cpp
namespace Dia::Observation::Trace {
    struct Span {
        Dia::Core::StringCRC name;
        uint64_t             traceId;
        uint64_t             spanId;
        uint64_t             parentSpanId;   // 0 if root
        uint64_t             startUnixNano;
        uint64_t             endUnixNano;    // 0 if open
        uint32_t             threadId;
        Dia::Core::StringCRC scenarioStep;
    };

    class ScopedZone {
    public:
        explicit ScopedZone(const Dia::Core::StringCRC& name,
                            const std::source_location& loc =
                                std::source_location::current());
        ~ScopedZone();
        ScopedZone(const ScopedZone&) = delete;
        ScopedZone& operator=(const ScopedZone&) = delete;
    };
}

#define DIA_TRACE_ZONE(name) \
    Dia::Observation::Trace::ScopedZone _dia_zone_##__LINE__(Dia::Core::StringCRC(name))

#define DIA_TRACE_ZONE_NAMED(var, name) \
    Dia::Observation::Trace::ScopedZone var(Dia::Core::StringCRC(name))
```

### Metrics

```cpp
namespace Dia::Observation::Metric {
    class MetricRegistry {
    public:
        static MetricRegistry& Instance();

        Counter*   RegisterCounter(const Dia::Core::StringCRC& name);
        Gauge*     RegisterGauge(const Dia::Core::StringCRC& name);
        Histogram* RegisterHistogram(const Dia::Core::StringCRC& name,
                                     const float* bucketBounds,
                                     unsigned int bucketCount);

        Counter*   FindCounter(const Dia::Core::StringCRC& name) const;
        Gauge*     FindGauge(const Dia::Core::StringCRC& name) const;
        Histogram* FindHistogram(const Dia::Core::StringCRC& name) const;

        // Snapshot — drives both periodic emission and final dump
        void Snapshot(MetricSnapshot& out) const;
    };

    class Counter {
    public:
        void Inc(uint64_t delta = 1);  // per-thread shard, lock-free
        uint64_t Value() const;        // reduce-on-read
    };

    class Gauge {
    public:
        void  Set(double value);
        double Value() const;
    };

    class Histogram {
    public:
        void Observe(double value);
        // Read via Snapshot() — bucket counts, p50/p95/p99 computed at snapshot time
    };
}
```

### Health

```cpp
namespace Dia::Observation::Health {
    enum class HealthStatus : uint8_t { kOK, kDegraded, kFailing };

    struct Health {
        HealthStatus         status;
        unsigned int         errors;
        unsigned int         warnings;
        Dia::Core::StringCRC reason;  // kInvalidCRC if status == kOK
    };

    class IHealthReporter {
    public:
        virtual ~IHealthReporter() = default;
        virtual Dia::Core::StringCRC GetReporterName() const = 0;
        virtual Health               Report() const = 0;
    };
}

// E2E failure macros — log + flip caller's health to Failing
#define DIA_OBSERVATION_ASSERT(cond, reason_crc, msg) /* ... */
#define DIA_OBSERVATION_FAIL(reason_crc, msg)         /* ... */
```

### Wire format — `session.json` top-level shape

The full schema is documented in `docs/research/observ_telemetry/choose.md` and frozen in feature #2. Top-level keys:

```json
{
  "schema_version": "1.0",
  "session":   { "id", "app_name", "build_version", "build_config", "platform", "started_unix_nano", "ended_unix_nano", "duration_ms" },
  "exit":      { "reason", "code", "stage_at_exit" },
  "counts":    { "errors", "warnings", "frames", "scenario_steps_completed" },
  "scenario_steps": [ { "step", "started_unix_nano", "ended_unix_nano" } ],
  "modules":   [ { "name", "status", "errors", "warnings", "reason" } ],
  "retained_warnings_and_errors": [ ... ],
  "files":     [ { "path", "kind", "bytes" } ]
}
```

### Wire format — `log.jsonl` record shape

```json
{"schema_version":"1.0","ts_unix_nano":...,"session_id":"...","level":"info","severity_number":9,"channel":"asset","scenario_step":"load","thread_id":12,"msg":"..."}
```

`severity_number` follows OpenTelemetry's mapping: Trace=1, Debug=5, Info=9, Warning=13, Error=17.

## Features

| # | Feature | Description | Spec | Status |
|---|---------|-------------|------|--------|
| 1 | DiaObservation Skeleton + DiaLogger Fold + Async Drain | Create `Dia/DiaObservation/` module skeleton with `Log/` subsystem; move all DiaLogger files into it; update every caller's includes; delete `Dia/DiaLogger/`. Then implement async drain — `Logger::FlushBuffers` becomes drain-thread driven, `LoggerModule::DoUpdate` becomes no-op. | [skeleton-and-logger-fold.md](../../features/dia/diaobservation/skeleton-and-logger-fold.md) | Approved |
| 2 | DiaObservation Foundation | `SessionManager`, session directory, `ObservationFileSink`, timestamps + scenario step + thread id added to `LogEntry`, retention ring (warnings/errors), exit-reason capture (assert/exception/terminate), crash auto-dump (`crashes/<n>.json`), `session.json` writer, `SessionModule` host. Freezes the `session.json` and `log.jsonl` schemas at v1.0. | [foundation.md](../../features/dia/diaobservation/foundation.md) | Approved |
| 3 | DiaObservation Config | `.diagame` `observation` block parser; CLI overrides (`--log-level`, `--log-channel`); per-channel level threshold; sink enable/disable; scenario tagging toggle. Forward-compatible with future `metrics`/`traces`/`health`/`live_reload` sub-blocks. | [config.md](../../features/dia/diaobservation/config.md) | Approved |
| 4 | DiaTrace Spans | `DIA_TRACE_ZONE` / `DIA_TRACE_ZONE_NAMED` macros, `Span` struct, per-thread span ring with drop-oldest, `MaxOpenSpans` dev-time guard, `trace.jsonl` writer, OTel-compatible field names. | [trace-spans.md](../../features/dia/diaobservation/trace-spans.md) | Approved |
| 5 | DiaMetrics Registry | `MetricRegistry`, `Counter` (per-thread shard + reduce), `Gauge` (atomic), `Histogram` (fixed buckets, p50/p95/p99 at snapshot), periodic snapshots → `metric.jsonl`, final dump → `metrics-final.json`. Subsume `MetricsCollectorModule` (FPS, frame-time, memory, uptime become registered gauges) and `DebugServer::ServerStats`. | [metrics-registry.md](../../features/dia/diaobservation/metrics-registry.md) | Approved |
| 6 | DiaHealth Reporting | `IHealthReporter` interface, `HealthStatus` enum, `Health` struct, session-level rollup (`overall_status`, per-module status), `health.json` writer, `DIA_OBSERVATION_ASSERT` / `DIA_OBSERVATION_FAIL` macros. Health polled at exit AND on crash before manifest write. | [health-reporting.md](../../features/dia/diaobservation/health-reporting.md) | Approved |
| 7 | DebugServer Observation Bridge | `ObservationBridge` class in `DiaDebugServer` that subscribes to observation streams and forwards as WebSocket topics (`observation.log`, `observation.trace`, `observation.metric`, `observation.health`). Replaces hand-rolled `BroadcastCoreMetrics` with registry-driven version. | [debugserver-bridge.md](../../features/dia/diaobservation/debugserver-bridge.md) | Approved |

## Dependencies on Other Systems

**Required:**
- **DiaCore** — `StringCRC` (identifiers), `String1024` (formatting), `DynamicArrayC` / `HashTable` (registries), `Json` (`.diagame` parsing, manifest writing), `Log::OutputLine` (DebugOutputSink continues to use it), `DIA_ASSERT`

**Optional:**
- **DiaApplicationFlow** — only at the `SessionModule` host layer, which is application code; the system itself does not depend on `DiaApplicationFlow`

**Explicitly excluded:**
- **DiaLogger** — folded into this system; superseded
- **DiaApplicationFlow** — observation primitives must be callable from any thread, including before/after lifecycle; no Module dependency
- **DiaWebSocket** / **DiaDebugServer** — feature #7 (the bridge) lives *in* `DiaDebugServer`, not here; this system does not pull WebSocket as a dependency

**Dependents:**
- **All Dia systems and applications that currently use `DIA_LOG_*`** — include path changes from `<DiaLogger/...>` to `<DiaObservation/Log/...>`; macro API preserved verbatim
- **DiaApplicationFlow** — `MetricsCollectorModule` is rewritten as a `MetricRegistry` consumer (or removed in favour of direct registry access from existing PUs)
- **DiaDebugServer** — `ObservationBridge` lives here; `BroadcastCoreMetrics` rewritten on top of `MetricRegistry`
- **CluicheTest** — owns a `SessionModule` (parallel to existing `LoggerModule` pattern); `.diagame` gains `observation` config block
- **CluicheEditor** — same, plus consumes `observation.*` WebSocket topics for live editor panels
- **Future `DiaE2E` system** — consumes session directory layout as its primary contract

## Out of Scope

- The `DiaE2E` test framework itself (orchestrator CLI, suite manifest, JUnit emitter, scenario subprocess spawning) — future system on top of v1
- Sidecar observation process (C7) — deferred until crash-survival of log stream is needed
- OTLP exporter (C8) — deferred until aggregation backend exists; ships as a sink class
- Live config reload (C9) — deferred until dev-loop pain is real
- GPU markers (PIX / RenderDoc bridge)
- Multi-dimensional metric labels
- Cross-process span propagation
- Sampling
- Long-term aggregation, dashboards, historical trends
- Snapshot / golden-image testing, network-driven scenarios — even in the future E2E system's first pass
- Replacing `Dia::Core::Log` — DiaCore internals continue using it
- Mid-run sink registration (existing DiaLogger constraint preserved — sinks register at SessionModule start, not during update)

## Decisions

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| SD-O01 | Logs, traces, health in one `DiaObservation` module; metrics extracted to standalone `DiaMetrics` sibling | Logs/traces/health are intrinsically session-aware — a span without a session ID has no `trace_id`; a health report without a session has no rollup. Metrics (`Counter`/`Gauge`/`Histogram`) are general-purpose primitives useful for profiling, gameplay stats, and editor panels without any observation context. `DiaMetrics → DiaCore` only; `DiaObservation → DiaMetrics + DiaCore`. **Amended in Feature #5** from original "one module for all four pillars". | All features | Accepted | Yes |
| SD-O02 | DiaLogger is folded into `Dia/DiaObservation/Log/`; `Dia/DiaLogger/` is deleted | Logs are one of the four pillars; keeping it as a sibling is asymmetric and forces consumers to import two modules. Macro API preserved verbatim — only include paths change. Schema coupling (timestamp + session ID on every entry) is unavoidable, so the producer must already know about the observation schema. | Feature #1 | Accepted | Yes |
| SD-O03 | Logger drain runs on a dedicated thread inside `Logger`, not on the main PU | Producer side (`Logger::Log` writing to thread-local rings) is already cheap; the cost is `FlushBuffers` running synchronously on main + per-entry `printf` / `OutputDebugStringA`. Async drain removes the perf hit without changing the producer API. Sidecar (C7) is the next escalation, not this one. | Feature #1 | Accepted | Yes |
| SD-O04 | Two distinct sinks coexist: human console (`StdOutSink` / `DebugOutputSink`) and AI JSON-line (`ObservationFileSink`) | Humans want `[INFO][channel] msg`; AI wants `{"level":"info",...}`. Forcing one format on both audiences makes neither happy. Both ship by default; config can disable either. | Feature #1, #2 | Accepted | Yes |
| SD-O05 | `schema_version: "1.0"` on every record from day one | Adding versioning retroactively is a multi-week migration once consumers exist. Doing it now is two characters in two places. Future breaking changes bump major; additive bumps minor. | All features | Accepted | Yes |
| SD-O06 | OpenTelemetry wire-format alignment, no SDK adoption | Field names match OTel where reasonable (`trace_id`, `span_id`, `parent_span_id`, `start_unix_nano`, `severity_text`, `severity_number`). `opentelemetry-cpp` is NOT a dependency. A future OTLP exporter (C8) is one sink class translating internal records to OTLP/HTTP — never a producer-side refactor. | All features | Accepted | Yes |
| SD-O07 | Session = a directory at `Cluiche/out/<App>/sessions/<id>/`; `<id>` = `<UTC-timestamp>-<short-uuid>` | Consumers (AI, CI, future E2E) get one directory containing all artifacts for one run, sortable chronologically by name, never collide. Aligns with PD-009. | Feature #2 | Accepted | Yes |
| SD-O08 | Session-directory layout and `session.json` shape are public contracts | Future E2E orchestration depends on them. Internal-only treatment would foreclose the terminal goal of this work. Changes bump `schema_version` major. | Feature #2 | Accepted | Yes |
| SD-O09 | Sessions stay flat under `sessions/`; suites (future) live as siblings under `suites/` and reference sessions by id + relative path | Whether a run was triggered by a developer or an E2E suite, it produces a session in the same shape. Suites are metadata about a group of sessions, not the sessions themselves. Avoids "is this directory a session or a suite?" ambiguity. | Feature #2 | Accepted | Yes |
| SD-O10 | `IHealthReporter` is polled at every exit, including crash | `SessionManager`'s exception/assert handler polls health one final time before writing `session.json` and `health.json`. A scenario module reporting `Failing` must reach the manifest even if the crash handler runs. | Feature #2, #6 | Accepted | Yes |
| SD-O11 | Health is duplicated across three sinks: `session.json` (embedded), `log.jsonl` (`record_type: "health"` events on transition), `health.json` (standalone final rollup) | Three audiences, three reads. AI investigating one run reads `session.json`. Time-correlated analysis reads `log.jsonl`. CI / OTLP / editor panel reads `health.json`. Data is written once by `SessionManager`; the producer side cost is negligible. | Feature #6 | Accepted | Yes |
| SD-O12 | `DIA_OBSERVATION_ASSERT(cond, reason, msg)` and `DIA_OBSERVATION_FAIL(reason, msg)` belong to feature #6's API surface | These are how E2E scenarios report failure WITHOUT crashing the process. Each writes a `level: "error"` log record AND flips the calling module's health to `Failing` with a `reason` StringCRC. Existing `DIA_ASSERT` continues to crash; these new macros do not. | Feature #6 | Accepted | Yes |
| SD-O13 | Scenario steps with `ended_unix_nano: null` mean "started but never closed" | Documented schema behaviour, not implementation detail. The future E2E orchestrator's `actual_steps == expected_steps` predicate depends on this distinction. | Feature #2 | Accepted | Yes |
| SD-O14 | Stable monotonic timestamp on every observation record (`uint64_t timestampNs` from `std::chrono::steady_clock`) | Required for cross-thread ordering, span correlation, OTel wire-compat. Producer-side capture (not drain-time) so cross-thread interleaving is correct. `system_clock` would be wrong (can jump on NTP). | All features | Accepted | Yes |
| SD-O15 | Errors-and-warnings retention ring (256 entries, never-drop) separate from main per-thread rings | Guarantees critical entries survive a flood of trace-level output. Dumped into `session.json` and `crashes/<n>.json`. Size is fixed; memory cost is bounded (~256 KB). | Feature #2 | Accepted | Yes |
| SD-O16 | Per-session ID stamped on every record | Disambiguate runs when AI compares run #N vs run #N-1. Required for any future multi-run analysis. SessionManager owns the canonical id; producers read it via `SessionManager::Instance().GetSessionId()`. | All features | Accepted | Yes |
| SD-O17 | Counter primitive uses per-thread shards reduced on snapshot, not a single shared atomic | Per-thread is faster for hot paths; reduce happens once per snapshot (default 100ms). A spin-locked counter on a 10kHz hot path is the textbook 30% frame-time regression. Reads are slightly stale until the next snapshot — acceptable for engine telemetry. | Feature #5 | Accepted | Yes |
| SD-O18 | `MaxOpenSpans` dev-time guard catches missing `End` calls | RAII spans should never leak, but bugs happen. In Debug, the per-thread span ring asserts when `OpenSpansCount > MaxOpenSpans` (default 64). In Release, oldest open span is force-closed with a warning. | Feature #4 | Accepted | Yes |
| SD-O19 | Config is in v1 (minimal); v2 will unify with broader engine config in a separate user-led pass | Smallest surface: per-channel log level + sink enable/disable + scenario tagging toggle. `.diagame` `observation` block + CLI overrides. Format MUST NOT foreclose live reload (no compile-time templates). v2 will add `metrics`, `traces`, `health`, `live_reload`, `sidecar` sub-blocks. | Feature #3 | Accepted | Yes |
| SD-O20 | Test utilities ship in `Dia/DiaObservation/Testing/` | Mock sink, session fixture, scenario harness live with the library; consumers opt in via include. Platform-wide pattern (matches DiaStateMachine, DiaRig2D). | All features | Accepted | Yes |
| SD-O21 | DiaCore is the only required dependency; the system is callable from anywhere above DiaCore | Observation must work from any thread, in any phase, before/after `DiaApplicationFlow` is up. No `Module` subclass in this system. The `SessionModule` host lives in application code (`Cluiche/CluicheGameBaseline/`), not in this module. | All features | Accepted | Yes |
| SD-O22 | DiaCore cannot include observation headers | Would create a cycle. DiaCore continues to use `Dia::Core::Log::OutputLine` for its own internal output (the same constraint as the current DiaLogger arrangement). | All features | Accepted | Yes |
| SD-O23 | DiaLogger system spec marked Superseded by feature #1 | Once feature #1 lands and `Dia/DiaLogger/` is deleted, `docs/specs/systems/dia/dialogger.md` Status is set to `Superseded` with a pointer to this spec. Channel registry table from DiaLogger spec is migrated into this spec's documentation. | Feature #1 | Accepted | Yes |

**Status values:** `Proposed` · `Accepted` · `Rejected` · `Superseded`
**Binding:** `Yes` = enforced constraint on all features in this system · `No` = guidance only

## Inherited Binding Decisions

| ID | Source | Decision | Implication for this system |
|----|--------|----------|----------------------------|
| PD-001 | Platform | StringCRC for all entity/component IDs | Session id, scenario step, log channel, span name, metric name, health module name, reason codes — all StringCRC. No raw string maps in public API. |
| PD-002 | Platform | ProcessingUnit/Phase/Module architecture | Observation primitives are callable outside the framework (logging from any thread, any time). The optional `SessionModule` host is application code, not in this system (SD-O21). |
| PD-003 | Platform | Component-based entities | Mostly orthogonal — observation is engine-side, not entity-side. Per-component metrics is a future v2 question. |
| PD-004 | Platform | No STL containers in public APIs | Public API uses `String1024`, `DynamicArrayC`, `Json::Value`. Internal use of `<chrono>`, `<atomic>`, `<source_location>`, `<thread>` is permitted (not a container). |
| PD-005 | Platform | x64 is the only supported build target | `std::chrono::steady_clock` (resolves to `QueryPerformanceCounter`) is fine; no platform abstraction needed. `thread_local` well-supported on x64 MSVC. |
| PD-006 | Platform | Visual Studio project files are source of truth | `Dia/DiaObservation/DiaObservation.vcxproj` and `.vcxproj.filters` created and manually maintained. Registered in `Cluiche.sln`. |
| PD-007 | Platform | C++20 is the required language standard | `std::source_location` for span auto-naming (SD-O18); `std::span` for snapshot views; `std::atomic_ref` for unaligned counter increments. |
| PD-008 | Platform | Directory.Build.props owns OutDir/IntDir/toolchain | `DiaObservation.vcxproj` must NOT override `OutDir`, `IntDir`, `PlatformToolset`, `WindowsTargetPlatformVersion`, or `LanguageStandard`. |
| PD-009 | Platform | Generated non-binary output under `Cluiche/out/<AppName>/` | Session directories live at `Cluiche/out/<AppName>/sessions/<sessionId>/`. Future `suites/` directory will be a sibling, not nested (SD-O09). |
| PD-010 | Platform | `.diagame` is the project root file; `.diastage` declares stage metadata; typed imports route loading | Telemetry config slots into `.diagame` `config.observation` block (SD-O19). v1 reads at startup. Future live-reload format must not be foreclosed. |
| AD-001 | Dia App | Module system with YAML frontmatter | Create `dia.dia.observation.architecture.module.md` with public API, responsibilities, and dependency declarations. |
| AD-002 | Dia App | No STL containers in public APIs | Reinforces PD-004. |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | All code in `Dia::Observation::` (and subnamespaces `Dia::Observation::Log`, `::Trace`, `::Metric`, `::Health`). |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Folding | Does folding `DiaLogger` into `DiaObservation/Log/` break callers' include paths? | Yes — every `<DiaLogger/...>` becomes `<DiaObservation/Log/...>`. The macro API (`DIA_LOG_*`) is preserved verbatim, so call sites do not change. The fold is a mechanical sweep done in feature #1, scoped to one PR to minimise disruption. The DiaLogger system spec is marked Superseded once the fold lands. |
| 2 | Folding | What about the existing `DiaLogger.vcxproj` and registry references? | Solution file (`Cluiche.sln`), dependent project lists, and `docs/reference/registry/module-registry.md` all need updating in feature #1. Listed as explicit subtasks in the feature plan. |
| 3 | Threading | What happens if a thread logs before calling `RegisterThreadBuffer`? | Same as today's DiaLogger — the entry is silently dropped. Logger checks for a valid buffer; no buffer means programmer error but no crash. Documented as a constraint. |
| 4 | Threading | What happens to the new drain thread on shutdown? | `Logger::Stop()` (called by `SessionModule::DoStop`) signals the drain thread, waits for it to drain remaining entries, then joins it. Sinks are unregistered AFTER drain join to avoid use-after-free. Documented in the feature #1 plan. |
| 5 | Threading | Is the drain thread a regular `std::thread` or part of the engine's `JobSystem`? | `std::thread`. The drain runs forever, owned by `Logger` (a singleton with a longer lifetime than `JobSystem`). JobSystem is for finite work; observation drain is infrastructure. |
| 6 | Schema | What if `session.json` is partially written when the process is killed (`terminate` exit reason)? | Possible. `SessionManager` writes via temp-file-and-rename only at the *very end* of `Stop()`; before that, `session.json` does not exist. On crash, the handler writes a best-effort `session.json` directly (no temp file) before letting the OS reap the process. Consumers must tolerate a missing or truncated `session.json` — the `files` array signals which artifacts are valid. |
| 7 | Schema | Why mirror OpenTelemetry field names without using `opentelemetry-cpp`? | OTel SDK is heavy (gRPC, protobuf, complex allocator) and incompatible with the engine's container conventions (PD-004). Wire-format alignment is the smallest commitment that pre-stages a future OTLP exporter (C8) as a sink-class change rather than a refactor. |
| 8 | Schema | Are field names lowercase-snake (`session_id`) or camelCase (`sessionId`)? | Lowercase-snake to match OpenTelemetry / OTLP / chrome://tracing. Internal C++ types use camelCase fields per Dia convention; serialisation translates. |
| 9 | Health | Can `IHealthReporter::Report()` be called from any thread? | Yes — `SessionManager` polls reporters from its exit thread (and crash handler), which may not be the same thread that registered them. Reporters MUST be thread-safe in their `Report()` implementation. Documented in the feature #6 spec. |
| 10 | Health | What if a reporter crashes inside `Report()`? | The crash propagates. We don't catch exceptions inside the polling loop because the polling loop may itself be running in the crash handler. Reporters MUST NOT throw or assert from `Report()`. |
| 11 | E2E | What does `DIA_OBSERVATION_ASSERT(false, ...)` do exactly? | (a) Writes `level: "error", channel: "scenario", record_type: "log"` with the message; (b) Flips the calling module's `IHealthReporter` to `kFailing` with the supplied `reason`; (c) Returns control to the caller. Does NOT trigger `DIA_ASSERT`, does NOT crash. The macro is the contract a future E2E framework depends on (SD-O12). |
| 12 | Crash | What happens to open spans on crash? | They appear in `crashes/<n>.json` under `open_spans_at_crash` with their `start_unix_nano` and `duration_ms_so_far`. They are NOT auto-closed in `trace.jsonl` because their actual end time is undefined. This is a documented schema-version-1.0 behaviour. |
| 13 | Crash | What if an assert fires DURING the crash handler? | `SessionManager`'s crash handler must not log via `DIA_LOG_*` (would re-enter the logger). It writes via direct `Json::Value` → file path, bypassing the sink stack. Documented in feature #2 plan. |
| 14 | Wire | What separator is used inside `log.jsonl` — newline or `\r\n`? | Single `\n` (LF), even on Windows. Matches JSON-line / NDJSON convention. Tools that consume JSON-line don't expect CRLF. |
| 15 | Performance | What is the producer-side cost of `DIA_LOG_INFO` after this change? | Same as today: `vsnprintf` into a thread-local stack buffer, copy into a thread-local ring. ~50–200 ns. The drain refactor is consumer-side; producers don't notice. Profiled in feature #1's verification gate. |
| 16 | Performance | What is the producer-side cost of `DIA_TRACE_ZONE("Name")`? | One `steady_clock::now()` call (~30 ns on Windows x64) at construction, one at destruction. Span record copied into thread-local ring (~50 ns). Total budget: <200 ns per zone. Profiled in feature #4's verification gate. |
| 17 | Performance | Counter `Inc()` cost? | Per-thread shard increment via `std::atomic_ref` (lock-free, no contention with other threads). ~5–10 ns. Reduce happens at snapshot time, not on `Inc()`. |
| 18 | Memory | What is the per-thread memory cost of observation? | Log: 1024 entries × ~1080 bytes = ~1.05 MB (preserved from DiaLogger). Trace: 256 spans × ~64 bytes = ~16 KB. Counter shards: ~4 KB. Total: ~1.07 MB per logging thread. Acceptable for an engine with typically 2–4 threads. |
| 19 | Memory | What is the session-directory disk footprint? | Bounded by run length and verbosity. A typical 3-minute CluicheTest run at default verbosity: `log.jsonl` ~50 KB, `trace.jsonl` ~200 KB, `metric.jsonl` ~10 KB, `session.json` ~5 KB. Total: ~265 KB per session. |
| 20 | Lifecycle | What if `SessionManager::Start()` is never called? | `Logger::Log` continues to work (sinks still drain), but no session directory is created. `ObservationFileSink` checks `SessionManager::IsActive()` and silently no-ops. This preserves DiaLogger's existing "logger works without a host module" behaviour for tools and unit tests. |
| 21 | Lifecycle | Can multiple `SessionManager::Start()` calls happen in one process? | No — single session per process. Subsequent `Start()` calls return false and log a warning. If a test fixture needs multiple sessions, it spawns subprocesses (which is what the future E2E framework does anyway). |
| 22 | Lifecycle | What if two threads simultaneously call `PushScenarioStep()`? | The scenario stack is thread-local. Each thread has its own stack; records are stamped with the calling thread's current step. Cross-thread step propagation is out of scope (would require explicit context passing, like OTel `Span::Current()`). |
| 23 | Wire-compat | If the OTel spec changes, do we follow? | Within `schema_version: "1.0"`, no — our fields are frozen. A `schema_version: "2.0"` could realign. Until then, we add fields that don't conflict (additive minor bump) but don't break existing consumers. |
| 24 | Future E2E | Does this spec commit to building `DiaE2E`? | No. This spec commits only to the contracts a future `DiaE2E` would consume (session layout, scenario tags, health interface, `DIA_OBSERVATION_ASSERT`/`FAIL` macros, `null` step semantics). The framework itself is a separate future system. |
| 25 | Migration | What happens to the channels listed in DiaLogger's "Channel Registry" table? | Migrated verbatim into the documentation for this system. Channel naming and content conventions are unchanged; only the include path moves. The migration is a documentation-only task in feature #1. |

## Status

`Approved` — 2026-05-17. Spec written from research at `docs/research/observ_telemetry/summary.md`. Steps 3 (Inherited Binding Decisions) and 4 (AI Review Questions) complete and confirmed.

All 7 feature specs Approved — 2026-05-17.

**Next:**
- Create plan file `diaobservation.plan.md` when implementation starts (serial order: #1 → #2 → #3 → #4/#5/#6 can partially overlap → #7 last)
- Note: `DiaMetrics` is a new standalone module (sibling to `DiaObservation`) — needs its own system spec or can be treated as a sub-spec of this feature work. Confirm before implementation.
- System becomes `In Progress` once feature #1 implementation begins; `Done` only when all 7 features are `Done`.
- System Status becomes `In Progress` once feature #1 begins, `Done` only when all 7 features are `Approved`
