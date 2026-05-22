"""Core orchestrate logic: plan loading and pytest invocation."""
import json
import sys
from pathlib import Path

import pytest

ORCHESTRATOR_ROOT = Path(__file__).resolve().parent


def load_plan(suite: str) -> dict:
    """Load plan JSON from plans/<suite>.json.

    suite — slash-separated path, e.g. 'cluichetest/default'.
    """
    plan_path = ORCHESTRATOR_ROOT / "plans" / f"{suite}.json"
    if not plan_path.exists():
        raise FileNotFoundError(f"Plan not found: {plan_path}")
    with plan_path.open(encoding="utf-8") as f:
        return json.load(f)


def run_orchestration(
    plan: dict,
    filter_expr: str = None,
    no_launch: bool = False,
    repo_root: Path = None,
) -> int:
    """Invoke pytest with the scenario list from the plan.

    Returns pytest exit code (0 = all pass).
    """
    if no_launch:
        plan = dict(plan)
        plan["no_launch"] = True

    scenario_paths = []
    for s in plan.get("scenarios", []):
        p = ORCHESTRATOR_ROOT / s
        scenario_paths.append(str(p))

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
