# Research: Choice — Engine Observability / Telemetry

**Date:** 2026-05-17
**Chosen candidate:** `DiaObservation` system — bundle of C1 + C2 + C3 + C4 + C5 + C6 + C10

## Rationale

The user committed to a bundle covering Group A (foundation: C1 session directory + C2 config + C3 async drain), Group B (pillars: C4 traces + C5 metrics + C6 health), and one Group C item (C10 DebugServer WebSocket bridge). The Group C scaling moves not chosen — C7 sidecar, C8 OTLP exporter, C9 live config reload — are deferred for the deliberate reasons noted below.

**Terminal goal: E2E testing.** The user explicitly stated that the whole point of the observation work is to enable E2E testing. The v1 bundle is *the substrate* a future E2E framework will sit on — every binding decision below is chosen to keep that future framework cheap to build. The v1 bundle does **not** include the test framework itself; that ships as a separate later system. What v1 commits to is the session-directory layout, the scenario tag mechanism, the `IHealthReporter` surface, the crash-dump shape, and the `DIA_OBSERVATION_ASSERT` / `DIA_OBSERVATION_FAIL` primitives the orchestrator will depend on. See `summary.md` "Future: Multi-Scenario E2E Suites" for the future system's expected shape.

The standard `evaluate.md` scoring pass was skipped because the candidates are complementary, not competing: each one covers a distinct part of the design space (session/log infra, traces, metrics, health, transport bridge), and the user's framing was "scale up from simple — give me all four pillars" rather than "pick one pillar to start with." Scoring would have ranked candidates that were all going to ship.

The bundle adds up to a **system**, not a feature: roughly 6–12 weeks of work across one new module (`DiaObservation`) and modifications to two existing ones (`DiaLogger`, `DiaDebugServer`). It will land as a system spec with feature specs underneath, not a single feature spec.

## What Was Ruled Out

| Candidate | Reason not chosen |
|-----------|------------------|
| C7 — Sidecar observation process (L) | Deferred. The async drain in C3 fixes the immediate perf concern in-process. Sidecar buys *crash-survival* of the log stream and zero-engine sink overhead — both real, neither felt yet. Cost (IPC framing, two-process lifecycle, schema versioning over the wire) outweighs benefit at current scale. Reconsider when (a) crash investigation requires log entries past the engine death, or (b) sink I/O measurably blocks the drain thread. |
| C8 — OTLP exporter (M) | Deferred. The user explicitly framed v1 as single-session, not long-term aggregation, but said "prepare for that." That preparation is satisfied by C1's wire format being OTel-shaped from day one (see Pre-Spec Commitments below); the exporter itself is a sink class added later, not a refactor. Reconsider when a real aggregation surface (Grafana, Honeycomb, internal dashboard) exists to talk to. |
| C9 — Auto-reload config (S) | Deferred. v1 reads config at startup; the schema must not foreclose live reload, but the implementation is unjustified until the dev-loop pain is real. Reconsider when developers are repeatedly restarting just to flip log levels. |

## Pre-Spec Commitments

These are constraints carried into the system + feature specs from this research.

### Module structure

- **One module: `Dia/DiaObservation/`.** All four pillars (logs, traces, metrics, health), the session manager, the JSON sinks, and the scenario tag stack live as subsystems inside this module.
- **`DiaLogger` is folded into `Dia/DiaObservation/Log/`.** The existing `DiaLogger` files (`Logger.h/.cpp`, `LogEntry.h`, `ISink.h/.cpp`, `ThreadLogBuffer.h/.cpp`, `StdOutSink.h/.cpp`, `DebugOutputSink.h/.cpp`, `AssertSinkBridge.h/.cpp`, `DiaLog.h`, `LogLevel.h/.cpp`) move into `Dia/DiaObservation/Log/`. `Dia/DiaLogger/` is deleted. All call-site includes (`<DiaLogger/...>` → `<DiaObservation/Log/...>`) are updated. Reasoning: logs are one of the four pillars and the schema/timestamp/session-id coupling is unavoidable — keeping it as a sibling module is asymmetric with the other three pillars and forces consumers to import two modules instead of one.
- `DiaDebugServer` gets the C10 bridge as an additional class, not a new module.
- No `DiaTrace`, `DiaMetrics`, or `DiaHealth` sibling modules in v1. If any pillar needs to ship without the others later, extraction is a 1-day refactor — pre-emptive splitting is the wrong trade for v1.

### Sequencing

The system spec's plan ships features in this order:

1. **DiaLogger fold + C3 async drain.** Step (a) creates `Dia/DiaObservation/` skeleton with a `Log/` subsystem; moves all `DiaLogger` files into it; updates every caller's includes; deletes `Dia/DiaLogger/`; ships the build green with no behaviour change. Step (b) implements the async drain in its new home. Both steps in one feature, possibly two PRs. Lands first to de-risk later work and remove the existing pain point.
2. **C1 + C2 — Foundation.** Bundled. Session directory, observation sink, timestamps, retention ring, scenario tagging, exit-reason capture, crash auto-dump, plus the minimal `.diagame` config block and CLI overrides. The `session.json` schema (below) is fixed in this PR.
3. **C4, C5, C6 — Pillars in parallel.** After foundation lands, traces / metrics / health are genuinely independent (different files, different schemas, no shared headers). Dispatch in parallel as separate feature specs.
4. **C10 — DebugServer WebSocket bridge.** Last. Has nothing to expose until at least one pillar produces records.

