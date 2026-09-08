# Feature Spec: dia-orchestrate

## Parent System
@docs/specs/applications/dia/systems/diacli/diacli.md

## Builds On
@docs/specs/applications/dia/systems/diacli/plugin-discovery.md
@docs/specs/applications/dia/systems/diaapplicationflow/baseline-commands.md

## Research
@docs/research/e2e_testing/design-decisions.md (Section 2, 6, 13)

## Traceability

| Level | Spec | Decision IDs honored |
|-------|------|---------------------|
| Platform | @docs/specs/platform/Cluiche.md | PD-005, PD-006, PD-009 |
| Application | @docs/specs/applications/dia/dia.md | AD-001 |
| System (DiaCLI) | @docs/specs/applications/dia/systems/diacli/diacli.md | SD-CLI-001, SD-CLI-002, SD-CLI-004, SD-CLI-006, SD-CLI-008 |
| System (DiaAutomation) | @docs/specs/applications/dia/systems/diaautomation/diaautomation.md | SD-AUT-005 |

## Problem Statement

There is no way to launch a Dia application, connect to it over WebSocket, drive it through stages via DiaAutomation commands, and assert correctness from outside. The E2E orchestration stack needs a CLI entry point (`dia orchestrate`) that wraps pytest, and a pytest plugin that provides fixtures for WebSocket communication with DiaAutomation's command surface.

## Acceptance Criteria

| # | Criterion | Verification |
|---|-----------|--------------|
| AC1 | `dia orchestrate` command exists, discoverable via DiaCLI plugin system (`dia --help` lists it). | Manual verification |
| AC2 | `dia orchestrate --suite=<name>` loads a plan JSON from `Tools/orchestrator/plans/<app>/<name>.json` and invokes pytest with the listed scenarios. | Unit test + integration test |
| AC3 | `dia orchestrate` without `--suite` runs all scenarios found in the default plan (`default.json`). | Integration test |
| AC4 | Plan JSON specifies: `app` (target name), `port` (WebSocket port), `scenarios` (list of relative paths), `config` (build config). | Schema validation test |
| AC5 | pytest plugin provides a `dia_client` fixture — a connected `DiaClient` WebSocket instance with helper methods: `navigate_to`, `quit`, `validate`, `report`, `pause`, `resume`, `send_command`. | Unit test (mocked WebSocket) |
| AC6 | pytest plugin provides an `app_launcher` fixture that starts the app via `dia launch <target>` and waits for WebSocket connection readiness (retry until connected, 60s timeout, then fail). | Integration test |
| AC7 | `DiaClient` helper methods send the correct JSON command envelope and return the parsed `data` field from the response. | Unit test |
| AC8 | Implicit `assert_no_log_errors` assertion runs at end of every scenario unless disabled via `@pytest.mark.allow_log_errors`. | Unit test |
| AC9 | Exit code 0 = all scenarios pass, non-zero = failure. Maps directly to pytest exit codes. | Integration test |
| AC10 | Scenarios are pytest test functions located in `Tools/orchestrator/scenarios/<app>/<stage>/*.py`. | Convention + integration test |
| AC11 | Plans live at `Tools/orchestrator/plans/<app>/*.json`. | Convention |
| AC12 | CLI supports `--filter=<expr>` to run a subset of scenarios (maps to pytest `-k`). | Manual verification |
| AC13 | CLI supports `--port=<N>` to override the default WebSocket port. | Manual verification |
| AC14 | CLI supports `--no-launch` to skip app launch and connect to an already-running app. | Manual verification |
| AC15 | Scenarios run sequentially in plan order. | Integration test |
| AC16 | `app_launcher` fixture kills the app process on teardown (even on test failure). | Integration test |
| AC17 | `dia orchestrate --help` shows usage, flags, and available suites. | Manual verification |

## Design

### Directory Layout

```
Tools/orchestrator/
├── cli.py              # Core orchestrate logic (plan loading, pytest invocation)
├── plugin.py           # pytest plugin (fixtures, implicit assertions)
├── client.py           # DiaClient WebSocket wrapper
├── conftest.py         # Auto-registers plugin.py with pytest
├── plans/
│   └── cluichetest/
│       └── default.json
└── scenarios/
    └── cluichetest/
        └── boot/
            └── test_boot_smoke.py
```

