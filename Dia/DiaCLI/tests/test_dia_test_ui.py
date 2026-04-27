"""Unit tests for dia test ui (AC1–AC7 from diatest/ui.md spec)."""
import subprocess
from pathlib import Path
from unittest.mock import MagicMock, patch, call

import pytest
from click.testing import CliRunner

from dia_cli.cli_main import cli
from dia_cli.commands.test.ui_runner import check_node_modules, run


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _fake_root(tmp_path: Path) -> Path:
    ui = tmp_path / "Dia" / "DiaApplicationEditor" / "UI"
    ui.mkdir(parents=True)
    return tmp_path


def _fake_root_with_modules(tmp_path: Path) -> Path:
    root = _fake_root(tmp_path)
    (root / "Dia" / "DiaApplicationEditor" / "UI" / "node_modules").mkdir()
    return root


# ---------------------------------------------------------------------------
# CLI discovery
# ---------------------------------------------------------------------------

def test_test_group_discovered():
    """dia test appears in dia --help."""
    runner = CliRunner()
    result = runner.invoke(cli, ["--help"])
    assert result.exit_code == 0
    assert "test" in result.output


def test_ui_subcommand_discovered():
    """dia test ui appears in dia test --help."""
    runner = CliRunner()
    result = runner.invoke(cli, ["test", "--help"])
    assert result.exit_code == 0
    assert "ui" in result.output


def test_ui_help():
    """dia test ui --help exits 0 and mentions Vitest."""
    runner = CliRunner()
    result = runner.invoke(cli, ["test", "ui", "--help"])
    assert result.exit_code == 0
    assert "Vitest" in result.output or "vitest" in result.output.lower()


# ---------------------------------------------------------------------------
# check_node_modules
# ---------------------------------------------------------------------------

def test_check_node_modules_true(tmp_path):
    (tmp_path / "node_modules").mkdir()
    assert check_node_modules(tmp_path) is True


def test_check_node_modules_false(tmp_path):
    assert check_node_modules(tmp_path) is False


# ---------------------------------------------------------------------------
# AC5: exit 2 if node_modules missing
# ---------------------------------------------------------------------------

def test_run_exits_2_when_node_modules_missing(tmp_path, capsys):
    root = _fake_root(tmp_path)
    code = run(repo_root=root, filter_pattern=None, watch=False, docker=False)
    assert code == 2
    captured = capsys.readouterr()
    assert "node_modules" in captured.out
    assert "npm install" in captured.out


# ---------------------------------------------------------------------------
# AC1: runs npm run test; exits with vitest exit code
# ---------------------------------------------------------------------------

@patch("dia_cli.commands.test.ui_runner.subprocess.run")
def test_run_calls_npm_run_test(mock_run, tmp_path):
    root = _fake_root_with_modules(tmp_path)
    mock_run.return_value = MagicMock(returncode=0)
    code = run(repo_root=root, filter_pattern=None, watch=False, docker=False)
    assert code == 0
    args = mock_run.call_args[0][0]
    # Invocation: node npm-cli.js run test  (or npm run test on systems with npm on PATH)
    assert "run" in args
    assert "test" in args


@patch("dia_cli.commands.test.ui_runner.subprocess.run")
def test_run_propagates_nonzero_exit(mock_run, tmp_path):
    root = _fake_root_with_modules(tmp_path)
    mock_run.return_value = MagicMock(returncode=1)
    code = run(repo_root=root, filter_pattern=None, watch=False, docker=False)
    assert code == 1


# ---------------------------------------------------------------------------
# AC2: cwd is set to UI directory
# ---------------------------------------------------------------------------

@patch("dia_cli.commands.test.ui_runner.subprocess.run")
def test_run_uses_ui_dir_as_cwd(mock_run, tmp_path):
    root = _fake_root_with_modules(tmp_path)
    mock_run.return_value = MagicMock(returncode=0)
    run(repo_root=root, filter_pattern=None, watch=False, docker=False)
    cwd = mock_run.call_args[1]["cwd"]
    assert cwd == str(root / "Dia" / "DiaApplicationEditor" / "UI")


# ---------------------------------------------------------------------------
# AC3: --filter passes -t to vitest via npm run test -- -t <pattern>
# ---------------------------------------------------------------------------

