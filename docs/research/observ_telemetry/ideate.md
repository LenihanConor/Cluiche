# Research: Ideate — Engine Observability / Telemetry

**Input:** docs/research/observ_telemetry/explore.md

## Framing for the Candidate List

Explore established (a) four pillars (logs, traces, metrics, health), (b) an AI-first consumption model, (c) a baseline that any candidate must produce (per-session directory + `session.json` + observation sink + timestamps + crash dump + exit-reason + retention ring + scenario tagging + minimal config), and (d) a perf finding that the existing logger's drain is sync on the main thread. The candidate list below is structured in three groups:

- **Group A — Foundation** (the v1 baseline shape, broken into the smallest sensible specs). One or more of these must ship first; everything else builds on them.
- **Group B — Pillar additions** (traces, metrics, health). Each is a separate candidate scoped at "minimal but useful".
- **Group C — Transport / scaling moves** (sidecar process, OTLP exporter, async drain). Optional layers; relevant if/when the foundation hits a wall.

Sizes use the `/research` convention: S ≤ 1 week, M = 1–3 weeks, L = 1–2 months, XL > 2 months. Every candidate names a home module and is checked against PD-001 through PD-010.

---

## Candidates

### Candidate 1: DiaObservation Foundation — session directory + observation sink + timestamps

**Home module/system:** New `DiaObservation` module under `Dia/`. Coexists with `DiaLogger` (which becomes one *producer* into the observation system, not a separate stack).
**Size:** M
**Description:**
Stand up the binding skeleton for AI-consumable observability. The module owns:
- A **`SessionManager`** that opens `Cluiche/out/<AppName>/sessions/<sessionId>/` on startup, generates the session ID, captures app/build identity, and writes `session.json` on exit (including the file inventory of the session directory).
- A **`JsonLineSink`** (the "observation sink") that writes `log.jsonl` with one structured record per line: `{ts_ns, session_id, level, channel, msg, scenario_step?, thread_id}`. Wire format mirrors OTel log records but does not depend on the OTel SDK.
- A **timestamp added to `LogEntry`** at producer side (monotonic `std::chrono::steady_clock::now()`), and **session ID** stamped in every record. Two new fields, one schema commit.
- An **errors-and-warnings retention ring** (separate, never-drop, 256 entries) that survives flooding from trace-level output. Dumped into `session.json` on exit and on crash.
- An **exit-reason capture** (`normal_exit | assert | unhandled_exception | terminate`) using `SetUnhandledExceptionFilter` + `std::set_terminate` + the existing `AssertSinkBridge`. On any non-normal exit, the last N log entries + retention ring + the session manifest get flushed to `session.json` and `crashes/<n>.json`.
- A **scenario tag stack** (push/pop StringCRC) — every log record while a tag is active carries it. Used by E2E tests to mark "step 3 of asset-load test".
- The **existing console sinks (`StdOutSink`, `DebugOutputSink`) stay for humans**; this new sink is added alongside.

**Primary value:** Every run leaves behind a single directory an AI agent can ingest. One file (`session.json`) tells the agent what happened, what files exist, and where to dig deeper. The schema becomes the contract for every later pillar.

**Compliance check:** PD-001 ✅ (session/scenario/channel IDs are StringCRC). PD-002 ✅ (ships as a `Module`; a `SessionModule` joins the main PU). PD-004 ✅ (`String1024`, `DynamicArrayC`, no STL in the public API; internal use of `<chrono>` and `<atomic>` is fine). PD-007 ✅ (uses `std::source_location`, `std::span` where helpful). PD-009 ✅ (writes under `Cluiche/out/<App>/`). PD-010 ✅ (config wires through `.diagame`).

---

### Candidate 2: DiaObservation Config — minimal `.diagame` integration + CLI override

**Home module/system:** `DiaObservation` (Candidate 1) + `Cluiche/CluicheGameBaseline` for the wiring + `Tools/` CLI for the override.
**Size:** S
**Description:**
The minimum-viable config surface that makes telemetry usable without recompiling. v1 reads config at startup only.

`.diagame` gains an optional `observation` block:
```json
"observation": {
  "log": {
    "default_level": "info",
    "channels": { "asset": "trace", "physics": "warning" }
  },
  "sinks": {
    "console": { "enabled": true },
    "observation": { "enabled": true, "directory": "auto" }
  },
  "scenario_tagging": true
}
```

