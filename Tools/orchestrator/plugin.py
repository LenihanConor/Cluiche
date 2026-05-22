"""pytest plugin for DiaAutomation E2E scenarios."""
import json
import subprocess
import time
from pathlib import Path

import pytest

from orchestrator.client import DiaClient


# ---------------------------------------------------------------------------
# Fixtures
# ---------------------------------------------------------------------------

@pytest.fixture(scope="session")
def app_launcher(request):
    """Launch the app via 'dia launch', wait for WebSocket readiness.

    Graceful quit on teardown; force-kills if app does not exit within 5s.
    Session-scoped so a single process runs all scenarios in the suite.
    Skip launch if --no-launch flag passed to CLI (plan["no_launch"] = True).
    """
    plan = request.config._dia_plan
    if plan.get("no_launch"):
        yield None
        return

    cmd = ["dia", "launch", plan["app"], "--config", plan.get("config", "Debug")]
    proc = subprocess.Popen(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )

    yield proc

    # --- teardown: graceful quit then force-kill ---
    port = plan.get("port", 9876)
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
    """Connected DiaClient. Connects on setup, disconnects on teardown."""
    plan = request.config._dia_plan
    port = plan.get("port", 9876)
    client = DiaClient(port=port)
    client.connect(timeout=60.0)
    yield client
    client.disconnect()


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

    errors = _collect_log_errors(item.config, plan)
    if errors:
        pytest.fail("Log errors detected in session logs:\n" + "\n".join(errors))


def _collect_log_errors(config, plan: dict) -> list:
    """Read the most-recent session log for the app, return ERROR-level messages."""
    repo_root = getattr(config, "_dia_repo_root", None)
    if repo_root is None:
        return []

    app = plan.get("app", "")
    sessions_dir = Path(repo_root) / "Cluiche" / "out" / _app_out_name(app) / "sessions"
    if not sessions_dir.exists():
        return []

    # Most recent session directory
    session_dirs = sorted(sessions_dir.iterdir(), key=lambda p: p.name, reverse=True)
    if not session_dirs:
        return []

    log_path = session_dirs[0] / "log.jsonl"
    if not log_path.exists():
        return []

    errors = []
    try:
        for line in log_path.read_text(encoding="utf-8").splitlines():
            entry = json.loads(line)
            if entry.get("level", "").lower() == "error":
                errors.append(entry.get("msg", line))
    except Exception:
        pass
    return errors


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
