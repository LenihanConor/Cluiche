"""Unit tests for dia test game-ui (mirrors editor-ui AC1–AC7, game-specific paths)."""
import subprocess
from pathlib import Path
from unittest.mock import MagicMock, patch

import pytest
from click.testing import CliRunner

from dia_cli.cli_main import cli
from dia_cli.commands.test.ui_runner import run

_SUBPATH = "Cluiche/CluicheTest/UI"
_DOCKER_SUBCMD = "game-ui"


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _fake_root(tmp_path: Path) -> Path:
    ui = tmp_path / "Cluiche" / "CluicheTest" / "UI"
    ui.mkdir(parents=True)
    return tmp_path


def _fake_root_with_modules(tmp_path: Path) -> Path:
    root = _fake_root(tmp_path)
    (root / "Cluiche" / "CluicheTest" / "UI" / "node_modules").mkdir()
    return root


def _run(repo_root, filter_pattern=None, watch=False, docker=False):
    return run(
        repo_root=repo_root,
        ui_subpath=_SUBPATH,
        docker_subcmd=_DOCKER_SUBCMD,
        filter_pattern=filter_pattern,
        watch=watch,
        docker=docker,
    )


# ---------------------------------------------------------------------------
# CLI discovery
# ---------------------------------------------------------------------------

def test_game_ui_subcommand_discovered():
    runner = CliRunner()
    result = runner.invoke(cli, ["test", "--help"])
    assert result.exit_code == 0
    assert "game-ui" in result.output


def test_game_ui_help():
    runner = CliRunner()
    result = runner.invoke(cli, ["test", "game-ui", "--help"])
    assert result.exit_code == 0
    assert "Vitest" in result.output or "vitest" in result.output.lower()


# ---------------------------------------------------------------------------
# AC5: exit 2 if node_modules missing
# ---------------------------------------------------------------------------

def test_run_exits_2_when_node_modules_missing(tmp_path, capsys):
    root = _fake_root(tmp_path)
    code = _run(repo_root=root)
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
    code = _run(repo_root=root)
    assert code == 0
    args = mock_run.call_args[0][0]
    assert "run" in args
    assert "test" in args


@patch("dia_cli.commands.test.ui_runner.subprocess.run")
def test_run_propagates_nonzero_exit(mock_run, tmp_path):
    root = _fake_root_with_modules(tmp_path)
    mock_run.return_value = MagicMock(returncode=1)
    code = _run(repo_root=root)
    assert code == 1


# ---------------------------------------------------------------------------
# AC2: cwd is set to CluicheTest UI directory
# ---------------------------------------------------------------------------

@patch("dia_cli.commands.test.ui_runner.subprocess.run")
def test_run_uses_game_ui_dir_as_cwd(mock_run, tmp_path):
    root = _fake_root_with_modules(tmp_path)
    mock_run.return_value = MagicMock(returncode=0)
    _run(repo_root=root)
    cwd = mock_run.call_args[1]["cwd"]
    assert cwd == str(root / "Cluiche" / "CluicheTest" / "UI")


# ---------------------------------------------------------------------------
# AC3: --filter passes -t to vitest
# ---------------------------------------------------------------------------

@patch("dia_cli.commands.test.ui_runner.subprocess.run")
def test_run_filter_passes_t_flag(mock_run, tmp_path):
    root = _fake_root_with_modules(tmp_path)
    mock_run.return_value = MagicMock(returncode=0)
    _run(repo_root=root, filter_pattern="ExamplePanel")
    args = mock_run.call_args[0][0]
    assert "-t" in args
    assert "ExamplePanel" in args
    assert args.index("-t") == len(args) - 2


# ---------------------------------------------------------------------------
# AC4: --watch uses npm run test:watch
# ---------------------------------------------------------------------------

@patch("dia_cli.commands.test.ui_runner.subprocess.run")
def test_run_watch_uses_test_watch_script(mock_run, tmp_path):
    root = _fake_root_with_modules(tmp_path)
    mock_run.return_value = MagicMock(returncode=0)
    _run(repo_root=root, watch=True)
    args = mock_run.call_args[0][0]
    assert "test:watch" in args


# ---------------------------------------------------------------------------
# AC6: --docker re-invokes in container with "game-ui" subcommand
# ---------------------------------------------------------------------------

@patch("dia_cli.commands.test.ui_runner.subprocess.run")
def test_run_docker_invokes_docker_run(mock_run, tmp_path):
    root = _fake_root_with_modules(tmp_path)
    mock_run.side_effect = [
        MagicMock(returncode=0),  # image exists
        MagicMock(returncode=0),  # docker run
    ]
    code = _run(repo_root=root, docker=True)
    assert code == 0
    first_call_args = mock_run.call_args_list[0][0][0]
    assert first_call_args[:3] == ["docker", "image", "inspect"]
    second_call_args = mock_run.call_args_list[1][0][0]
    assert "docker" in second_call_args[0]
    assert "run" in second_call_args


@patch("dia_cli.commands.test.ui_runner.subprocess.run")
def test_run_docker_uses_game_ui_subcmd(mock_run, tmp_path):
    root = _fake_root_with_modules(tmp_path)
    mock_run.side_effect = [
        MagicMock(returncode=0),
        MagicMock(returncode=0),
    ]
    _run(repo_root=root, docker=True)
    docker_cmd = mock_run.call_args_list[1][0][0]
    joined = " ".join(docker_cmd)
    assert "game-ui" in joined


@patch("dia_cli.commands.test.ui_runner.subprocess.run")
def test_run_docker_filter_forwarded(mock_run, tmp_path):
    root = _fake_root_with_modules(tmp_path)
    mock_run.side_effect = [
        MagicMock(returncode=0),
        MagicMock(returncode=0),
    ]
    _run(repo_root=root, filter_pattern="GameBridge", docker=True)
    docker_cmd = mock_run.call_args_list[1][0][0]
    joined = " ".join(docker_cmd)
    assert "--filter" in joined
    assert "GameBridge" in joined


# ---------------------------------------------------------------------------
# AC7: --docker image missing → exit 3
# ---------------------------------------------------------------------------

@patch("dia_cli.commands.test.ui_runner.subprocess.run")
def test_run_docker_missing_image_exits_3(mock_run, tmp_path, capsys):
    root = _fake_root_with_modules(tmp_path)
    mock_run.return_value = MagicMock(returncode=1)
    code = _run(repo_root=root, docker=True)
    assert code == 3
    captured = capsys.readouterr()
    assert "not found" in captured.out


# ---------------------------------------------------------------------------
# repo_root=None falls back to _REPO_ROOT (file-relative)
# ---------------------------------------------------------------------------

@patch("dia_cli.commands.test.ui_runner.subprocess.run")
def test_run_none_repo_root_uses_file_relative_path(mock_run):
    from dia_cli.commands.test.ui_runner import _REPO_ROOT
    mock_run.return_value = MagicMock(returncode=0)
    with patch("dia_cli.commands.test.ui_runner.check_node_modules", return_value=True):
        code = run(
            repo_root=None,
            ui_subpath=_SUBPATH,
            docker_subcmd=_DOCKER_SUBCMD,
            filter_pattern=None,
            watch=False,
            docker=False,
        )
    assert code == 0
    cwd = mock_run.call_args[1]["cwd"]
    assert str(_REPO_ROOT / _SUBPATH) == cwd
