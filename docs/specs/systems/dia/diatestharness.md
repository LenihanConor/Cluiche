# System Spec: DiaTestHarness

## Parent Application
@docs/specs/applications/dia.md

## Purpose

DiaTestHarness is an external Python/pytest orchestration system that drives end-to-end testing of any Dia application. It launches applications via DiaCLI, connects to them over WebSocket (DiaDebugServer), executes JSON-defined test scenarios, asserts on application state/metrics/logs, and produces structured test results. The harness lives entirely outside the engine — it consumes existing DiaDebugServer and DiaCLI interfaces without introducing engine-side code.

## Responsibilities

- **Scenario Definition** - Define e2e test scenarios as JSON files (inputs, expected state, assertions)
- **Application Lifecycle** - Launch target applications via DiaCLI, manage process lifetime, detect crashes
- **WebSocket Client** - Connect to DiaDebugServer, subscribe to data, receive metrics/events/logs
- **Assertion Framework** - Assert on application state (PU/phase/module), metrics (FPS, frame time), events (phase transitions), and logs
- **Test Orchestration** - Execute scenarios in order: setup, action sequence, wait conditions, assertions, teardown
- **Result Reporting** - Output structured results to `Cluiche/out/<AppName>/test-results/` (JSON + human-readable summary)
- **Crash Detection** - Detect process exit, timeout, and unresponsive applications
- **DiaAPI Command Dispatch** - Send commands to the running application via DiaDebugServer's command gateway

## Public Interfaces

### CLI Entry Point

```bash
# Run all e2e scenarios for an application
pytest Tools/e2e/ --app=cluichetest

# Run specific scenario file
pytest Tools/e2e/ --app=cluichetest --scenario=smoke_test.json

# Run with verbose WebSocket logging
pytest Tools/e2e/ --app=cluichetest -v --ws-log

# Override DiaDebugServer port
pytest Tools/e2e/ --app=cluichetest --port=9090
```

### JSON Scenario Schema

```json
{
  "name": "smoke_test",
  "description": "Verify application launches, enters main phase, and renders frames",
  "app": "cluichetest",
  "config": "Debug",
  "timeout_seconds": 30,
  "steps": [
    {
      "action": "wait_for_connection",
      "timeout_seconds": 10
    },
    {
      "action": "wait_for_phase",
      "phase": "MainPhase",
      "timeout_seconds": 5
    },
    {
      "action": "subscribe",
      "data_type": "core_metrics"
    },
    {
      "action": "wait_frames",
      "count": 60
    },
    {
      "action": "assert_metrics",
      "assertions": {
        "fps": { "min": 30.0 },
        "frame_time_ms": { "max": 33.3 }
      }
    }
  ],
  "teardown": {
    "action": "command",
    "command": "quit"
  }
}
```

### Python API (internal)

```python
class DiaTestClient:
    """WebSocket client for DiaDebugServer."""
    async def connect(self, port: int = 8080) -> None
    async def subscribe(self, data_type: str, filter: dict = None) -> None
    async def unsubscribe(self, data_type: str) -> None
    async def send_command(self, command: str, args: dict = None) -> dict
    async def get_state(self) -> dict
    async def wait_for_phase(self, phase: str, timeout: float) -> bool
    async def wait_for_event(self, event_type: str, timeout: float) -> dict
    async def get_metrics(self) -> dict

class ScenarioRunner:
    """Executes a JSON scenario against a running application."""
    def __init__(self, client: DiaTestClient, scenario: dict)
    async def run(self) -> ScenarioResult

class AppLauncher:
    """Launches and manages application processes via DiaCLI."""
    def launch(self, app: str, config: str = "Debug") -> subprocess.Popen
    def is_running(self) -> bool
    def kill(self) -> None
    def wait(self, timeout: float) -> int
```

## Directory Structure

