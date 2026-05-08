# Feature Spec: cli-output

## Parent System
@docs/specs/systems/dia/diacli.md

## Status
`Done`

## Summary

Implement a shared output and observability layer for all DiaCLI commands. Provides structured terminal output (via `rich`) and a newline-delimited JSON (NDJSON) event log written to `Cluiche/out/DiaCLI/logs/<system>/last-run.ndjson`. All three systems (DiaEnv, DiaPipeline, DiaTest) emit events through this layer. The JSON log is the stable contract for the future CluicheEditor pipeline viewer — the editor watches the file and renders pipeline state live without any coupling to the CLI process.

## Problem

DiaCLI currently has no consistent output format — commands use a mix of `loguru` log lines, `print()`, and raw subprocess output. There is no way for the editor (or any external tool) to observe pipeline progress in real time. Each command reinvents its own progress reporting, and there is no machine-readable record of what ran, when, and whether it succeeded.

## Goals

- Single shared output module (`dia_cli/utils/dia_output.py`) used by all DiaCLI commands
- Streaming log-style terminal output using `rich` — timestamped, colour-coded, structured
- Every event written as a JSON line to `Cluiche/out/DiaCLI/logs/<system>/last-run.ndjson` in real time (as it fires, not buffered)
- Fixed log paths so CluicheEditor always knows where to look; `--log-json <path>` overrides the default
- `--no-color` flag disables ANSI colour codes for CI/piped contexts
- `--quiet` flag suppresses terminal output but still writes the JSON log
- Consistent event lifecycle: every unit of work emits `OnXStarted` / `OnXCompleted` or `OnXFailed`

## Non-Goals

- A TUI or interactive dashboard — streaming log only (Option B)
- CluicheEditor integration itself — the editor watches the file; this feature only writes it
- Persisting historical runs — `last-run.ndjson` is overwritten on each run; history is a future concern
- Replacing `loguru` entirely — `loguru` continues to be used for debug/internal logging; `dia_output` is for user-facing structured events

## Terminal Output Format

Streaming log style — appends downward, never updates in place. Works in Docker, CI, and piped contexts.

```
[12:04:01] ▶ proto-compile
[12:04:01]   protoc debug_protocol.proto ... done (0.8s)
[12:04:01] ✓ proto-compile  0.8s
[12:04:02] ▶ compile-code
[12:04:02]   msbuild GoogleTests.vcxproj ...
[12:04:06]   msbuild GoogleTests.vcxproj ... done (4.2s)
[12:04:06] ✓ compile-code  4.2s
[12:04:06] ▶ package
[12:04:06]   copy python311.dll → Cluiche/bin/Debug/x64/
[12:04:06]   copy *.diaapp → Cluiche/bin/Debug/x64/Data/Manifests/
[12:04:06] ✓ package  0.1s
[12:04:06] ─────────────────────────────────────────────
[12:04:06] ✓ pipeline complete  3 passed · 0 failed · 5.1s
```

**Colour coding:**
- `▶` stage/step started — white
- `✓` completed — green
- `✗` failed — red
- `⚠` warning — yellow
- `○` skipped — dim

## NDJSON Event Schema

One JSON object per line, written to `Cluiche/out/DiaCLI/logs/<system>/last-run.ndjson` as each event fires (e.g. `pipeline/last-run.ndjson`, `env/last-run.ndjson`, `test/last-run.ndjson`).

### Event types and fields

```json
{"event": "OnRunStarted",     "schema": "dia.output.v1", "system": "pipeline", "target": "googletest", "config": "Debug", "stages": ["proto-compile", "compile-code", "package"], "ts": 1714123456.000}
{"event": "OnStageStarted",   "system": "pipeline", "stage": "compile-code", "ts": 1714123456.100}
{"event": "OnStepStarted",    "system": "pipeline", "stage": "compile-code", "step": "msbuild", "detail": "GoogleTests.vcxproj", "ts": 1714123456.200}
{"event": "OnStepCompleted",  "system": "pipeline", "stage": "compile-code", "step": "msbuild", "durationMs": 4200, "ts": 1714123460.400}
{"event": "OnStepFailed",     "system": "pipeline", "stage": "compile-code", "step": "msbuild", "error": "msbuild exited 1", "durationMs": 4200, "ts": 1714123460.400}
{"event": "OnStageCompleted", "system": "pipeline", "stage": "compile-code", "durationMs": 4250, "ts": 1714123460.450}
{"event": "OnStageFailed",    "system": "pipeline", "stage": "compile-code", "error": "msbuild exited 1", "durationMs": 4250, "ts": 1714123460.450}
{"event": "OnRunCompleted",   "system": "pipeline", "passCount": 3, "failCount": 0, "totalDurationMs": 5100, "ts": 1714123461.100}
{"event": "OnRunFailed",      "system": "pipeline", "passCount": 2, "failCount": 1, "totalDurationMs": 5100, "ts": 1714123461.100}
{"event": "OnLogLine",        "system": "pipeline", "stage": "compile-code", "level": "info", "message": "Building GoogleTests.vcxproj...", "ts": 1714123456.300}
{"event": "OnLogLine",        "system": "env",      "stage": "deps",         "level": "warn", "message": "SHA-256 mismatch for sfml — retrying", "ts": 1714123456.300}
```

