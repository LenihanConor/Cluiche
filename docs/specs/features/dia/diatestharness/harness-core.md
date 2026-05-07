# Feature Spec: Harness Core

## Parent System
@docs/specs/systems/dia/diatestharness.md

## Summary

Provides the complete Python infrastructure for DiaTestHarness: the DiaCLI `dia test e2e` entry point, application launcher, WebSocket client with pluggable observation model, JSON test plan orchestrator with gate/fan-out execution, scenario runner with static + semantic validation, event-driven failure reporting, and structured result output.

## Goals

- Provide a single CLI entry point (`dia test e2e`) that abstracts all harness internals
- Launch Dia applications via DiaCLI and manage their lifecycle
- Connect to DiaDebugServer over WebSocket with state-on-connect
- Execute declarative JSON scenarios with pluggable observation types
- Orchestrate multi-scenario runs via JSON test plans with directory-based discovery
- Report structured results with five distinct failure categories
- Require no engine-side code beyond a minor DiaDebugServer handshake enhancement

## Non-Goals

- Visual regression testing (no screenshot capture)
- Input replay or raw input injection
- Parallel scenario execution
- Performance benchmarking (beyond threshold assertions via observation plugins)
- CI/CD pipeline configuration

## Acceptance Criteria

| # | Criterion | Test |
|---|-----------|------|
| AC1 | `dia test e2e --app=<app>` launches pytest via DiaCLI shim | CLI command invokes pytest with correct args |
| AC2 | `dia test e2e --scenario=<file>` runs a single scenario standalone (no plan required) | Single scenario file executes and reports result |
| AC3 | `dia test e2e --plan=<name>` loads a JSON test plan, executes gate then fan-out by directory | Gate failure skips subsequent scenarios with BLOCKED status |
| AC4 | AppLauncher starts app via `dia run` (build+launch, default) or `dia launch` (skip build), polls process liveness alongside WebSocket backoff | Process crash during startup reported immediately, not after timeout |
| AC5 | DiaTestClient connects on non-default port, receives current state snapshot on handshake | Client knows current phase without waiting for transition event |
| AC6 | Port conflict detected and reported with clear error, no silent failures | Specific error message naming the conflict |
| AC7 | Observation types are pluggable: subscribe / snapshot / unsubscribe contract | New observation type addable without modifying harness core |
| AC8 | On any terminal state, harness calls `snapshot()` on all active observations | Failure reports include state from all active observers |
| AC9 | Static JSON schema validation before launch rejects malformed scenarios | Invalid scenario reports ERROR with specific validation failure |
| AC10 | Semantic preflight validation after connect checks phase names, subscription types, commands | Typo in phase name fails at preflight, not after 10s timeout |
| AC11 | Teardown ladder: quit command, then terminate, then kill | Orphaned processes cannot accumulate |
| AC12 | Results output to `Cluiche/out/<AppName>/test-results/` as JSON + summary | Results directory populated after every run |
| AC13 | Pre-connection crash reports exit code only (no stdout/stderr capture) | CRASH result includes exit code, no pipe infrastructure |
| AC14 | Plans reference directories; new scenario files auto-discovered | Adding a JSON file to a plan directory includes it on next run |

## Tasks

| # | Task | AC | Description |
|---|------|----|-------------|
| 1 | DiaCLI shim | AC1, AC2, AC3 | Add `dia test e2e` Click command under `cli/test_e2e.py` with `--app`, `--scenario`, `--plan`, `--port` flags |
| 2 | AppLauncher | AC4, AC13 | Python class: `dia launch` subprocess, process polling, exit code capture, liveness check |
| 3 | DiaTestClient | AC5, AC6 | Async WebSocket client: connect with backoff, handshake with state snapshot, non-default port, conflict detection |
| 4 | DiaDebugServer handshake change | AC5 | Include current phase + PU state in handshake response (minor C++ change) |
| 5 | Observation contract | AC7, AC8 | Base class/protocol: `subscribe()`, `snapshot()`, `unsubscribe()`. Registry for active observations. Snapshot-all on terminal state |
| 6 | ScenarioRunner | AC9, AC10, AC11 | Step executor: schema validation, semantic preflight, step dispatch, teardown ladder |
| 7 | Test Plan Loader | AC3, AC14 | Parse test plan JSON, resolve directories to scenario files, execute gate → fan-out |
| 8 | Result Reporter | AC12 | Write JSON results + text summary to `Cluiche/out/<AppName>/test-results/` |
| 9 | Integration test | All | Smoke-level integration: launch app, connect, run trivial scenario, verify result output |

