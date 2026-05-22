"""Core orchestrate logic: plan loading and pytest invocation."""
import fnmatch
import json
import sys
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
        "-p", "no:cacheprovider",
    ]
    pytest_args.extend(scenario_paths)
    if filter_expr:
        pytest_args.extend(["-k", filter_expr])

    class _DiaPlugin:
        """Injects plan and repo_root into pytest config."""
        def pytest_configure(self, config):
            config._dia_plan = plan
            config._dia_repo_root = str(repo_root) if repo_root else None

    return pytest.main(pytest_args, plugins=[_DiaPlugin()])