### Lifecycle contract

Every unit of work emits a matched pair:

```
OnRunStarted
  OnStageStarted
    OnStepStarted
    OnStepCompleted | OnStepFailed
  OnStageCompleted | OnStageFailed
OnRunCompleted | OnRunFailed
```

The editor tracks unmatched `Started` events to determine what is currently in progress. A `Started` with no matching `Completed`/`Failed` means the process crashed — the editor should show this as "interrupted".

### Common fields

| Field | Type | Present on | Description |
|-------|------|-----------|-------------|
| `event` | string | all | Event type name |
| `system` | string | all | `"pipeline"` \| `"env"` \| `"test"` |
| `stage` | string | stage/step events | Stage name (e.g. `"compile-code"`) |
| `step` | string | step events | Step name (e.g. `"msbuild"`) |
| `ts` | float | all | Unix timestamp (seconds, fractional) |
| `durationMs` | int | Completed/Failed | Elapsed time in milliseconds |
| `error` | string | Failed events | Human-readable error description |
| `level` | string | OnLogLine | `"info"` \| `"warn"` \| `"error"` \| `"debug"` |
| `message` | string | OnLogLine | Log message text |

## Python API

All DiaCLI commands emit events through a single shared context object:

```python
from dia_cli.utils.dia_output import OutputContext

# Obtained from Click context — initialised once per CLI invocation
ctx: OutputContext

# Emit events
ctx.run_started(system="pipeline", target="googletest", config="Debug", stages=[...])
ctx.stage_started(system="pipeline", stage="compile-code")
ctx.step_started(system="pipeline", stage="compile-code", step="msbuild", detail="GoogleTests.vcxproj")
ctx.step_completed(system="pipeline", stage="compile-code", step="msbuild")
ctx.step_failed(system="pipeline", stage="compile-code", step="msbuild", error="msbuild exited 1")
ctx.stage_completed(system="pipeline", stage="compile-code")
ctx.stage_failed(system="pipeline", stage="compile-code", error="msbuild exited 1")
ctx.run_completed(system="pipeline", pass_count=3, fail_count=0)
ctx.log(system="pipeline", stage="compile-code", level="info", message="Building...")
```

`OutputContext` handles both terminal rendering and JSON file writing internally. Callers never interact with `rich` or the file directly.

## Implementation

### Files introduced

```
Dia/DiaCLI/
└── dia_cli/
    └── utils/
        └── dia_output.py        # OutputContext class — terminal + NDJSON writer
```

### Files modified

```
Dia/DiaCLI/
└── dia_cli/
    └── cli_main.py              # Initialise OutputContext; attach to Click context obj
```

### `dia_output.py` responsibilities

- `OutputContext.__init__(log_path, no_color, quiet)` — open log file, configure `rich.Console`
- One method per event type (see Python API above) — each method:
  1. Renders to terminal via `rich` (unless `--quiet`)
  2. Writes JSON line to log file immediately (always, even in `--quiet` mode)
- `_write_event(payload: dict)` — serialise to JSON, append newline, flush immediately
- `_render_terminal(event: dict)` — map event type to coloured `rich` output line
- Log files written to `Cluiche/out/DiaCLI/logs/<system>/last-run.ndjson` (e.g. `pipeline/last-run.ndjson`, `env/last-run.ndjson`, `test/last-run.ndjson`)
- Log file opened in write mode at `OnRunStarted` (truncates previous run; always contains exactly one run)

### Integration with `cli_main.py`

`OutputContext` is created once per CLI invocation in `cli_main.py` and attached to the Click context object. All commands access it via `click.get_current_context().obj.output`.

Global flags added to the top-level `dia` command:
- `--log-json <path>` — override log file path (default: `Cluiche/out/DiaCLI/logs/<system>/last-run.ndjson`)
- `--no-color` — disable ANSI colour codes
- `--quiet` — suppress terminal output; still writes JSON log

