"""Unit tests for dia test e2e subcommand."""
import sys
from types import ModuleType
from unittest.mock import MagicMock, patch

import pytest
from click.testing import CliRunner

from dia_cli.cli_main import cli


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _make_e2e_mock(load_plan_return=None, run_orchestration_return=0):
    """Return a fake 'cli' module that stands in for Cluiche/Tests/E2E/cli.py."""
    mock_cli_module = ModuleType("cli")
    mock_load_plan = MagicMock(return_value=load_plan_return if load_plan_return is not None else {})
    mock_run_orch = MagicMock(return_value=run_orchestration_return)
    mock_cli_module.load_plan = mock_load_plan
    mock_cli_module.run_orchestration = mock_run_orch
    return mock_cli_module, mock_load_plan, mock_run_orch


# ---------------------------------------------------------------------------
# CLI discovery
# ---------------------------------------------------------------------------

def test_e2e_subcommand_discovered():
    runner = CliRunner()
    result = runner.invoke(cli, ["test", "--help"])
    assert result.exit_code == 0
    assert "e2e" in result.output


def test_e2e_help_shows_all_options():
    runner = CliRunner()
    result = runner.invoke(cli, ["test", "e2e", "--help"])
    assert result.exit_code == 0
    assert "--suite" in result.output
    assert "--filter" in result.output
    assert "--scenario" in result.output
    assert "--list" in result.output
    assert "--port" in result.output
    assert "--no-launch" in result.output


# ---------------------------------------------------------------------------
# ImportError → exit 1
# ---------------------------------------------------------------------------

def test_e2e_import_error_exits_1():
    """When the E2E cli module cannot be imported, exit code must be 1 and output must contain ERROR."""
    runner = CliRunner()
    # Setting sys.modules["cli"] = None causes Python to raise ImportError on
    # 'from cli import ...' — this is the documented sentinel-None behaviour.
    with patch.dict(sys.modules, {"cli": None}):
        result = runner.invoke(cli, ["test", "e2e"])

    assert result.exit_code == 1
    assert "ERROR" in result.output


# ---------------------------------------------------------------------------
# FileNotFoundError from load_plan → exit 1
# ---------------------------------------------------------------------------

def test_e2e_missing_plan_exits_1():
    """When load_plan raises FileNotFoundError, exit code must be 1 and output must contain ERROR."""
    runner = CliRunner()
    mock_cli_module, mock_load_plan, _ = _make_e2e_mock()
    mock_load_plan.side_effect = FileNotFoundError("plans/foo/bar.json not found")

    with patch.dict(sys.modules, {"cli": mock_cli_module}):
        result = runner.invoke(cli, ["test", "e2e", "--suite=foo/bar"])

    assert result.exit_code == 1
    assert "ERROR" in result.output


# ---------------------------------------------------------------------------
# Happy path — args forwarded to run_orchestration
# ---------------------------------------------------------------------------

def test_e2e_passes_args_to_run_orchestration():
    """run_orchestration is called with the correct filter_expr and no_launch values."""
    runner = CliRunner()
    mock_cli_module, mock_load_plan, mock_run_orch = _make_e2e_mock(
        load_plan_return={"port": 9000},
        run_orchestration_return=0,
    )

    with patch.dict(sys.modules, {"cli": mock_cli_module}):
        result = runner.invoke(cli, [
            "test", "e2e",
            "--suite=myapp/myplan",
            "--filter=test_foo",
            "--no-launch",
        ])

    assert mock_run_orch.called
    call_kwargs = mock_run_orch.call_args[1]
    assert call_kwargs["filter_expr"] == "test_foo"
    assert call_kwargs["no_launch"] is True
