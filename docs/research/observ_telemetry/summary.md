# Research Summary — Engine Observability / Telemetry

**Session folder:** docs/research/observ_telemetry/
**Date:** 2026-05-17

## One-Line Answer

Build a `DiaObservation` system that captures all four pillars (logs, traces, metrics, health) into a per-session directory under `Cluiche/out/<App>/sessions/<id>/`, with a binding `session.json` manifest as the AI's entry point — wire-compatible with OpenTelemetry but with no SDK dependency, shippable as a bundle of one perf fix + one foundation + three pillars + one live-stream bridge — and **explicitly designed as the substrate for E2E testing**: the session directory is the contract a future test orchestrator consumes, and "did this run pass?" becomes a one-file read.

## Terminal Goal: E2E Testing

This research's terminal goal is **E2E testing infrastructure**, not observability for its own sake. Observability is the substrate; the test framework is the consumer that justifies the work.

Today's signal of "did the engine work?" is `exit_code == 0` plus stdout grep — neither expressive enough nor deterministic enough for AI-driven testing or for catching the failure modes that matter (degraded modules, missed scenario steps, warning floors, partial completion). The `DiaObservation` system gives a future E2E framework five things it cannot get any other way:

1. **A binding session manifest** (`session.json`) — one file an orchestrator reads to compute pass/fail without parsing logs.
2. **Scenario tagging** — every log/trace/metric record stamped with the scenario step active when it was produced; orchestrator filters records by step rather than by line range.
3. **Health reporting** — modules report `OK / Degraded / Failing` with a reason; "the asset module silently degraded but the test still finished" becomes a detectable failure.
4. **Crash artifacts** — when an assert fires, the session directory still closes out cleanly with an `exit.reason: "assert"`, last-N log entries, open-spans-at-crash, and a self-contained `crashes/0.json`. Tests can distinguish "asserted at frame 8200 in Solver.cpp" from "exit code non-zero".
5. **Stable, parseable artifacts** — JSON-line format with `schema_version`, monotonic timestamps, OTel-shaped field names. AI agents can ingest a directory and reason about it without writing fragile parsers.

The v1 bundle (C1+C2+C3+C4+C5+C6+C10) **does not include** the E2E framework itself — that ships as a separate later system (`DiaE2E` or similar). What v1 does is make that future system small: once observation is in place, an orchestrator becomes ~200 lines of file reading, manifest assembly, and JUnit emission. See "Future: Multi-Scenario E2E Suites" below.

## Journey

1. **Explored:** Mapped logs/traces/metrics/health as four distinct pillars with separate cardinality and lifetime profiles. Found that `DiaLogger` is solid on the producer side (thread-local rings, no mutex) but the consumer drains synchronously on the main PU and the default sinks block on `printf`/`OutputDebugStringA` per entry — confirming the user's perf intuition. Identified 10 AI-consumption quick wins (JSON-line sink, session ID, stable timestamps, crash auto-dump, session summary, CLI overrides, scenario tagging, retention ring, exit-reason capture, metrics dump) that the user accepted and that became baseline-required for any chosen candidate.
2. **Ideated:** 10 candidates across 3 groups — Foundation (C1 session directory, C2 config, C3 async drain), Pillars (C4 traces, C5 metrics, C6 health), Scaling (C7 sidecar, C8 OTLP exporter, C9 live reload, C10 WebSocket bridge). Sized S–L; XL deliberately avoided. Coverage map verified the list spans every design axis from explore.
3. **Evaluated:** Standard scoring pass skipped — the user's framing was "give me all four pillars from day one, simple-but-scalable", so the candidates were complementary, not competing. Scoring would have ranked candidates that were all going to ship.
4. **Chose:** Bundle of C1 + C2 + C3 + C4 + C5 + C6 + C10 — Group A foundation, Group B pillars, plus the live WebSocket bridge. C7 sidecar / C8 OTLP / C9 live reload deferred with explicit re-evaluation triggers.

## Chosen Work Item

**Name:** `DiaObservation` system
**Home module:** new `Dia/DiaObservation/` — one module covering all four pillars. **`DiaLogger` is folded into `Dia/DiaObservation/Log/` as part of feature #1.** Modifications also to `DiaDebugServer` (subscription bridge).
**Suggested spec type:** **System** — 7 feature specs underneath
**Estimated size:** L (~6–12 weeks across the bundle)

Feature specs the system will produce:
1. `DiaObservation Skeleton + DiaLogger Fold + Async Drain` (M) — ships first; folds `DiaLogger` into the new module and lands the async drain
2. `DiaObservation Foundation` (M) — C1
3. `DiaObservation Config` (S) — C2
4. `DiaTrace Spans` (M) — C4
5. `DiaMetrics Registry` (M) — C5
6. `DiaHealth Reporting` (S) — C6
7. `DebugServer Observation Bridge` (S) — C10