DiaCLI integration:
```
Dia/DiaCLI/dia_cli/cli/orchestrate.py   # Plugin entry point: delegates to Tools/orchestrator/cli.py
```

### Plan JSON Schema

```json
{
    "app": "cluichetest",
    "port": 9876,
    "config": "Debug",
    "scenarios": [
        "scenarios/cluichetest/boot/test_boot_smoke.py",
        "scenarios/cluichetest/rigid_body/test_basic_shapes.py"
    ]
}
```

Fields:
- `app` (string, required) — target name for `dia launch`
- `port` (int, optional, default: 9876) — DiaDebugServer WebSocket port
- `config` (string, optional, default: "Debug") — build configuration
- `scenarios` (string[], required) — ordered list of scenario file paths relative to `Tools/orchestrator/`

### DiaClient (sync-native)

```python
import json
from websockets.sync.client import connect as ws_connect
import time

class AutomationError(Exception):
    pass

class DiaClient:
    """Sync WebSocket client for DiaAutomation commands."""

    def __init__(self, host="localhost", port=9876):
        self.host = host
        self.port = port
        self._ws = None

    def connect(self, timeout=60.0):
        """Retry WebSocket connection every 500ms until success or timeout."""
        deadline = time.time() + timeout
        while time.time() < deadline:
            try:
                self._ws = ws_connect(f"ws://{self.host}:{self.port}")
                return
            except (ConnectionRefusedError, OSError):
                time.sleep(0.5)
        raise TimeoutError(f"Could not connect to app within {timeout}s")

    def disconnect(self):
        """Close WebSocket connection."""
        if self._ws:
            self._ws.close()
            self._ws = None

    def send_command(self, command: str, params: dict = None) -> dict:
        """Send command, return parsed response data field."""
        request = {"command": command, "params": params or {}}
        self._ws.send(json.dumps(request))
        response = json.loads(self._ws.recv())
        if not response.get("success"):
            raise AutomationError(response.get("error", "unknown error"))
        return response.get("data", {})

    # --- Helpers (thin wrappers over send_command) ---

    def navigate_to(self, target: str) -> dict:
        return self.send_command("dia.automation.navigate_to", {"target": target})

    def quit(self) -> dict:
        return self.send_command("dia.app.quit")

    def validate(self, checkpoint: str) -> dict:
        return self.send_command("dia.automation.validate", {"checkpoint": checkpoint})

    def report(self) -> dict:
        return self.send_command("dia.app.report")

    def pause(self) -> dict:
        return self.send_command("dia.automation.pause")

    def resume(self) -> dict:
        return self.send_command("dia.automation.resume")
```

### pytest Plugin

```python
# plugin.py
import pytest
import subprocess
import time
from pathlib import Path
from .client import DiaClient

@pytest.fixture(scope="session")
def app_launcher(request):
    """Launch app via dia launch, wait for WebSocket ready, graceful quit on teardown."""
    plan = request.config._dia_plan
    proc = subprocess.Popen(
        ["dia", "launch", plan["app"], "--config", plan.get("config", "Debug")],
        # ... stdout/stderr capture ...
    )
    yield proc
    # Graceful shutdown: quit command first, force-kill fallback
    try:
        client = DiaClient(port=plan.get("port", 9876))
        client.connect(timeout=5.0)
        client.quit()
        client.disconnect()
        proc.wait(timeout=5.0)
    except Exception:
        proc.kill()
        proc.wait()

@pytest.fixture
def dia_client(app_launcher, request):
    """Connected DiaClient. Disconnects on teardown."""
    plan = request.config._dia_plan
    client = DiaClient(port=plan.get("port", 9876))
    client.connect(timeout=60.0)
    yield client
    client.disconnect()

def pytest_runtest_teardown(item, nextitem):
    """Implicit assert_no_log_errors unless @pytest.mark.allow_log_errors."""
    if "allow_log_errors" not in [m.name for m in item.iter_markers()]:
        # File-based: read DiaObservation session logs, grep for ERROR
        session_dir = _find_session_log_dir(item.config._dia_plan)
        errors = _grep_log_errors(session_dir)
        if errors:
            pytest.fail(f"Log errors detected:\n" + "\n".join(errors))
```