CLI gains `dia run <app> --log-level=trace --log-channel=asset` overrides that win over file values. Schema is shaped so a future v2 can add `metrics`, `traces`, `health`, `live_reload`, `sidecar` sub-blocks without a breaking change.

**Primary value:** Closes the gap the user explicitly flagged: "where can I change log levels via config?" Lets the same build run with different verbosity for different investigations. Pre-stages the v2 config story without committing to its full shape.

**Compliance check:** PD-010 ✅ (hangs off `.diagame`). PD-001 ✅ (channel IDs are StringCRC). PD-004 ✅ (uses `Json::Value` from `DiaCore/Json` like the rest of the platform).

---

### Candidate 3: Async Logger Drain — fix the perf hit found in explore

**Home module/system:** `DiaLogger` (refactor in place; no API change for callers).
**Size:** S
**Description:**
Move `Logger::FlushBuffers` off the main PU thread. Internal change only — `Logger::Log` callers see no difference.

Mechanism: `Logger` spawns a single drain thread on first sink registration. That thread spins `FlushBuffers` at e.g. 1kHz (or on a wake condition variable). `LoggerModule::DoUpdate` no longer flushes synchronously — it's a no-op (or a wake hint). Sinks get a guarantee they are called from the drain thread, never the producer or the main PU.

Open sub-questions kept tactical:
- Drain interval (fixed 1ms vs producer-driven wake)?
- Sink ordering guarantees (per-thread FIFO preserved; cross-thread merge by timestamp is "best effort" until traces ship)?
- Shutdown ordering (drain must flush before sink Unregister — already a lifecycle event).

**Primary value:** Removes the perf hit the user identified. Unblocks raising default channel verbosity without paying a frame-time tax. Independent of any other candidate; can ship before or after Candidate 1.

**Compliance check:** PD-002 ✅ (Logger continues to be host-driven via `LoggerModule`; the drain thread is implementation-internal). No public API change.

---

### Candidate 4: Span-Based Tracing — `DiaTrace` minimal v1

**Home module/system:** `DiaObservation` (subsystem) or sibling `DiaTrace` module — preference for subsystem to keep the four pillars co-located.
**Size:** M
**Description:**
A scope-based tracing primitive matching OpenTelemetry's `Span` concept. API is two macros:
```cpp
DIA_TRACE_ZONE("Render::Frame");                    // RAII; uses std::source_location
DIA_TRACE_ZONE_NAMED(zoneVar, "Asset::Load");       // explicit handle for nested scopes
```
A `Span` records `{trace_id, span_id, parent_span_id, name (StringCRC), start_ns, end_ns, thread_id, scenario_step}`. Spans are written into the same observation sink as logs, with `record_type: "span"`.

v1 deliberately omits:
- GPU markers (PIX/RenderDoc) — adds when render budget bites.
- Cross-process span propagation — irrelevant until sidecar exists.
- Sampling — every span is recorded; rate-limit via channel level instead.

Includes per-thread span ring (drop-oldest on overflow) and a `MaxOpenSpans` guard to catch missing-end bugs at dev time.

**Primary value:** Closes the user's "trace ≠ log" question with the right primitive. Lets AI reconstruct call flow ("Render::Frame contained Render::Shadows containing Render::Shadows::Cascade[2]") rather than guessing from log sequence. Wire format is OTel-compatible so a future exporter is one sink class.

**Compliance check:** PD-001 ✅ (span names are StringCRC). PD-007 ✅ (uses `std::source_location` for auto-name fallback). PD-004 ✅ (no STL in public API).

---

### Candidate 5: Metrics — counters, gauges, histograms via a registry

**Home module/system:** `DiaObservation` (subsystem).
**Size:** M
**Description:**
Three primitives, all registered with a central `MetricRegistry` by StringCRC:
- `Counter` — monotonic, per-thread shard, reduced on snapshot. `counter("frames_rendered").Inc()`.
- `Gauge` — last-sampled value. `gauge("memory_used_mb").Set(123.4f)`.
- `Histogram` — fixed-bucket. `histogram("frame_time_ms").Observe(16.7f)`. Buckets declared at registration.

Snapshot model: every N ms (default 100ms; configurable) the `MetricRegistry` reduces per-thread shards into the canonical record and emits a metrics record into the observation sink (`record_type: "metric"`). On exit, a final snapshot is dumped into `session.json` so AI can see end-state numbers without reading the whole stream.

Subsumes the existing `MetricsCollectorModule` — its `MetricsSnapshot` becomes a registered set of gauges (FPS, frame-time, memory, uptime) populated by the same module that produces them today.