```
Tools/e2e/
  conftest.py              # pytest fixtures (launcher, client, scenario loading)
  test_scenarios.py        # pytest parametrize over scenario files
  dia_test_client.py       # DiaTestClient (WebSocket)
  scenario_runner.py       # ScenarioRunner (step execution)
  app_launcher.py          # AppLauncher (DiaCLI wrapper)
  assertions.py            # Assertion helpers (metrics, state, logs)
  scenarios/
    cluichetest/
      smoke_test.json
      ...
    cluicheeditor/
      ...
```

## Dependencies

### Required Systems (from Dia)
- **DiaCLI** - Build and launch target applications (`dia run`, `dia launch`)
- **DiaDebugServer** - WebSocket communication with running applications (state, metrics, commands, events, logs)

### External (Python)
- **pytest** - Test runner and assertion framework
- **websockets** - WebSocket client (stdlib-compatible async)
- **Python 3.10+** - stdlib (asyncio, subprocess, json, pathlib)

No other dependencies. No heavy frameworks.

## Non-Responsibilities

What DiaTestHarness explicitly does NOT handle:

- **Engine-side code** - No C++ modules; harness is pure Python external tooling
- **Unit testing** - GoogleTests handles that; harness is for application-level e2e
- **Visual regression** - No screenshot comparison in v1; state/metrics/logs cover critical failures
- **Performance benchmarking** - Asserts on thresholds, doesn't produce detailed profiling data
- **CI/CD integration** - CI can call pytest; harness doesn't own pipeline config
- **Input replay** - Not in v1; scenarios use commands, not raw input injection
- **Protocol extensions** - Does not own DiaDebugServer protocol changes; consumes as-is

## Related Systems

| System | Relationship | Interface |
|--------|--------------|-----------|
| DiaDebugServer | Data source | Harness connects via WebSocket, subscribes to state/metrics/events |
| DiaCLI | Launch mechanism | Harness calls `dia launch` to start applications |
| DiaAPI | Indirect | Commands sent through DiaDebugServer's command gateway |
| DiaTest | Sibling | DiaTest owns `dia test cli`; DiaTestHarness owns e2e scenarios. Could integrate as `dia test e2e` in future |
| CluicheTest TestLevels | Consumer | Multi-stage test levels are the *targets* harness scenarios validate |

## Inherited Binding Decisions

These decisions from parent platform and application specs are binding constraints on DiaTestHarness:

| Source | ID | Decision | Impact on DiaTestHarness |
|--------|----|----------|--------------------------|
| Platform | PD-005 | x64 is the only supported build target | Target applications are x64; harness doesn't compile C++ but launches x64 apps |
| Platform | PD-006 | Visual Studio project files are source of truth | Harness doesn't touch vcxproj; uses DiaCLI which wraps MSBuild |
| Platform | PD-009 | All generated output under `Cluiche/out/<AppName>/` | Test results written to `Cluiche/out/<AppName>/test-results/` |
| Platform | PD-010 | `.diagame` is the project root file | Harness resolves app targets through DiaCLI, which uses `.diagame` |
| Dia | AD-001 | Module system with YAML frontmatter | N/A — harness is external Python, not a Dia module |
| Dia | AD-002 | No STL in public APIs | N/A — harness is external Python, not a Dia module |
| Dia | AD-003 | Namespace convention `Dia::<Module>::` | N/A — harness is external Python |

## System-Specific Decisions

Decisions specific to DiaTestHarness. Binding decisions constrain all features within this system.