@patch("dia_cli.commands.test.ui_runner.subprocess.run")
def test_run_filter_passes_t_flag(mock_run, tmp_path):
    root = _fake_root_with_modules(tmp_path)
    mock_run.return_value = MagicMock(returncode=0)
    run(repo_root=root, filter_pattern="ManifestStore", watch=False, docker=False)
    args = mock_run.call_args[0][0]
    assert "-t" in args
    assert "ManifestStore" in args
    assert args.index("-t") == len(args) - 2  # -t is second-to-last


# ---------------------------------------------------------------------------
# AC4: --watch uses npm run test:watch
# ---------------------------------------------------------------------------

@patch("dia_cli.commands.test.ui_runner.subprocess.run")
def test_run_watch_uses_test_watch_script(mock_run, tmp_path):
    root = _fake_root_with_modules(tmp_path)
    mock_run.return_value = MagicMock(returncode=0)
    run(repo_root=root, filter_pattern=None, watch=True, docker=False)
    args = mock_run.call_args[0][0]
    assert "test:watch" in args


# ---------------------------------------------------------------------------
# AC6: --docker re-invokes in container
# ---------------------------------------------------------------------------

@patch("dia_cli.commands.test.ui_runner.subprocess.run")
def test_run_docker_invokes_docker_run(mock_run, tmp_path):
    root = _fake_root_with_modules(tmp_path)
    # First call: docker image inspect (success); second: docker run
    mock_run.side_effect = [
        MagicMock(returncode=0),  # image exists
        MagicMock(returncode=0),  # docker run
    ]
    code = run(repo_root=root, filter_pattern=None, watch=False, docker=True)
    assert code == 0
    first_call_args = mock_run.call_args_list[0][0][0]
    assert first_call_args[:3] == ["docker", "image", "inspect"]
    second_call_args = mock_run.call_args_list[1][0][0]
    assert "docker" in second_call_args[0]
    assert "run" in second_call_args


@patch("dia_cli.commands.test.ui_runner.subprocess.run")
def test_run_docker_filter_forwarded(mock_run, tmp_path):
    root = _fake_root_with_modules(tmp_path)
    mock_run.side_effect = [
        MagicMock(returncode=0),
        MagicMock(returncode=0),
    ]
    run(repo_root=root, filter_pattern="FlowView", watch=False, docker=True)
    docker_cmd = mock_run.call_args_list[1][0][0]
    joined = " ".join(docker_cmd)
    assert "--filter" in joined
    assert "FlowView" in joined


# ---------------------------------------------------------------------------
# AC7: --docker image missing → exit 3
# ---------------------------------------------------------------------------

@patch("dia_cli.commands.test.ui_runner.subprocess.run")
def test_run_docker_missing_image_exits_3(mock_run, tmp_path, capsys):
    root = _fake_root_with_modules(tmp_path)
    mock_run.return_value = MagicMock(returncode=1)  # image not found
    code = run(repo_root=root, filter_pattern=None, watch=False, docker=True)
    assert code == 3
    captured = capsys.readouterr()
    assert "not found" in captured.out


# ---------------------------------------------------------------------------
# repo_root=None falls back to _REPO_ROOT (file-relative)
# ---------------------------------------------------------------------------

@patch("dia_cli.commands.test.ui_runner.subprocess.run")
def test_run_none_repo_root_uses_file_relative_path(mock_run):
    """When repo_root is None, runner resolves path from __file__ — not a crash."""
    from dia_cli.commands.test.ui_runner import _REPO_ROOT
    mock_run.return_value = MagicMock(returncode=0)
    # If _REPO_ROOT/Dia/DiaApplicationEditor/UI/node_modules exists this will call npm.
    # Patch check_node_modules so we always reach subprocess.run regardless of disk state.
    with patch("dia_cli.commands.test.ui_runner.check_node_modules", return_value=True):
        code = run(repo_root=None, filter_pattern=None, watch=False, docker=False)
    assert code == 0
    cwd = mock_run.call_args[1]["cwd"]
    assert str(_REPO_ROOT / "Dia/DiaApplicationEditor/UI") == cwd