## Dependencies

| Dependency | Type | Notes |
|------------|------|-------|
| `rich` | Python | Add to `pyproject.toml`; terminal rendering |
| `loguru` | Python | Existing; continues for internal/debug logging |

## Acceptance Criteria

1. `dia pipeline --stage compile-code --target googletest` prints timestamped, colour-coded streaming output matching the terminal format above
2. `Cluiche/out/DiaCLI/logs/<system>/last-run.ndjson` is created/overwritten at the start of each run
3. Each event is written to the NDJSON file immediately as it fires — not buffered until completion
4. `OnRunStarted` is always the first line; `OnRunCompleted` or `OnRunFailed` is always the last line
5. Every `OnXStarted` event has a matching `OnXCompleted` or `OnXFailed` in a successful run
6. `--no-color` produces plain text output with no ANSI escape codes
7. `--quiet` suppresses all terminal output but still writes the NDJSON log
8. `--log-json <path>` writes the log to the specified path instead of `Cluiche/out/DiaCLI/logs/<system>/last-run.ndjson`
9. `dia env setup`, `dia test googletest`, and `dia test cli` all use `OutputContext` and produce consistent terminal output and NDJSON events
10. A simulated crash (process killed mid-run) leaves the NDJSON file with an unmatched `OnStageStarted` — the editor can detect this as "interrupted"

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaCLI | @docs/specs/systems/dia/diacli.md |

## Binding Decisions Compliance

| ID | Source | Decision | Compliance |
|----|--------|----------|------------|
| PD-001 | Platform | Use StringCRC for all identifiers | Compliant — Python tooling only; no C++ identifiers |
| PD-002 | Platform | ProcessingUnit/Phase/Module architecture | Compliant — tooling; no runtime components |
| PD-003 | Platform | Component-based entities | Compliant — tooling |
| PD-004 | Platform | No STL containers in public APIs | Compliant — Python only |
| PD-005 | Platform | x64 Windows only | Compliant — no platform-specific output logic; `rich` is cross-platform but Cluiche is Windows-only |
| PD-006 | Platform | VS project files are source of truth | Compliant — no `.vcxproj` files touched |
| PD-007 | Platform | C++20 required | Compliant — tooling; no compiler configuration |
| PD-008 | Platform | `Directory.Build.props` owns OutDir/IntDir | Compliant — log path is `Cluiche/out/DiaCLI/logs/`, not `OutDir` |
| PD-009 | Platform | All generated non-binary output under `Cluiche/out/<AppName>/` | Compliant — NDJSON logs written to `Cluiche/out/DiaCLI/logs/<system>/last-run.ndjson` |
| AD-001 | Dia App | Module system with YAML frontmatter | Compliant — Python tooling only |
| SD-CLI-001 | DiaCLI | MDK CLI architecture | Compliant — `dia_output.py` is a shared utility; no new plugin pattern needed |
| SD-CLI-002 | DiaCLI | Python-based implementation | Compliant |
| SD-CLI-006 | DiaCLI | Click framework | Compliant — global flags added to top-level `dia` Click command |
| SD-CLI-008 | DiaCLI | Exit codes follow Unix conventions | Compliant — `OutputContext` handles rendering only; exit codes are still owned by each command |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Log path | Where should log files live? | `Cluiche/out/DiaCLI/logs/<system>/last-run.ndjson` per PD-009. `Cluiche/out/` is the platform-wide root for all generated non-binary output, parallel to `Cluiche/bin/`. Each DiaCLI system (pipeline, env, test) gets its own subdirectory. `OutputContext` resolves the repo root the same way `cli_main.py` does (walking up from cwd to find `dia_cli_prime_config.json`). |
| 2 | `rich` version | Which version of `rich` should be pinned in `pyproject.toml`? | `rich >= 13.0` — stable API, wide adoption, supports Python 3.11. |
| 3 | CluicheEditor contract | Is the NDJSON schema stable enough to be treated as a versioned contract now? | Yes — add `"schema": "dia.output.v1"` to `OnRunStarted`. Future breaking changes bump to `v2`. The editor checks this field before parsing. This protects against silent schema drift as the CLI evolves. |
| 4 | Thread safety | If a future command emits events from multiple threads, is `OutputContext` thread-safe? | Yes — `OutputContext` uses a `threading.Lock` around `_write_event` and `_render_terminal`. Low cost; prevents corrupt NDJSON if parallelism is introduced later. Included in the implementation. |