## Key Insights from Exploration

- **AI consumers want different things than humans.** Human eyeballs want flamegraphs and dashboards; AI agents want deterministic JSON, complete artifacts on crash, per-run identity, stable timestamps, and a single-line summary file. The "session = a directory" model with `session.json` as the binding manifest is the AI-first answer.
- **Two distinct sinks, not one.** The existing console sinks (`StdOutSink`, `DebugOutputSink`) stay human-readable; a *new* `JsonLineSink` is added alongside for AI. They coexist; they are not configured as alternatives.
- **The logger perf hit is the *consumer* side, not the transport.** `Logger::Log` is genuinely cheap (thread-local rings, no mutex). The cost is `FlushBuffers` running synchronously on the main PU plus `printf`+`fflush` and `OutputDebugStringA` per entry. The cheap fix is "drain off the main thread" (C3) — sidecar (C7) is unjustified at current scale.
- **"Trace" means span-based tracing in Cluiche.** The "verbose mode for the asset system" sense of trace is a *log feature* (existing channel-level filter), not a separate system. `DIA_TRACE_ZONE("Render::Frame")` is the primitive.
- **OpenTelemetry wire-format alignment without SDK adoption.** Match the field names (`trace_id`, `span_id`, `parent_span_id`, `start_unix_nano`, `severity_text`, `severity_number`); do not pull `opentelemetry-cpp`. A future OTLP exporter (C8) is one sink class translating internal records to OTLP/HTTP — never a producer-side refactor.
- **Schema versioning is non-negotiable from v1.** Every record carries `"schema_version": "1.0"`. Adding versioning retroactively is a multi-week migration once consumers exist; doing it now is two characters in two places.
- **Health = "more than exit code 0/1".** Pull-based `IHealthReporter` per Module + session-level rollup (errors, warnings, scenario steps completed, per-module status, exit reason) — answers E2E test predicates without parsing every log line.
- **Config is in v1 but minimal and forward-compatible.** Smallest surface that lets users change log levels without recompiling, slotted into `.diagame`, with CLI overrides. Format must not foreclose live reload, even though v1 only does startup reload — the user is doing a separate cross-system config pass later.
- **One module, not four — and `DiaLogger` is folded in.** All four pillars live as subsystems inside `DiaObservation` (`Log/`, `Trace/`, `Metric/`, `Health/`). They share too much plumbing (session ID, timestamps, JSON sink, scenario tags, schema) to justify splitting. The existing `Dia/DiaLogger/` module is moved into `Dia/DiaObservation/Log/` in feature #1 — keeping it as a sibling would be asymmetric with the other three pillars and force consumers to import two modules. Pre-emptive splitting is the wrong trade for v1; if any pillar ever needs to ship without the others, extraction is a 1-day refactor.

## Discarded Candidates

| Candidate | Why discarded |
|-----------|--------------|
| C7 — Sidecar observation process (L) | Deferred. C3 async drain solves the immediate perf concern in-process. Sidecar buys crash-survival of the log stream and zero engine sink overhead — neither felt yet. Reconsider when crash investigation requires log entries past engine death, or sink I/O measurably blocks the drain thread. |
| C8 — OTLP exporter (M) | Deferred. v1 is single-session, not long-term aggregation. Wire format is OTel-shaped from day one (C1 commitment), so the exporter is a sink class added later — never a refactor. Reconsider when an aggregation backend (Grafana, Honeycomb, Tempo, internal dashboard) exists to talk to. |
| C9 — Auto-reload config (S) | Deferred. v1 reads config at startup; format must not foreclose live reload but implementation is unjustified until dev-loop pain is real. Reconsider when developers are repeatedly restarting just to flip log levels. |

## Pre-Spec Commitments (carried into the system spec)

- One `Dia/DiaObservation/` module; pillars are subsystems.
- Sequencing: C3 → C1+C2 → C4/C5/C6 in parallel → C10.
- `schema_version: "1.0"` on every record from day one.
- OTel wire-format alignment, no SDK adoption.
- Two distinct sinks coexist (human console + AI JSON-line).
- Session directory layout and `session.json` shape are fixed in C1.
- Out of scope for v1: GPU markers, multi-dimensional metric labels, cross-process span propagation, sampling, live config reload, sidecar process, OTLP exporter.

## Future: Multi-Scenario E2E Suites

The terminal goal of this work is E2E testing. The v1 observation bundle does not ship the test framework itself — but it commits to a session-directory layout and a `session.json` schema **specifically chosen to make a future E2E framework cheap**. Recording the design intent here so the system spec can reference it and so the future spec inherits the layout decisions verbatim.