### Schema versioning

- Every `session.json` and every record in `log.jsonl` carries `"schema_version": "1.0"`. Semver. Future changes bump major (breaking) or minor (additive).
- This is non-negotiable from v1 — adding versioning retroactively is a multi-week migration when consumers exist.

### Wire format alignment

- Record shapes mirror OpenTelemetry log/span/metric records but **do not depend on the `opentelemetry-cpp` SDK**. Field names match where reasonable (`trace_id`, `span_id`, `parent_span_id`, `start_unix_nano`, `severity_text`, `severity_number`).
- A future OTLP exporter (C8) ships as a single sink class translating internal records to OTLP/HTTP — never as a producer-side refactor.
- Match OTel; do not adopt OTel.

### Two distinct sinks, not one

- The existing console sinks (`StdOutSink`, `DebugOutputSink`) **stay as-is** for human eyeballs. Format remains `[INFO][channel] msg`.
- The new `JsonLineSink` ("observation sink") writes structured JSON-line to `Cluiche/out/<App>/sessions/<id>/log.jsonl` for AI consumption.
- These coexist; they are not configured as alternatives.

### Session directory layout

```
Cluiche/out/<AppName>/sessions/<sessionId>/
  session.json          # binding manifest — AI's entry point
  log.jsonl             # JSON-line log stream (the observation sink's output)
  trace.jsonl           # JSON-line span stream (when C4 ships)
  metric.jsonl          # JSON-line metric snapshots (when C5 ships)
  metrics-final.json    # final counter/gauge values dumped on exit (when C5 ships)
  health.json           # final per-module health rollup on exit (when C6 ships)
  crashes/              # populated only on assert/exception
    <n>.json            # auto-dump: last N log entries + state snapshot
```

`<sessionId>` is generated at session start: `<UTC-timestamp>-<short-uuid>` so directories sort chronologically by name and never collide.

### `session.json` top-level shape (v1)

```json
{
  "schema_version": "1.0",
  "session": {
    "id": "20260517T142336Z-3f4a",
    "app_name": "CluicheTest",
    "build_version": "1.2.3-dev",
    "build_config": "Debug",
    "platform": "x64",
    "started_unix_nano": 1747496616000000000,
    "ended_unix_nano":   1747496789000000000,
    "duration_ms": 173000
  },
  "exit": {
    "reason": "normal_exit",
    "code": 0,
    "stage_at_exit": "DummyStage"
  },
  "counts": {
    "errors":   0,
    "warnings": 2,
    "frames":   10380,
    "scenario_steps_completed": 4
  },
  "scenario_steps": [
    { "step": "init",      "started_unix_nano": ..., "ended_unix_nano": ... },
    { "step": "load",      "started_unix_nano": ..., "ended_unix_nano": ... },
    { "step": "play",      "started_unix_nano": ..., "ended_unix_nano": ... },
    { "step": "shutdown",  "started_unix_nano": ..., "ended_unix_nano": ... }
  ],
  "modules": [
    { "name": "AssetServiceModule",  "status": "OK",       "errors": 0, "warnings": 0 },
    { "name": "DebugServer",         "status": "Degraded", "errors": 0, "warnings": 2, "reason": "WebSocketReconnected" }
  ],
  "retained_warnings_and_errors": [
    { "ts_unix_nano": ..., "level": "warning", "channel": "websocket", "msg": "client disconnected" }
  ],
  "files": [
    { "path": "log.jsonl",          "kind": "log",    "bytes": 45821 },
    { "path": "metric.jsonl",       "kind": "metric", "bytes": 9120 },
    { "path": "metrics-final.json", "kind": "metric", "bytes": 412 },
    { "path": "health.json",        "kind": "health", "bytes": 287 }
  ]
}
```

The `files` array is the AI's directory index — it can read `session.json` and discover everything else without scanning the filesystem.

### `log.jsonl` record shape (v1)

```json
{"schema_version":"1.0","ts_unix_nano":1747496616012345678,"session_id":"20260517T142336Z-3f4a","level":"info","severity_number":9,"channel":"asset","scenario_step":"load","thread_id":12,"msg":"Loaded \"hero.png\" in 12ms"}
```

`level` is human-readable ("info"); `severity_number` is the OTel-compat integer (9 = INFO). Both ship from day one so consumers can pick.

### Config — what v1 ships

`.diagame` gains an optional `observation` block:

```json
"observation": {
  "log": {
    "default_level": "info",
    "channels": { "asset": "trace", "physics": "warning" }
  },
  "sinks": {
    "console":     { "enabled": true },
    "observation": { "enabled": true, "directory": "auto" }
  },
  "scenario_tagging": true
}
```

CLI overrides win over file values:

```
dia run cluichetest --log-level=trace --log-channel=asset
```

The block is shaped so v2 can add `metrics`, `traces`, `health`, `live_reload`, `sidecar` sub-blocks without a breaking change.

### E2E-driven binding decisions

These commitments exist specifically because the terminal goal is E2E testing. The future E2E framework is not in v1 scope, but v1 must not foreclose it. The system spec's binding-decisions table must include every item below.

- **Session directory layout is a public contract.** Not internal to `DiaObservation`. Changes to the layout are breaking changes that bump `schema_version` major. Future E2E orchestration depends on it.
- **Sessions stay flat (`Cluiche/out/<App>/sessions/<id>/`).** A future suite folder (`Cluiche/out/<App>/suites/<id>/`) sits as a sibling and references sessions by id + relative path; suites do not own or nest sessions. Keeps `sessions/` as a uniform corpus regardless of how the run was triggered.
- **Scenario steps with `ended_unix_nano: null` mean "started but never closed."** The orchestrator's `actual_steps == expected_steps` predicate depends on this distinction; documented schema behaviour, not implementation detail.
- **`IHealthReporter` is polled at every exit, including crash.** `SessionManager`'s exception/assert handler polls health one final time before writing `session.json` and `health.json`. A scenario module reporting `Failing` must reach the manifest even if the crash handler runs.
- **`DIA_OBSERVATION_ASSERT(cond, msg)` and `DIA_OBSERVATION_FAIL(msg)` belong to feature C6's API surface.** These are how scenarios report failure without crashing the process. Each writes a `level: "error"` log record *and* flips the harness module's health to `Failing` with a `reason` StringCRC. Existing `Dia::Core::Assert` continues to crash; these new macros do not.

### Out of scope for v1

- The E2E framework itself — `dia e2e` CLI command, suite manifest, scenario subprocess spawning, JUnit emitter, per-scenario summary writer. Ships as its own future system on top of v1.
- GPU markers (PIX/RenderDoc bridge) — comes when render budget bites.
- Multi-dimensional metric labels (`counter("http", method="GET")`) — large API surface, no current need.
- Cross-process span propagation — irrelevant until C7 sidecar exists.
- Sampling — every span and log record is captured; rate-limit via channel level instead.
- Live config reload (C9) — format must not foreclose it, but no implementation in v1.
- Sidecar process (C7) — async drain handles the perf concern; sidecar deferred until crash-survival of log stream is needed.
- OTLP exporter (C8) — wire format is OTel-shaped; exporter ships when an aggregation backend exists to talk to.
- Cross-suite trend analysis, flake detection, snapshot/golden-image testing, network-driven scenarios — future-future-system concerns; not even in the future E2E system's first pass.

### Compliance with binding decisions

| Decision | How v1 honours it |
|----------|-------------------|
| PD-001 StringCRC for IDs | All session, scenario, channel, span-name, metric-name, module-name identifiers are StringCRC. |
| PD-002 ProcessingUnit/Phase/Module | `SessionModule` joins the main PU; pillars hook in as Module subsystems or via `IHealthReporter` on each Module. |
| PD-003 Component-based entities | Mostly orthogonal; per-component metrics is a future v2 question. |
| PD-004 No STL in public APIs | Public API uses `String1024`, `DynamicArrayC`, `Json::Value`. Internal use of `<chrono>`, `<atomic>`, `<source_location>` is fine. |
| PD-005 x64 Windows only | `QueryPerformanceCounter` (via `std::chrono::steady_clock`) for timestamps; no platform abstraction needed. |
| PD-006 .vcxproj source of truth | New `Dia/DiaObservation/DiaObservation.vcxproj` and filters; no per-project overrides of `Directory.Build.props`. |
| PD-007 C++20 | `std::source_location` for span auto-naming; `std::span` for snapshot views. |
| PD-008 Build paths | Inherit from `Directory.Build.props`. |
| PD-009 out/ for non-binary output | `Cluiche/out/<App>/sessions/<id>/` is the canonical location. |
| PD-010 .diagame is project root | Telemetry config slots into `.diagame` `config` block. |

## Next Step

Run `/spec-system` with this research as input.

**Suggested system name:** `DiaObservation`
**Suggested home:** `Dia/DiaObservation/`
**Estimated total size:** L (system) — 7 feature specs underneath, sized as 4×S + 3×M.
**Feature specs the system will need:**
1. `DiaObservation Skeleton + DiaLogger Fold + Async Drain` (M) — folds `Dia/DiaLogger/` into `Dia/DiaObservation/Log/` and implements C3
2. `DiaObservation Foundation` (M) — C1
3. `DiaObservation Config` (S) — C2
4. `DiaTrace Spans` (M) — C4
5. `DiaMetrics Registry` (M) — C5
6. `DiaHealth Reporting` (S) — C6
7. `DebugServer Observation Bridge` (S) — C10

The system spec's binding decisions table cascades the platform decisions (PD-001..PD-010) into observation-specific constraints (schema versioning, two-sinks-not-one, OTel wire alignment without SDK adoption, session-directory layout, sequencing, DiaLogger fold).