## Data Models

### Test Plan Schema

```json
{
  "name": "cluichetest_full",
  "description": "Full e2e suite for CluicheTest",
  "app": "cluichetest",
  "port": 8090,
  "stages": [
    {
      "gate": "smoke/",
      "on_pass": [
        "stages/stage_a/",
        "stages/stage_b/"
      ]
    }
  ]
}
```

### Observation Protocol

```python
class Observation(Protocol):
    async def subscribe(self, client: DiaTestClient) -> None: ...
    async def snapshot(self) -> dict: ...
    async def unsubscribe(self, client: DiaTestClient) -> None: ...
```

### Scenario Result

```python
@dataclass
class ScenarioResult:
    name: str
    status: Literal["PASS", "FAIL", "TIMEOUT", "CRASH", "ERROR"]
    duration_seconds: float
    failed_step: int | None
    observations: dict          # snapshot from all active observations
    exit_code: int | None       # only for CRASH
    error_message: str | None   # for ERROR / FAIL / TIMEOUT
```

### CLI Interface

```bash
dia test e2e --app=cluichetest                          # discover plan or run all scenarios
dia test e2e --app=cluichetest --scenario=smoke.json    # standalone
dia test e2e --app=cluichetest --plan=full              # explicit plan
dia test e2e --app=cluichetest --port=8090              # override port
```

## Directory Structure

```
Tools/e2e/
  conftest.py              # pytest fixtures
  test_scenarios.py        # pytest entry (parametrize over scenarios)
  dia_test_client.py       # DiaTestClient (WebSocket)
  scenario_runner.py       # ScenarioRunner (step execution + validation)
  app_launcher.py          # AppLauncher (process management)
  observations/
    __init__.py            # Observation protocol + registry
    core_metrics.py        # Example: FPS/frame time observation
    state.py               # Example: phase/module state observation
  plan_loader.py           # Test plan parser + directory resolver
  result_reporter.py       # JSON + summary output
  schema/
    scenario.schema.json   # JSON Schema for scenario validation
    plan.schema.json       # JSON Schema for plan validation
  scenarios/
    cluichetest/
      smoke/
        smoke_test.json
      stages/
        ...
```

## Files Modified

| File | Change |
|------|--------|
| `Dia/DiaCLI/dia_cli/cli/test_e2e.py` | New: DiaCLI shim command |
| `Tools/e2e/` (all files above) | New: entire harness Python package |
| `Dia/DiaDebugServer/DebugServerModule.cpp` | Modify: include current state in handshake response |
| `Dia/DiaDebugProtocol/` | Modify: handshake response includes phase/state fields (if not already) |

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaTestHarness | @docs/specs/systems/dia/diatestharness.md |
| Feature | Harness Core | (this spec) |

## Binding Decisions Compliance