### CLI Entry Point

```python
# Dia/DiaCLI/dia_cli/cli/orchestrate.py
import click
import subprocess
import json
from pathlib import Path

ORCHESTRATOR_ROOT = Path(__file__).resolve().parents[4] / "Tools" / "orchestrator"

@click.command()
@click.option("--suite", default="cluichetest/default", help="Plan path: <app>/<name> (without .json)")
@click.option("--filter", "filter_expr", default=None, help="pytest -k filter expression")
@click.option("--port", type=int, default=None, help="Override WebSocket port")
@click.option("--no-launch", is_flag=True, help="Skip app launch, connect to running app")
@click.pass_context
def cli(ctx, suite, filter_expr, port, no_launch):
    """Run E2E automation scenarios against a Dia application."""
    plan_path = ORCHESTRATOR_ROOT / "plans" / f"{suite}.json"

    if not plan_path.exists():
        click.echo(f"Plan not found: {plan_path}", err=True)
        ctx.exit(1)

    plan = json.loads(plan_path.read_text())

    # Override port if specified
    if port:
        plan["port"] = port

    # Build pytest args
    pytest_args = ["-x"]  # fail-fast
    for scenario in plan["scenarios"]:
        pytest_args.append(str(ORCHESTRATOR_ROOT / scenario))
    if filter_expr:
        pytest_args.extend(["-k", filter_expr])

    # Set plan as env or conftest config
    # Invoke pytest
    exit_code = subprocess.call(["pytest"] + pytest_args, cwd=str(ORCHESTRATOR_ROOT))
    ctx.exit(exit_code)
```

### Scenario Authoring

Scenarios are plain sync pytest functions. No async ceremony:

```python
# scenarios/cluichetest/boot/test_boot_smoke.py

def test_app_boots_and_reports(dia_client):
    result = dia_client.report()
    assert result["stage"] == "Boot"
    assert len(result["modules"]) > 0

def test_navigate_to_gameplay(dia_client):
    dia_client.navigate_to("DummyStage")
    result = dia_client.report()
    assert result["stage"] == "DummyStage"
```

### Dependencies (Python)

- `websockets` — WebSocket client
- `pytest` — test runner
- `click` — CLI framework (already a DiaCLI dependency)

### Out of Scope

- **Gate ordering / fail-fast groups** — sequential for now; add groups later if CI duration hurts
- **Parallel scenario execution** — sequential only (design-decisions §14)
- **Visual regression** — screenshots as artifact only, not pass/fail
- **Binary protobuf on wire** — JSON encoding sufficient
- **Custom scenario engine** — pytest IS the runner
- **Plan generation** — manually authored JSON
- **App build** — `dia orchestrate` uses `dia launch` (run only), not `dia run` (build+run). Caller builds first.

## Files Touched

