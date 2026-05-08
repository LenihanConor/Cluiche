"""Unit tests for dia test env-integration."""
from pathlib import Path
from unittest.mock import MagicMock, patch, call

import pytest
from click.testing import CliRunner

from dia_cli.cli_main import cli
from dia_cli.commands.test.env_integration_runner import (
    run,
    _classify_failure,
    _generate_fix,
)


# ---------------------------------------------------------------------------
# CLI discovery
# ---------------------------------------------------------------------------

def test_env_integration_subcommand_discovered():
    runner = CliRunner()
    result = runner.invoke(cli, ["test", "--help"])
    assert result.exit_code == 0
    assert "env-integration" in result.output


def test_env_integration_help():
    runner = CliRunner()
    result = runner.invoke(cli, ["test", "env-integration", "--help"])
    assert result.exit_code == 0
    assert "--skip-env" in result.output
    assert "--max-auto-fixes" in result.output
    assert "--no-fix" in result.output
    assert "--docker" in result.output


# ---------------------------------------------------------------------------
# _classify_failure
# ---------------------------------------------------------------------------

def test_classify_msvc_error_is_logic():
    kind, reason = _classify_failure("Build FAILED\n error C2065: 'x': undeclared identifier")
    assert kind == "logic"


def test_classify_linker_error_is_logic():
    kind, reason = _classify_failure("error LNK2019: unresolved external symbol")
    assert kind == "logic"


def test_classify_pytest_failure_is_logic():
    kind, reason = _classify_failure("FAILED tests/test_foo.py::test_bar - AssertionError")
    assert kind == "logic"


def test_classify_path_error_is_env():
    kind, reason = _classify_failure("'python' is not recognized as an internal command")
    assert kind == "env"


def test_classify_sha256_mismatch_is_env():
    kind, reason = _classify_failure("ERROR: sfml SHA-256 mismatch\n  expected: abc\n  actual: xyz")
    assert kind == "env"


def test_classify_module_not_found_is_env():
    kind, reason = _classify_failure("ModuleNotFoundError: No module named 'requests'")
    assert kind == "env"


def test_classify_unknown_falls_back_to_env():
    kind, reason = _classify_failure("Something went wrong unexpectedly")
    assert kind == "env"


# ---------------------------------------------------------------------------
# _generate_fix — no API key
# ---------------------------------------------------------------------------

def test_generate_fix_no_api_key(monkeypatch):
    monkeypatch.delenv("ANTHROPIC_API_KEY", raising=False)
    fix = _generate_fix("pipeline", "some output", "PATH not configured")
    assert "ANTHROPIC_API_KEY" in fix


def test_generate_fix_with_api_key(monkeypatch):
    monkeypatch.setenv("ANTHROPIC_API_KEY", "test-key")
    mock_client = MagicMock()
    mock_message = MagicMock()
    mock_message.content = [MagicMock(text="Add Python to PATH in Directory.Build.targets")]
    mock_client.messages.create.return_value = mock_message

    mock_anthropic = MagicMock()
    mock_anthropic.Anthropic.return_value = mock_client

    with patch.dict("sys.modules", {"anthropic": mock_anthropic}):
        fix = _generate_fix("pipeline", "error output", "PATH issue")

    assert len(fix) > 0


# ---------------------------------------------------------------------------
# AC8: --no-fix exits immediately on first failure
# ---------------------------------------------------------------------------

@patch("dia_cli.commands.test.env_integration_runner.subprocess.Popen")
def test_no_fix_exits_on_first_failure(mock_popen, tmp_path):
    mock_proc = MagicMock()
    mock_proc.stdout = iter(["error: something went wrong\n"])
    mock_proc.wait.return_value = None
    mock_proc.returncode = 1
    mock_popen.return_value = mock_proc

    code = run(repo_root=tmp_path, skip_env=True, max_auto_fixes=1,
               no_fix=True, inject_fault=None, docker=False)
    assert code == 1


# ---------------------------------------------------------------------------
# AC2: all stages pass → exit 0
# ---------------------------------------------------------------------------

@patch("dia_cli.commands.test.env_integration_runner.subprocess.Popen")
def test_all_stages_pass_exits_0(mock_popen, tmp_path):
    mock_proc = MagicMock()
    mock_proc.stdout = iter(["all good\n"])
    mock_proc.wait.return_value = None
    mock_proc.returncode = 0
    mock_popen.return_value = mock_proc

    code = run(repo_root=tmp_path, skip_env=False, max_auto_fixes=1,
               no_fix=False, inject_fault=None, docker=False)
    assert code == 0


# ---------------------------------------------------------------------------
# AC9: --skip-env skips stage 1
# ---------------------------------------------------------------------------