### Platform Decisions (PD-)

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for all entity/component IDs | N/A — harness is external Python, does not create engine entities |
| PD-002 | ProcessingUnit/Phase/Module architecture | N/A — harness is external; consumes PU state via DiaDebugServer, does not define PUs |
| PD-003 | Component-based entities | N/A — harness is external Python |
| PD-004 | No STL in public APIs | N/A — harness is external Python. DiaDebugServer handshake change uses existing protocol serialization (jsoncpp) |
| PD-005 | x64 only | Compliant — AppLauncher launches x64 apps via DiaCLI |
| PD-006 | Visual Studio project files are source of truth | Compliant — no vcxproj changes needed. DiaDebugServer .cpp change is within existing project |
| PD-007 | C++20 required | Compliant — DiaDebugServer handshake change compiles under C++20 |
| PD-008 | Directory.Build.props owns output paths | Compliant — no build path changes |
| PD-009 | Generated output under `Cluiche/out/<AppName>/` | Compliant — results written to `Cluiche/out/<AppName>/test-results/` |
| PD-010 | `.diagame` is project root file | Compliant — harness uses DiaCLI which resolves via `.diagame` |

### Application Decisions (AD-)

| ID | Decision | Compliance |
|----|----------|------------|
| AD-001 | Module system with YAML frontmatter | N/A — harness is external Python, not a Dia module |
| AD-002 | No STL in public APIs | N/A — Python code. DiaDebugServer change is internal implementation |
| AD-003 | Namespace convention `Dia::<Module>::` | N/A — Python code |
| AD-004 | PU/Phase/Module for app structure | N/A — harness doesn't define application structure |
| AD-005 | Component-based entities | N/A — harness doesn't create components |

### System Decisions (DTH-)

| ID | Decision | Compliance |
|----|----------|------------|
| DTH-001 | Harness is pure Python — no engine-side code | Compliant — all Python except minor DiaDebugServer handshake enhancement (existing module, not new engine code) |
| DTH-002 | JSON scenario format | Compliant — scenarios and plans are JSON |
| DTH-003 | Minimal deps: stdlib + pytest + websockets | Compliant — no additional dependencies |
| DTH-004 | Results to `Cluiche/out/<AppName>/test-results/` | Compliant — ResultReporter writes there |
| DTH-005 | Smoke test is first scenario | Compliant — plan gate model ensures smoke runs first |
| DTH-006 | Scenarios are declarative JSON | Compliant — scenarios are flat declarative steps. Orchestration logic lives in plan loader, not scenarios |
| DTH-007 | Fresh app per scenario | Compliant — AppLauncher creates new process per scenario; ScenarioRunner owns full lifecycle |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Port | What specific non-default port should be the harness default? | 8090. Distinct from editor default (8080), easy to remember the relationship |
| 2 | Plan discovery | If `--plan` is not specified but a plan file exists in the app's scenario directory, should it auto-discover? | Yes — auto-discover plan file in app scenario dir. Explicit `--plan` overrides. No plan = run all scenarios flat |
| 3 | Handshake | Does the current DiaDebugProtocol HandshakeResponse already have fields for phase/state, or do we need to add them? | Needs new fields. Current HandshakeResponse only has protocol_version, accepted, server_name, server_version. Add: current_phase, available_phases, available_commands |
| 4 | Semantic preflight | How does the harness learn valid phase names and commands — from the handshake topology, or a separate query? | From the handshake response itself. New fields (available_phases, available_commands) provide everything needed in one round trip |
| 5 | Teardown timing | What timeout for each teardown step (quit command wait → terminate wait → kill)? | 10s quit command wait → 5s after terminate → immediate kill |
| 6 | Observation defaults | Should harness auto-subscribe to state observation by default (for failure context), or only what the scenario requests? | Auto-subscribe to state observation always. Ensures failure reports have phase/module context regardless of scenario contents |
| 7 | DTH-001 tension | The DiaDebugServer handshake change is C++ engine code. Does this violate DTH-001, or is "enhancement to existing module" acceptable? | Acceptable — adding fields to an existing handshake in an existing module is not "introducing engine-side code for testing." Benefits editors too |
| 8 | Result format | Should the JSON result file be one file per scenario, or one file per run containing all scenarios? | One file per scenario — individual files, no clobbering, better for incremental runs |

## Open Questions

None — all architectural questions resolved in design session.

## Status

`Approved`