v1 deliberately omits:
- Multi-dimensional labels (`counter("http", method="GET")`) — large API surface, not needed yet.
- Aggregation in the consumer — the dump is per-snapshot.

**Primary value:** Replaces the ad-hoc per-module status structs (`ServerStats`, `MetricsSnapshot`) with a uniform mechanism. AI can answer "did asset load count match expectations? did warning count exceed threshold?" from the session metrics dump.

**Compliance check:** PD-001 ✅ (metric names are StringCRC). PD-004 ✅ (`DynamicArrayC` for the registry; per-thread shards use raw arrays + `std::atomic_ref` for unaligned increments).

---

### Candidate 6: Health Reporting — module status enum + session-level rollup

**Home module/system:** `DiaObservation` (subsystem) + opt-in implementation in each `Module`.
**Size:** S
**Description:**
A narrow surface for the user's health definition: "more than exit code 0/1". Two pieces:

1. **`IHealthReporter`** — modules optionally implement `Health Report() const` returning `{ status: OK | Degraded | Failing, last_warning_count, last_error_count, reason_code: StringCRC }`. The session manager polls registered reporters at exit (and on crash). Reporters are pull-based — modules don't have to push status updates every tick.

2. **Session-level rollup** in `session.json`: `{ overall_status, modules: [{name, status, reason}], errors: N, warnings: N, scenario_steps_completed: ["step1", "step2"], exit_reason }`. Computed by `SessionManager` from registered reporters + the retention ring + scenario tag history.

E2E tests get a clean predicate: "did the session finish with `overall_status == OK` and reach scenario step `final`?" rather than just exit code.

**Primary value:** Directly answers Q5 from explore. Gives E2E tests, AI agents, and humans a one-shot read on "was this run good?" without parsing every log line.

**Compliance check:** PD-001 ✅. PD-002 ✅ (reporters live on `Module`s — natural fit). PD-004 ✅.

---

### Candidate 7: Sidecar Observation Process — out-of-process consumer

**Home module/system:** New `DiaObservationSidecar` (sidecar host binary) + minor changes in `DiaObservation` to add an IPC sink. Sidecar binary lives under `Cluiche/Cluiche/CluicheObservationSidecar/`.
**Size:** L
**Description:**
A second process that consumes observation records over a named pipe (or local TCP), writes the `session/<id>/` directory itself, and survives engine crashes. Engine becomes a pure producer; if the engine asserts, the sidecar is the one that closes out `session.json` cleanly with `exit_reason: assert` and the last N records the engine had buffered before crashing.

Adds:
- `IpcSink` in `DiaObservation` — non-blocking write to a pipe; drop-on-overflow with an internal counter that itself becomes a metric.
- Sidecar binary that listens, deserializes, writes the session directory, optionally tail-streams to stdout for `dia run --tail`.
- `dia run` learns to spawn the sidecar before the engine and reap it after.

Open sub-questions:
- IPC framing: length-prefixed JSON-line, FlatBuffers, or protobuf (DebugServer already uses protobuf-ish).
- Engine-side fallback if the sidecar dies mid-run (write to local disk directly + `health.sidecar = Failing`).

**Primary value:** Crash-survival of the log stream. Zero engine overhead for sink I/O. Decouples session-directory layout from the engine, so v2 schema changes don't require a full Cluiche rebuild.

**Compliance check:** PD-002 ✅ (sidecar is its own process, not a Module). PD-006 ✅ (its own `.vcxproj`). PD-008/009 ✅ (output paths). The IPC schema becomes a stable contract — explicit versioning required.

---

### Candidate 8: OTLP Exporter — wire-format compat as a sink

**Home module/system:** New `DiaObservationOtlp` module (depends on `DiaObservation`). Optional; not part of v1 baseline.
**Size:** M
**Description:**
A sink that emits records over the OTLP/HTTP protocol (JSON variant — no protobuf SDK dependency) so the engine can speak directly to Jaeger / Tempo / Grafana / Honeycomb / SigNoz. Implements the three OTel record shapes (Logs, Traces, Metrics) as serializers over the existing `LogEntry` / `Span` / `Metric` records.

v1 limits:
- Outbound only, no traces/spans inbound.
- HTTP/JSON only (gRPC pulls in a heavy dep). Exports a batch every N seconds.
- No retry; drop on failure with a warning.

Doesn't pull `opentelemetry-cpp`. The point is wire compatibility — three serializer files, one HTTP POST loop. Anyone wanting full SDK semantics can add it later.

