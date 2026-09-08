"""CLI-level tests for 'dia api' subcommands."""
import pytest
from unittest.mock import Mock, patch
from click.testing import CliRunner

from dia_cli.cli.api import cli as api_cli


# ---------------------------------------------------------------------------
# Command discovery
# ---------------------------------------------------------------------------

def test_api_command_discovered():
    runner = CliRunner()
    result = runner.invoke(api_cli, ["--help"])
    assert result.exit_code == 0
    assert "list" in result.output
    assert "exec" in result.output


def test_api_list_help():
    runner = CliRunner()
    result = runner.invoke(api_cli, ["list", "--help"])
    assert result.exit_code == 0


def test_api_exec_help():
    runner = CliRunner()
    result = runner.invoke(api_cli, ["exec", "--help"])
    assert result.exit_code == 0


# ---------------------------------------------------------------------------
# Bridge unavailable
# ---------------------------------------------------------------------------

def _make_unavailable_bridge():
    bridge = Mock()
    bridge.is_available.return_value = False
    return bridge


def test_api_list_when_bridge_unavailable():
    runner = CliRunner(mix_stderr=False)
    with patch("dia_cli.cli.api.get_bridge", return_value=_make_unavailable_bridge()):
        result = runner.invoke(api_cli, ["list"])
    assert result.exit_code != 0
    combined = result.output + (result.stderr if result.stderr else "")
    assert "not available" in combined.lower() or "error" in combined.lower()


def test_api_exec_when_bridge_unavailable():
    runner = CliRunner(mix_stderr=False)
    with patch("dia_cli.cli.api.get_bridge", return_value=_make_unavailable_bridge()):
        result = runner.invoke(api_cli, ["exec", "some-command"])
    assert result.exit_code != 0


# ---------------------------------------------------------------------------
# Bridge available — list returns commands
# ---------------------------------------------------------------------------

def test_api_list_with_mock_bridge():
    bridge = Mock()
    bridge.is_available.return_value = True
    bridge.list_commands.return_value = ["cmd-a", "cmd-b"]

    runner = CliRunner()
    with patch("dia_cli.cli.api.get_bridge", return_value=bridge):
        result = runner.invoke(api_cli, ["list"])

    assert "cmd-a" in result.output
    assert "cmd-b" in result.output