@patch("dia_cli.commands.test.env_integration_runner.subprocess.Popen")
def test_skip_env_runs_only_two_stages(mock_popen, tmp_path):
    mock_proc = MagicMock()
    mock_proc.stdout = iter(["ok\n"])
    mock_proc.wait.return_value = None
    mock_proc.returncode = 0
    mock_popen.return_value = mock_proc

    code = run(repo_root=tmp_path, skip_env=True, max_auto_fixes=1,
               no_fix=False, inject_fault=None, docker=False)
    assert code == 0
    assert mock_popen.call_count == 2  # pipeline + test cli only


# ---------------------------------------------------------------------------
# AC6: logic bug → exit 1, no fix attempt
# ---------------------------------------------------------------------------

@patch("dia_cli.commands.test.env_integration_runner._generate_fix")
@patch("dia_cli.commands.test.env_integration_runner.subprocess.Popen")
def test_logic_bug_exits_1_no_fix(mock_popen, mock_gen_fix, tmp_path):
    mock_proc = MagicMock()
    mock_proc.stdout = iter(["error C2065: 'x': undeclared\n"])
    mock_proc.wait.return_value = None
    mock_proc.returncode = 1
    mock_popen.return_value = mock_proc

    code = run(repo_root=tmp_path, skip_env=True, max_auto_fixes=1,
               no_fix=False, inject_fault=None, docker=False)
    assert code == 1
    mock_gen_fix.assert_not_called()


# ---------------------------------------------------------------------------
# AC7: same stage fails 3 times → exit 4
# ---------------------------------------------------------------------------

@patch("dia_cli.commands.test.env_integration_runner._generate_fix")
@patch("dia_cli.commands.test.env_integration_runner.subprocess.Popen")
def test_stage_fails_three_times_exits_4(mock_popen, mock_gen_fix, tmp_path, monkeypatch):
    mock_proc = MagicMock()
    mock_proc.stdout = iter(["ModuleNotFoundError: requests\n"])
    mock_proc.wait.return_value = None
    mock_proc.returncode = 1
    mock_popen.return_value = mock_proc
    mock_gen_fix.return_value = "pip install requests"

    # Simulate operator always saying yes so we loop
    monkeypatch.setattr("builtins.input", lambda _: "y")

    code = run(repo_root=tmp_path, skip_env=True, max_auto_fixes=0,
               no_fix=False, inject_fault=None, docker=False)
    assert code == 4


# ---------------------------------------------------------------------------
# AC5: operator says N → exit 1
# ---------------------------------------------------------------------------

@patch("dia_cli.commands.test.env_integration_runner._generate_fix")
@patch("dia_cli.commands.test.env_integration_runner.subprocess.Popen")
def test_operator_declines_exits_1(mock_popen, mock_gen_fix, tmp_path, monkeypatch):
    mock_proc = MagicMock()
    mock_proc.stdout = iter(["ModuleNotFoundError: requests\n"])
    mock_proc.wait.return_value = None
    mock_proc.returncode = 1
    mock_popen.return_value = mock_proc
    mock_gen_fix.return_value = "pip install requests"

    monkeypatch.setattr("builtins.input", lambda _: "n")

    code = run(repo_root=tmp_path, skip_env=True, max_auto_fixes=0,
               no_fix=False, inject_fault=None, docker=False)
    assert code == 1


# ---------------------------------------------------------------------------
# Auto-fix: env failure within limit → auto-applies, re-runs, passes
# ---------------------------------------------------------------------------

@patch("dia_cli.commands.test.env_integration_runner._generate_fix")
@patch("dia_cli.commands.test.env_integration_runner.subprocess.Popen")
def test_auto_fix_then_pass(mock_popen, mock_gen_fix, tmp_path):
    """First pipeline call fails (env), second passes."""
    fail_proc = MagicMock()
    fail_proc.stdout = iter(["ModuleNotFoundError: requests\n"])
    fail_proc.wait.return_value = None
    fail_proc.returncode = 1

    pass_proc = MagicMock()
    pass_proc.stdout = iter(["all good\n"])
    pass_proc.wait.return_value = None
    pass_proc.returncode = 0

    mock_popen.side_effect = [fail_proc, pass_proc, pass_proc]
    mock_gen_fix.return_value = "pip install requests"

    code = run(repo_root=tmp_path, skip_env=True, max_auto_fixes=1,
               no_fix=False, inject_fault=None, docker=False)
    assert code == 0


# ---------------------------------------------------------------------------
# AC1: docker image missing → exit 3
# ---------------------------------------------------------------------------

@patch("dia_cli.commands.test.env_integration_runner.subprocess.run")
def test_docker_missing_image_exits_3(mock_run, tmp_path):
    mock_run.return_value = MagicMock(returncode=1)
    code = run(repo_root=tmp_path, skip_env=False, max_auto_fixes=1,
               no_fix=False, inject_fault=None, docker=True)
    assert code == 3
