"""Unit tests for dia test googletest (AC1-AC9 from diatest/googletest.md spec)."""
from pathlib import Path
from unittest.mock import MagicMock, patch

import pytest
from click.testing import CliRunner

from dia_cli.cli_main import cli
from dia_cli.commands.test.googletest_runner import find_binary, run


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _make_binary(tmp_path: Path, config: str = "Debug") -> Path:
    binary = tmp_path / "Cluiche" / "bin" / "GoogleTests" / config / "x64" / "GoogleTests.exe"
    binary.parent.mkdir(parents=True, exist_ok=True)
    binary.write_bytes(b"fake exe")
    return binary


def _make_staged(tmp_path: Path, config: str = "Debug") -> Path:
    binary = _make_binary(tmp_path, config)
    (binary.parent / "python311.dll").write_bytes(b"fake dll")
    return binary


# ---------------------------------------------------------------------------
# CLI discovery
# ---------------------------------------------------------------------------

def test_googletest_subcommand_discovered():
    runner = CliRunner()
    result = runner.invoke(cli, ["test", "--help"])
    assert result.exit_code == 0
    assert "googletest" in result.output


def test_googletest_help():
    runner = CliRunner()
    result = runner.invoke(cli, ["test", "googletest", "--help"])
    assert result.exit_code == 0
    assert "--filter" in result.output
    assert "--config" in result.output
    assert "--verbose" in result.output
    assert "--docker" in result.output


# ---------------------------------------------------------------------------
# find_binary
# ---------------------------------------------------------------------------

def test_find_binary_returns_path_when_exists(tmp_path):
    _make_binary(tmp_path)
    assert find_binary(tmp_path, "Debug") is not None


def test_find_binary_returns_none_when_missing(tmp_path):
    assert find_binary(tmp_path, "Debug") is None


def test_find_binary_config_selects_release(tmp_path):
    _make_binary(tmp_path, "Release")
    assert find_binary(tmp_path, "Release") is not None
    assert find_binary(tmp_path, "Debug") is None


# ---------------------------------------------------------------------------
# AC5: exit 2 if binary missing
# ---------------------------------------------------------------------------

def test_run_exits_2_when_binary_missing(tmp_path, capsys):
    code = run(repo_root=tmp_path, config="Debug", filter_pattern=None,
               verbose=False, docker=False)
    assert code == 2
    captured = capsys.readouterr()
    assert "GoogleTests.exe not found" in captured.out
    assert "dia pipeline" in captured.out


# ---------------------------------------------------------------------------
# AC6: exit 2 if python311.dll missing
# ---------------------------------------------------------------------------

def test_run_exits_2_when_python_dll_missing(tmp_path, capsys):
    _make_binary(tmp_path)  # binary exists but no dll
    code = run(repo_root=tmp_path, config="Debug", filter_pattern=None,
               verbose=False, docker=False)
    assert code == 2
    captured = capsys.readouterr()
    assert "Runtime dependencies" in captured.out
    assert "dia pipeline" in captured.out


# ---------------------------------------------------------------------------
# AC1: runs binary and exits with its return code
# ---------------------------------------------------------------------------

@patch("dia_cli.commands.test.googletest_runner.subprocess.run")
def test_run_invokes_binary(mock_run, tmp_path):
    binary = _make_staged(tmp_path)
    mock_run.return_value = MagicMock(returncode=0)
    code = run(repo_root=tmp_path, config="Debug", filter_pattern=None,
               verbose=False, docker=False)
    assert code == 0
    args = mock_run.call_args[0][0]
    assert str(binary) == args[0]


@patch("dia_cli.commands.test.googletest_runner.subprocess.run")
def test_run_propagates_nonzero_exit(mock_run, tmp_path):
    _make_staged(tmp_path)
    mock_run.return_value = MagicMock(returncode=1)
    code = run(repo_root=tmp_path, config="Debug", filter_pattern=None,
               verbose=False, docker=False)
    assert code == 1


# ---------------------------------------------------------------------------
# AC2: cwd is set to binary directory
# ---------------------------------------------------------------------------

@patch("dia_cli.commands.test.googletest_runner.subprocess.run")
def test_run_uses_binary_dir_as_cwd(mock_run, tmp_path):
    binary = _make_staged(tmp_path)
    mock_run.return_value = MagicMock(returncode=0)
    run(repo_root=tmp_path, config="Debug", filter_pattern=None,
        verbose=False, docker=False)
    cwd = mock_run.call_args[1]["cwd"]
    assert cwd == str(binary.parent)


# ---------------------------------------------------------------------------
# AC3: --filter passes --gtest_filter
# ---------------------------------------------------------------------------

@patch("dia_cli.commands.test.googletest_runner.subprocess.run")
def test_run_filter_passes_gtest_filter(mock_run, tmp_path):
    _make_staged(tmp_path)
    mock_run.return_value = MagicMock(returncode=0)
    run(repo_root=tmp_path, config="Debug", filter_pattern="TestArray*",
        verbose=False, docker=False)
    args = mock_run.call_args[0][0]
    assert "--gtest_filter=TestArray*" in args


# ---------------------------------------------------------------------------
# AC4: --config Release uses Release binary
# ---------------------------------------------------------------------------

@patch("dia_cli.commands.test.googletest_runner.subprocess.run")
def test_run_config_release_uses_release_binary(mock_run, tmp_path):
    _make_staged(tmp_path, "Release")
    mock_run.return_value = MagicMock(returncode=0)
    run(repo_root=tmp_path, config="Release", filter_pattern=None,
        verbose=False, docker=False)
    args = mock_run.call_args[0][0]
    assert "Release" in args[0]


# ---------------------------------------------------------------------------
# AC9: --verbose passes --gtest_print_time
# ---------------------------------------------------------------------------

@patch("dia_cli.commands.test.googletest_runner.subprocess.run")
def test_run_verbose_passes_gtest_verbose(mock_run, tmp_path):
    _make_staged(tmp_path)
    mock_run.return_value = MagicMock(returncode=0)
    run(repo_root=tmp_path, config="Debug", filter_pattern=None,
        verbose=True, docker=False)
    args = mock_run.call_args[0][0]
    assert "--gtest_print_time=1" in args


# ---------------------------------------------------------------------------
# AC8: --docker image missing → exit 3
# ---------------------------------------------------------------------------

@patch("dia_cli.commands.test.googletest_runner.subprocess.run")
def test_run_docker_missing_image_exits_3(mock_run, tmp_path, capsys):
    _make_staged(tmp_path)
    mock_run.return_value = MagicMock(returncode=1)  # inspect fails
    code = run(repo_root=tmp_path, config="Debug", filter_pattern=None,
               verbose=False, docker=True)
    assert code == 3
    captured = capsys.readouterr()
    assert "not found" in captured.out


# ---------------------------------------------------------------------------
# AC7: --docker forwards all flags
# ---------------------------------------------------------------------------

@patch("dia_cli.commands.test.googletest_runner.subprocess.run")
def test_run_docker_forwards_flags(mock_run, tmp_path):
    _make_staged(tmp_path)
    mock_run.side_effect = [
        MagicMock(returncode=0),  # image inspect
        MagicMock(returncode=0),  # docker run
    ]
    run(repo_root=tmp_path, config="Release", filter_pattern="Foo*",
        verbose=True, docker=True)
    docker_cmd = mock_run.call_args_list[1][0][0]
    joined = " ".join(docker_cmd)
    assert "--config" in joined
    assert "Release" in joined
    assert "--filter" in joined
    assert "Foo*" in joined
    assert "--verbose" in joined
