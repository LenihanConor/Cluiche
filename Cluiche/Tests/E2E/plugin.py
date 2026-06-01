"""pytest plugin for DiaAutomation E2E scenarios."""
import json
import shutil
import subprocess
import time
from pathlib import Path

import pytest

from client import DiaClient


# ---------------------------------------------------------------------------
# Fixtures
# ---------------------------------------------------------------------------

@pytest.fixture(scope="session")
def app_launcher(request):
    """Launch the app via subprocess, wait for WebSocket readiness.

    Graceful quit on teardown; force-kills if app does not exit within 5s.
    Session-scoped so a single process runs all scenarios in the suite.
    Skip launch if --no-launch flag passed to CLI (plan["no_launch"] = True).
    """
    plan = request.config._dia_plan
    if plan.get("no_launch"):
        yield None
        return

    app = plan["app"]
    config = plan.get("config", "Debug")

    dia_exe = shutil.which("dia")
    if dia_exe is None:
        raise RuntimeError("'dia' not found on PATH — run 'dia env setup' to configure the CLI")

    proc = subprocess.Popen(
        [dia_exe, "launch", app, "--config", config],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )

    yield proc

    # --- teardown: graceful quit then force-kill ---
    port = plan.get("port", 9002)
    try:
        client = DiaClient(port=port)
        client.connect(timeout=5.0)
        try:
            client.quit()
        except Exception:
            pass
        client.disconnect()
        proc.wait(timeout=5.0)
    except Exception:
        pass
    finally:
        if proc.poll() is None:
            proc.kill()
            proc.wait()


@pytest.fixture
def dia_client(app_launcher, request):
    """Connected DiaClient. Connects, resets to Boot, disconnects on teardown."""
    plan = request.config._dia_plan
    port = plan.get("port", 9002)
    client = DiaClient(port=port)
    client.connect(timeout=60.0)
    # Reset to known state before each test so prior test failures don't cascade
    try:
        current = client.report().get("stage")
        if current != "Boot":
            client.navigate_to("Boot")
    except Exception:
        pass
    # Record how many log lines exist before this test runs (high-water mark)
    request.node._dia_log_hwm = _count_log_lines(request.config, plan)
    yield client
    client.disconnect()


@pytest.fixture
def assert_metric(dia_client):
    """Fixture that returns an assertion helper for live metric values.

    Usage: assert_metric("dia.frame.duration_ms", "<", 50)
    For histogram metrics, "value" defaults to the mean.
    """
    def _assert(name: str, op: str, threshold: float):
        result = dia_client.send_command("dia.automation.get_metric", {"name": name})
        value = result["value"]

        if isinstance(value, dict):
            value = value["mean"]

        ops = {
            "<":  lambda a, b: a < b,
            ">":  lambda a, b: a > b,
            "<=": lambda a, b: a <= b,
            ">=": lambda a, b: a >= b,
            "==": lambda a, b: a == b,
            "!=": lambda a, b: a != b,
        }
        if op not in ops:
            pytest.fail(f"Unknown operator: '{op}'")
        if not ops[op](value, threshold):
            pytest.fail(f"Metric '{name}' = {value}, expected {op} {threshold}")

    return _assert


# ---------------------------------------------------------------------------
# Implicit log-error assertion
# ---------------------------------------------------------------------------

def pytest_runtest_teardown(item, nextitem):
    """After each test, assert no ERROR-level log entries unless allow_log_errors."""
    marker_names = {m.name for m in item.iter_markers()}
    if "allow_log_errors" in marker_names:
        return

    plan = getattr(item.config, "_dia_plan", None)
    if plan is None:
        return

    hwm = getattr(item, "_dia_log_hwm", 0)
    errors = _collect_log_errors(item.config, plan, skip_lines=hwm)
    if errors:
        pytest.fail("Log errors detected in session logs:\n" + "\n".join(errors))


def _count_log_lines(config, plan: dict) -> int:
    """Return the current line count of the most-recent session log."""
    log_path = _find_log_path(config, plan)
    if log_path is None:
        return 0
    try:
        return len(log_path.read_text(encoding="utf-8").splitlines())
    except Exception:
        return 0


def _collect_log_errors(config, plan: dict, skip_lines: int = 0) -> list:
    """Return ERROR-level messages from the most-recent session log after skip_lines."""
    log_path = _find_log_path(config, plan)
    if log_path is None:
        return []

    errors = []
    try:
        lines = log_path.read_text(encoding="utf-8").splitlines()
        for line in lines[skip_lines:]:
            try:
                entry = json.loads(line)
                if entry.get("level", "").lower() == "error":
                    errors.append(entry.get("msg", line))
            except json.JSONDecodeError:
                pass
    except Exception:
        pass
    return errors


def _find_log_path(config, plan: dict):
    """Return the Path to the current session's log.jsonl, or None."""
    repo_root = getattr(config, "_dia_repo_root", None)
    if repo_root is None:
        return None

    app = plan.get("app", "")
    sessions_dir = Path(repo_root) / "Cluiche" / "out" / _app_out_name(app) / "sessions"
    if not sessions_dir.exists():
        return None

    session_dirs = sorted(sessions_dir.iterdir(), key=lambda p: p.name, reverse=True)
    if not session_dirs:
        return None

    log_path = session_dirs[0] / "log.jsonl"
    return log_path if log_path.exists() else None


def _app_out_name(app: str) -> str:
    mapping = {
        "cluichetest": "CluicheTest",
        "cluicheeditor": "CluicheEditor",
        "googletest": "GoogleTests",
    }
    return mapping.get(app.lower(), app)


# ---------------------------------------------------------------------------
# Marker registration (suppress PytestUnknownMarkWarning)
# ---------------------------------------------------------------------------

def pytest_configure(config):
    config.addinivalue_line(
        "markers",
        "allow_log_errors: skip implicit log-error assertion for this test",
    )