| ID | Decision | Rationale | Status | Binding |
|----|----------|-----------|--------|---------|
| DTH-001 | Harness is pure Python — no engine-side code | Clean boundary; harness consumes DiaDebugServer/DiaCLI. Engine commands belong in their respective modules | Proposed | Yes |
| DTH-002 | JSON scenario format (not YAML) | jsoncpp already in engine; zero Python deps for parsing; AI generates valid JSON reliably; no indentation gotchas | Proposed | Yes |
| DTH-003 | Minimal Python deps: stdlib + pytest + websockets only | No heavy frameworks; fast install; easy CI integration | Proposed | Yes |
| DTH-004 | Results output to `Cluiche/out/<AppName>/test-results/` | PD-009 compliance; structured JSON + summary text | Proposed | Yes |
| DTH-005 | Smoke test is the first scenario (app launch + frame validation) | Proves harness infrastructure before tackling complex system validation | Proposed | Yes |
| DTH-006 | Scenarios are declarative JSON, not imperative Python scripts | Enables non-programmers to write tests; AI can generate/validate scenarios; clear separation of orchestration from assertion logic | Proposed | Yes |
| DTH-007 | Application launched fresh per scenario (no shared state between scenarios) | Isolation prevents test pollution; deterministic starting state | Proposed | Yes |

**Status values:** `Proposed` · `Accepted` · `Rejected` · `Superseded`
**Binding:** `Yes` = enforced on all features · `No` = guidance only

## Features

Features within the DiaTestHarness system (create with `/spec-feature`):

| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| Harness Core | WebSocket client, app launcher, scenario runner, assertion framework, result output | TBD | Draft |
| Smoke Test Scenario | First scenario: CluicheTest launches, connects, enters main phase, renders 60 frames at >30 FPS, exits cleanly | TBD | Draft |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Lifecycle | How does the harness detect that DiaDebugServer is ready after launch? | Poll WebSocket connection with backoff (100ms, 200ms, 400ms...) up to `wait_for_connection` timeout. If timeout expires, scenario fails with "connection timeout" |
| 2 | Crash Detection | How does the harness distinguish crash from clean exit? | Monitor process return code: 0 = clean exit, non-zero = crash. Also detect connection drop during scenario execution as unexpected termination |
| 3 | Parallelism | Can scenarios run in parallel? | Not in v1 — sequential execution. Parallel would require port management and process isolation. Consider for future feature |
| 4 | Timeouts | What happens when a wait step times out? | Scenario fails immediately with the step that timed out. Remaining steps skipped. Teardown still attempted |
| 5 | DiaTest Integration | Should this be `dia test e2e` from day one? | Start as raw pytest. Integrate into DiaTest as `dia test e2e` as a follow-up feature once harness is proven |
| 6 | Port Conflicts | What if port 8080 is in use? | Harness accepts `--port` override. Default matches DiaDebugServer default (8080). Scenario can also specify port. Fail fast with clear error if connection refused |
| 7 | Config | Should harness support Release builds? | Yes — `--config Release` passed to DiaCLI. But DiaDebugServer is Debug-only (DDS-004), so e2e scenarios only work with Debug builds. Document this clearly |
| 8 | Assertions | What assertion types are needed beyond metrics? | Phase state (current phase matches expected), module state (started/stopped), event occurrence (phase transition happened), log presence (specific log line appeared), absence assertions (no error logs) |
| 9 | Flakiness | How to handle timing-sensitive assertions? | Wait conditions with explicit timeouts replace fixed sleeps. Metrics assertions use windowed averages (last N samples), not single-frame values. Retry logic is NOT built in — flaky = broken |
| 10 | Scenario Composition | Can scenarios include/reference other scenarios? | Not in v1. Each scenario is self-contained. Shared setup patterns extracted into conftest.py fixtures instead |

## Status

`Approved`

## Research

docs/research/e2e_testing/summary.md

## Notes

**Integration with DiaTest:**
DiaTestHarness is a sibling to DiaTest's existing `dia test cli` command. Future integration path:
```bash
dia test e2e                    # Run all e2e scenarios
dia test e2e --app=cluichetest  # Run scenarios for specific app
dia test cli                    # Existing: run DiaCLI pytest suite
dia test all                    # Future: run everything
```

**Scenario development workflow:**
1. Write JSON scenario describing expected behavior
2. Run `pytest Tools/e2e/ --scenario=new_test.json` — fails (RED)
3. Implement/fix the feature in the engine
4. Run again — passes (GREEN)
5. Commit scenario + implementation together