| File | Change |
|------|--------|
| `Dia/DiaCLI/dia_cli/cli/orchestrate.py` | New — DiaCLI plugin entry point |
| `Tools/orchestrator/cli.py` | New — core orchestrate logic |
| `Tools/orchestrator/plugin.py` | New — pytest plugin (fixtures, implicit assertions) |
| `Tools/orchestrator/client.py` | New — DiaClient WebSocket wrapper |
| `Tools/orchestrator/conftest.py` | New — auto-registers pytest plugin |
| `Tools/orchestrator/plans/cluichetest/default.json` | New — default plan for CluicheTest |
| `Tools/orchestrator/scenarios/cluichetest/boot/test_boot_smoke.py` | New — placeholder/first smoke scenario (item #5) |
| `Tools/orchestrator/pyproject.toml` | New — Python dependencies |
| `docs/specs/systems/dia/diacli.md` | Add feature row |

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Create `Tools/orchestrator/` directory structure, `pyproject.toml` with dependencies. | Directory exists, `pip install` succeeds | Done | haiku | |
| 2 | Implement `client.py` — DiaClient sync-native + all helper methods. | Sync, retry connect, helpers wrap send_command | Done | sonnet | sync-native per AR-1 |
| 3 | Implement `plugin.py` — `app_launcher` fixture (subprocess + retry connect + 60s timeout + kill on teardown), `dia_client` fixture, implicit `assert_no_log_errors`. | Markers registered, log error grep from sessions dir | Done | sonnet | |
| 4 | Implement `cli.py` — plan loading, pytest invocation, `--filter`/`--port`/`--no-launch` support. | `load_plan` raises FileNotFoundError on missing; `_DiaPlugin` injects plan | Done | sonnet | |
| 5 | Create `Dia/DiaCLI/dia_cli/cli/orchestrate.py` — DiaCLI plugin delegating to `Tools/orchestrator/cli.py`. | `dia orchestrate --help` works; listed in `dia --help` | Done | haiku | |
| 6 | Create `conftest.py` for auto-registration. Create `plans/cluichetest/default.json` placeholder. | `pytest_plugins = ["orchestrator.plugin"]` | Done | haiku | |
| 7 | Run `dia test cli` to confirm DiaCLI plugin discovery still works. | 6/6 plugin_discovery tests pass | Done | sonnet | 7 pre-existing failures unrelated |
| 8 | Add feature row to `diacli.md`. Update spec status to Done. Commit. | Doc only | Done | haiku | |

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-005 | x64 only | `dia launch <target>` runs x64 builds. Plan's `config` defaults to Debug x64. |
| PD-006 | VS project files source of truth | No C++ project changes. Python tooling only. |
| PD-009 | Generated output under Cluiche/out/ | pytest output (reports, logs) goes to stdout/CI. No generated files placed in-tree. |
| AD-001 | Module docs with YAML frontmatter | N/A — Python tooling, not a Dia C++ module. |
| SD-CLI-001 | MDK CLI architecture foundation | Orchestrate plugin follows same `cli/<name>.py` pattern as existing DiaCLI commands. |
| SD-CLI-002 | Python-based implementation | Entire feature is Python. |
| SD-CLI-004 | Plugin discovery via filesystem | `orchestrate.py` placed in `dia_cli/cli/` — auto-discovered. |
| SD-CLI-006 | Click framework for argument parsing | CLI uses `@click.command()` with `@click.option()`. |
| SD-CLI-008 | Exit codes follow Unix conventions | Maps directly to pytest exit codes (0=pass, 1+=fail). |
| SD-AUT-005 | Consistent {success, data/error} envelope | DiaClient expects and parses this envelope from all commands. `send_command` raises on `success: false`. |

## Open Questions

None.

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Async model | Should DiaClient be async-native with a sync wrapper, or sync-native using `websockets.sync`? | Sync-native using `websockets.sync` (v12+). Scenario authors write plain `def test_*(dia_client):` with no async ceremony. No `pytest-asyncio` dependency. Simple. |
| 2 | App readiness | How does app_launcher detect "ready"? Retry WebSocket connect, or wait for a specific handshake message? | Retry WebSocket connect. Loop: attempt every 500ms, up to 60s timeout. Once TCP handshake + WebSocket upgrade completes, the app is ready. No special handshake message needed. |
| 3 | Log error detection | How does `assert_no_log_errors` get the log data? Poll `dia.app.report`? Subscribe to a log stream? | File-based. Read DiaObservation session log files (`Cluiche/out/<app>/sessions/<id>/logs/`) at teardown, grep for ERROR-level entries. Decoupled from WebSocket, works even on crash, no new C++ commands needed. |
| 4 | Plan discovery | `--suite` currently requires knowing the app name for path resolution. Should the plan embed `app` so the CLI can find it from just the suite name? | Use slash convention: `dia orchestrate --suite=cluichetest/default`. The plan path is `plans/cluichetest/default.json`. Explicit, no search needed, no ambiguity between apps. |
| 5 | Process management | On Windows, `subprocess.kill()` may not cleanly shut down the app. Should app_launcher send `dia.app.quit` first, then kill after timeout? | Yes — graceful first, force-kill fallback. Teardown: (1) send `dia.app.quit` via WebSocket, (2) wait up to 5s for process exit, (3) `proc.kill()` if still alive. Ensures DiaObservation session files flush for `assert_no_log_errors`. |

## Status

`Done` (2026-05-21) — Implemented. Plan: [dia-orchestrate.plan.md](dia-orchestrate.plan.md)