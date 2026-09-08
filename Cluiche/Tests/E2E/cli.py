"""Core orchestrate logic: plan loading and pytest invocation."""
import fnmatch
import io
import json
import sys
import time
from pathlib import Path

import pytest

E2E_ROOT = Path(__file__).resolve().parent


def load_plan(suite: str) -> dict:
    """Load plan JSON from plans/<suite>.json.

    suite — slash-separated path, e.g. 'cluichetest/default'.
    """
    plan_path = E2E_ROOT / "plans" / f"{suite}.json"
    if not plan_path.exists():
        raise FileNotFoundError(f"Plan not found: {plan_path}")
    with plan_path.open(encoding="utf-8") as f:
        return json.load(f)


def filter_scenarios(scenarios: list, scenario_glob: str = None) -> list:
    """Filter scenario entries using fnmatch against a glob pattern."""
    if not scenario_glob:
        return list(scenarios)
    return [s for s in scenarios if fnmatch.fnmatch(s, scenario_glob)]


def list_scenarios(plan: dict, scenario_glob: str = None) -> int:
    """Print matched scenario paths (one per line) and exit."""
    scenarios = filter_scenarios(plan.get("scenarios", []), scenario_glob)
    for s in scenarios:
        print(s)
    return 0


def run_orchestration(
    plan: dict,
    filter_expr: str = None,
    no_launch: bool = False,
    repo_root: Path = None,
    scenario_glob: str = None,
    list_only: bool = False,
) -> int:
    """Invoke pytest with the scenario list from the plan.

    Returns pytest exit code (0 = all pass).
    """
    all_scenarios = plan.get("scenarios", [])
    matched_scenarios = filter_scenarios(all_scenarios, scenario_glob)

    if list_only:
        return list_scenarios(plan, scenario_glob)

    if no_launch:
        plan = dict(plan)
        plan["no_launch"] = True

    scenario_paths = []
    for s in matched_scenarios:
        p = E2E_ROOT / s
        scenario_paths.append(str(p))

    if not scenario_paths:
        print("No scenarios matched.", file=sys.stderr)
        return 1

    pytest_args = [
        "--tb=short",
        "-s",
        "-p", "no:cacheprovider",
    ]
    pytest_args.extend(scenario_paths)
    if filter_expr:
        pytest_args.extend(["-k", filter_expr])

    # Prepare log file in the standard out/ directory
    log_path = None
    if repo_root:
        report_dir = Path(repo_root) / "Cluiche" / "out" / "e2e_reports"
        report_dir.mkdir(parents=True, exist_ok=True)
        log_path = report_dir / f"e2e_{time.strftime('%Y%m%d_%H%M%S')}.log"

    class _DiaPlugin:
        """Injects plan and repo_root into pytest config."""
        def pytest_configure(self, config):
            config._dia_plan = plan
            config._dia_repo_root = str(repo_root) if repo_root else None

    # Tee stdout/stderr to log file while still printing to console
    tee = _TeeWriter(sys.stdout, log_path) if log_path else None
    if tee:
        sys.stdout = tee
        sys.stderr = _TeeWriter(sys.stderr, log_path, tee.file)

    try:
        exit_code = pytest.main(pytest_args, plugins=[_DiaPlugin()])
    finally:
        if tee:
            sys.stdout = tee.original
            sys.stderr = sys.stderr.original if hasattr(sys.stderr, "original") else sys.__stderr__
            tee.close()

    return exit_code


class _TeeWriter:
    """Writes to both a stream and a file simultaneously."""

    def __init__(self, original, path: Path, shared_file=None):
        self.original = original
        self.file = shared_file or open(path, "w", encoding="utf-8")
        self._owns_file = shared_file is None

    def write(self, text):
        self.original.write(text)
        try:
            self.file.write(text)
        except (ValueError, OSError):
            pass

    def flush(self):
        self.original.flush()
        try:
            self.file.flush()
        except (ValueError, OSError):
            pass

    def close(self):
        if self._owns_file and self.file and not self.file.closed:
            self.file.close()

    def __getattr__(self, name):
        return getattr(self.original, name)
