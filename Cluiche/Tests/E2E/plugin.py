"""pytest plugin for DiaAutomation E2E scenarios."""
import json
import shutil
import subprocess
import time
import warnings
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

    stdout_path = _new_stdout_path(request.config)
    stdout_file = open(stdout_path, "wb") if stdout_path else None
    request.config._dia_game_stdout = stdout_path
    proc = subprocess.Popen(
        [dia_exe, "launch", app, "--config", config, "--automation"],
        stdout=stdout_file if stdout_file else subprocess.DEVNULL,
        stderr=subprocess.STDOUT if stdout_file else subprocess.DEVNULL,
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
        if stdout_file:
            stdout_file.close()


@pytest.fixture
def dia_client(app_launcher, request):
    """Connected DiaClient. Connects, resets to Boot, disconnects on teardown."""
    plan = request.config._dia_plan
    port = plan.get("port", 9002)

    # If the managed process has already exited, skip immediately rather than
    # hanging for 60s trying to connect to a dead socket.
    if app_launcher is not None and app_launcher.poll() is not None:
        pytest.skip(f"app process exited (returncode={app_launcher.returncode}) — skipping remaining tests")

    client = DiaClient(port=port)
    client.connect(timeout=60.0)
    # Reset to known state before each test so prior test failures don't cascade
    try:
        current = client.report().get("stage")
        if current != "Boot":
            client.navigate_to("Boot", timeout_s=30.0)
    except Exception as e:
        client.disconnect()
        pytest.fail(f"Could not reset to Boot before test: {e}")
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
        return value

    return _assert


# ---------------------------------------------------------------------------
# Failure context: game stdout + session log path in failure output
# ---------------------------------------------------------------------------

_TAIL_LINES = 40


def _new_stdout_path(config):
    """Path for this session's captured game stdout, or None without a repo root."""
    repo_root = getattr(config, "_dia_repo_root", None)
    if repo_root is None:
        return None
    report_dir = Path(repo_root) / "Cluiche" / "out" / "e2e_reports"
    report_dir.mkdir(parents=True, exist_ok=True)
    return report_dir / f"game_stdout_{time.strftime('%Y%m%d_%H%M%S')}.log"


def _tail(path, n=_TAIL_LINES):
    try:
        lines = Path(path).read_text(encoding="utf-8", errors="replace").splitlines()
    except OSError as e:
        return [f"<unreadable: {e}>"]
    return lines[-n:]


def _failure_context(config, plan: dict) -> str:
    """Text appended to a failing test: game stdout tail, stdout path, session log path."""
    out = []
    stdout_path = getattr(config, "_dia_game_stdout", None)
    if stdout_path:
        out.append(f"game stdout: {stdout_path}")
        tail = _tail(stdout_path)
        if tail:
            out.append(f"--- last {len(tail)} lines of game stdout ---")
            out.extend(tail)
    log_path = _find_log_path(config, plan)
    out.append(f"session log: {log_path if log_path else '<not found>'}")
    return "\n".join(out)


@pytest.hookimpl(hookwrapper=True)
def pytest_runtest_makereport(item, call):
    outcome = yield
    rep = outcome.get_result()
    if rep.when != "call" or not rep.failed:
        return
    plan = getattr(item.config, "_dia_plan", None)
    if plan is None:
        return
    try:
        rep.sections.append(("Game output / logs", _failure_context(item.config, plan)))
    except Exception as e:  # never mask the real failure
        rep.sections.append(("Game output / logs", f"<context capture failed: {e}>"))


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
    # The game holds log.jsonl open (read-denying) for the whole session, so a
    # per-test read normally hits a lock. Do not wait here: defer the scan to
    # pytest_sessionfinish, after the app has exited and flushed.
    errors, locked = _scan_log(item.config, plan, skip_lines=hwm, lock_retry_s=0.0)
    if locked:
        item.config._dia_deferred_log_scans = getattr(item.config, "_dia_deferred_log_scans", [])
        item.config._dia_deferred_log_scans.append((item.nodeid, hwm))
        return
    if errors:
        pytest.fail("Log errors detected in session logs:\n" + "\n".join(errors))


def _count_log_lines(config, plan: dict) -> int:
    """Return the current line count of the most-recent session log."""
    log_path = _find_log_path(config, plan)
    if log_path is None:
        return 0
    try:
        return len(log_path.read_text(encoding="utf-8", errors="replace").splitlines())
    except Exception as e:
        if _is_lock_error(e):
            return 0  # expected while the game runs; the scan is deferred

        warnings.warn(f"Could not read session log {log_path} for line count: {e}", RuntimeWarning)
        return 0


def _is_lock_error(e: Exception) -> bool:
    """True if e is a sharing/permission violation from a file still held open."""
    return isinstance(e, PermissionError) or getattr(e, "winerror", None) in (32, 33)


def _scan_log(config, plan: dict, skip_lines: int = 0, lock_retry_s: float = 10.0):
    """Return (errors, locked). locked=True if the log stayed locked after retries."""
    log_path = _find_log_path(config, plan)
    if log_path is None:
        return [], False

    deadline = time.monotonic() + lock_retry_s
    while True:
        try:
            lines = log_path.read_text(encoding="utf-8", errors="replace").splitlines()
            break
        except Exception as e:
            if _is_lock_error(e):
                if time.monotonic() < deadline:
                    time.sleep(0.25)
                    continue
                return [], True
            return [f"Could not read session log {log_path}: {e}"], False

    errors = []
    malformed = 0
    for line in lines[skip_lines:]:
        if not line.strip():
            continue
        try:
            entry = json.loads(line)
        except json.JSONDecodeError:
            malformed += 1
            continue
        if isinstance(entry, dict) and str(entry.get("level", "")).lower() == "error":
            errors.append(entry.get("msg", line))
    if malformed:
        warnings.warn(f"{malformed} malformed JSON line(s) skipped in {log_path}", RuntimeWarning)
    return errors, False


def _collect_log_errors(config, plan: dict, skip_lines: int = 0, lock_retry_s: float = 10.0) -> list:
    """Return ERROR-level messages from the most-recent session log after skip_lines.

    Non-UTF-8 bytes are replaced, not fatal. Malformed JSON lines are skipped
    with a warning. A log that stays locked (PermissionError) after retrying for
    lock_retry_s is warned about loudly but does not fail; any other read
    failure returns a failure message so the check is never silently skipped.
    """
    errors, locked = _scan_log(config, plan, skip_lines, lock_retry_s)
    if locked:
        warnings.warn(
            f"Session log still locked after {lock_retry_s}s; implicit log-error check NOT performed",
            RuntimeWarning,
        )
    return errors


def pytest_sessionfinish(session, exitstatus):
    """Run the deferred log-error scans now that the app has exited and flushed."""
    config = session.config
    deferred = getattr(config, "_dia_deferred_log_scans", None)
    plan = getattr(config, "_dia_plan", None)
    if not deferred or plan is None:
        return
    skip = min(h for _, h in deferred)
    errors = _collect_log_errors(config, plan, skip_lines=skip)
    if errors:
        tr = config.pluginmanager.get_plugin("terminalreporter")
        msg = "Log errors detected in session logs:\n" + "\n".join(errors)
        if tr:
            tr.write_line(msg, red=True)
        else:
            print(msg)
        session.exitstatus = pytest.ExitCode.TESTS_FAILED


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