### Layout (sibling to `sessions/`, not nested)

```
Cluiche/out/<AppName>/
├── sessions/                                   # one folder per engine run (existing, v1)
│   ├── <session-id>/
│   │   └── session.json, log.jsonl, ...
│   └── ...
└── suites/                                     # NEW — one folder per suite execution (future)
    └── <suite-id>/
        ├── suite.json                          # top-level manifest for the whole suite
        ├── scenarios/<name>.json               # per-scenario slim summary
        └── junit.xml                           # standard JUnit XML for CI tools
```

Sessions stay flat. A suite is metadata about a group of sessions — it *references* sessions by id and relative path, never owns them. A developer running `dia run cluichetest` produces a session in the same shape the suite would.

### `suite.json` shape (preview, for the future spec to lock in)

```json
{
  "schema_version": "1.0",
  "suite": { "id": "...", "name": "smoke", "git_sha": "...", "duration_ms": 173000 },
  "result": { "status": "Failed", "passed": 10, "failed": 1, "crashed": 1, "total": 12 },
  "scenarios": [
    {
      "name": "asset_load",
      "status": "Passed",
      "session_id": "...",
      "session_dir": "../../sessions/.../",
      "summary":     "scenarios/asset_load.json"
    },
    {
      "name": "missing_asset_recovery",
      "status": "Failed",
      "session_id": "...",
      "session_dir": "../../sessions/.../",
      "failure": { "module": "...", "reason": "load timed out after 5s", "step_reached": "load_hero_png" }
    },
    {
      "name": "physics_settle",
      "status": "Crashed",
      "crash":  { "trigger": "assert", "file": "...", "line": 284, "crash_dump": "../../sessions/.../crashes/0.json" }
    }
  ]
}
```

Inline failure/crash summaries at the top level so AI / CI tooling triages without drilling into per-scenario files.

### How v1 enables this with no v1 work

The future E2E framework will need:

| Need | Provided by v1 feature |
|------|------------------------|
| One artifact per run with binding identity | C1 — session directory + `session.json` |
| Per-step record stamping | C1 — scenario tag stack |
| "Did module X degrade silently?" | C6 — `IHealthReporter` + session rollup |
| "Where did we crash, mid-which step?" | C1 — crash auto-dump + exit-reason capture |
| "Did frame budget hold across the scenario?" | C4/C5 — span and metric records filtered by scenario step |
| Live observation while a long scenario runs | C10 — DebugServer subscription bridge |

Every E2E need maps to a v1 feature. The future system's scope is the orchestration: a CLI command (`dia e2e --suite=smoke`), a per-suite manifest writer, scenario subprocess spawning, and the JUnit emitter. None of that touches the engine; all of it reads from `Cluiche/out/<App>/sessions/`. Estimated future system size: **M** — small precisely because v1 did the schema work.

### What this implies for the v1 spec

The v1 system spec must explicitly call out:

- **The session-directory layout is a public contract**, not internal to `DiaObservation`. Future E2E orchestration depends on it. Changes to the layout are breaking changes that bump `schema_version` major.
- **Scenario tag step boundaries (started/ended timestamps, `null` for unfinished steps) are part of the contract.** The orchestrator's pass predicate depends on `actual_steps == expected_steps`; how unfinished steps appear must be documented.
- **`IHealthReporter` polling order matters at exit and crash.** Health must be polled before `session.json` is finalised, including in the crash handler. A scenario whose harness module reports `Failing` only via a missing health poll would silently pass.
- **`DIA_OBSERVATION_ASSERT` and `DIA_OBSERVATION_FAIL`** primitives — the macros scenarios use to report failure without crashing the process — belong in feature C6's surface. They write a `level: "error"` log record *and* flip the harness module's health to `Failing`. This is a small but explicit API addition that the future E2E system will depend on.

Capturing these as binding decisions in the system spec (Step 3 of the spec process) is what guarantees the v1 work doesn't accidentally close off the E2E goal.

### Out of scope even for the future E2E system

- Cross-suite trend analysis, flake detection, historical dashboards. Belongs in tooling outside the engine; possible once OTLP exporter (C8) ships.
- Snapshot / golden-image testing. Different problem; could share the suite layout but is its own design.
- Network-driven scenarios (multiplayer, server). Out of scope until networking has its own observability story.

## References

- docs/research/observ_telemetry/explore.md
- docs/research/observ_telemetry/ideate.md
- docs/research/observ_telemetry/choose.md

(Note: `evaluate.md` was intentionally not produced — see `choose.md` for the rationale.)