**Primary value:** Production-grade aggregation surface as a *plugin*, not a refactor. Long-term aggregation (the user's stated future need) lights up the day this ships, with no producer-side change.

**Compliance check:** PD-004 ✅ (uses `Json::Value`; HTTP via existing networking). PD-006 ✅. The wire format must already be OTel-shaped from Candidate 1's schema work, otherwise this candidate becomes a refactor.

---

### Candidate 9: Auto-Reload Config — telemetry config live-reload

**Home module/system:** `DiaObservation` extension; depends on Candidate 2.
**Size:** S
**Description:**
`SessionModule` watches the `.diagame` (or `.diatelemetry`) file mtime; on change, re-reads `observation` block and re-applies sink/level changes mid-run. Existing log entries don't retroactively change, but new entries respect the new levels.

v1 doesn't need this; explore explicitly noted the format must "not foreclose live reload". This candidate is the realisation when the dev loop demands it.

**Primary value:** Zero-restart iteration when chasing intermittent issues. AI agents can dial verbosity up mid-investigation without losing the run.

**Compliance check:** PD-010 ✅. Same surface as Candidate 2.

---

### Candidate 10: DebugServer Bridge — observation as a WebSocket subscription

**Home module/system:** `DiaDebugServer` extension; depends on Candidate 1.
**Size:** S
**Description:**
A `DebugServerObservationBridge` that subscribes to the observation stream and forwards records as WebSocket subscription topics: `observation.log`, `observation.trace`, `observation.metric`, `observation.health`. Editor / future debug UI can stream live records over the existing protocol without a second port or framing.

`DiaDebugServer` already has `SubscriptionManager`, broadcast plumbing, and `BroadcastCoreMetrics` — this candidate replaces hand-rolled metric serialization with the registry-driven version from Candidate 5.

**Primary value:** Live observation in CluicheEditor with no new transport. Reuses an already-shipping subsystem.

**Compliance check:** PD-001 ✅. PD-004 ✅. No new dependencies.

---

## Coverage Map

| Design axis (from explore) | Candidates that span it |
|----------------------------|-------------------------|
| Push vs pull | C1 (push from producers, pull from `IHealthReporter` in C6), C5 (push counter increments, pull-snapshot reduce), C10 (subscription model) |
| Instrumentation primitive — logs/counters/spans/health | C1 (logs), C4 (spans), C5 (counters/gauges/histograms), C6 (health) — all four covered |
| Counter storage | C5 explicitly: per-thread shard + reduce |
| Scope/zone API | C4: macro-based with RAII guard |
| Event taxonomy | C1 commits to a JSON schema; C4/C5 layer on it; C8 maps to OTel |
| Snapshot model | C1 (per-session manifest + on-exit), C5 (periodic + final), C6 (on-exit + on-crash) |
| Storage model | C1 (in-process file write), C7 (sidecar process), C8 (OTLP export), C10 (live WebSocket) |
| Aggregation site | C5 (in-process reduce); C8 enables out-of-process |
| Per-frame vs continuous | C5 chooses periodic snapshots; C4 chooses continuous spans; C1 logs are continuous |
| Compile-time gating | Implicit in every candidate via channel/level — no candidate adds compile-time strip |
| Module consumption | C1+C5+C6 establish a shared registry pattern |
| Editor integration | C10 |
| Sidecar vs in-process | C3 (in-process, async drain), C7 (sidecar), both exist as candidates |
| OTel adoption depth | C8 = wire format; never SDK adoption |
| Config | C2 (minimal v1), C9 (live reload) |

| Scope range | Candidates |
|-------------|------------|
| S (≤1 week) | C2, C3, C6, C9, C10 |
| M (1–3 weeks) | C1, C4, C5, C8 |
| L (1–2 months) | C7 |
| XL | (none — explicitly avoided) |

| Foundation / Pillar / Scaling group | Candidates |
|-------------------------------------|------------|
| Group A — Foundation | C1, C2, C3 |
| Group B — Pillars | C4 (traces), C5 (metrics), C6 (health) |
| Group C — Transport / scaling | C7 (sidecar), C8 (OTLP), C9 (live reload), C10 (WebSocket bridge) |

The list deliberately includes both fixes for the existing system (C3 drain), the foundation work (C1, C2), each pillar as its own candidate (C4, C5, C6), and the scaling moves (C7, C8, C9, C10) so evaluation can choose how aggressively to bundle vs sequence.
