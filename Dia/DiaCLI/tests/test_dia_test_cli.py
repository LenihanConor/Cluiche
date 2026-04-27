"""Unit tests for dia test cli (AC1-AC10 from diatest/cli-unit-tests.md spec)."""
from pathlib import Path
from unittest.mock import MagicMock, patch

import pytest
from click.testing import CliRunner

from dia_cli.cli_main import cli
from dia_cli.commands.test.cli_runner import run


# ---------------------------------------------------------------------------
# CLI discovery
# ---------------------------------------------------------------------------

def test_cli_subcommand_discovered():
    runner = CliRunner()
    result = runner.invoke(cli, ["test", "--help"])
    assert result.exit_code == 0
    assert "cli" in result.output


def test_cli_help():
    runner = CliRunner()
    result = runner.invoke(cli, ["test", "cli", "--help"])
    assert result.exit_code == 0
    assert "--filter" in result.output
    assert "--parallel" in result.output
    assert "--coverage-out" in result.output
    assert "--docker" in result.output


# ---------------------------------------------------------------------------
# AC1: runs pytest and exits with its return code
# ---------------------------------------------------------------------------

@patch("dia_cli.commands.test.cli_runner.subprocess.run")
def test_run_invokes_pytest(mock_run, tmp_path):
    mock_run.return_value = MagicMock(returncode=0)
    code = run(repo_root=tmp_path, filter_pattern=None, parallel=False,
                coverage_out=None, docker=False)
    assert code == 0
    args = mock_run.call_args[0][0]
    assert "pytest" in " ".join(args)


@patch("dia_cli.commands.test.cli_runner.subprocess.run")
def test_run_propagates_nonzero_exit(mock_run, tmp_path):
    mock_run.return_value = MagicMock(returncode=1)
    code = run(repo_root=tmp_path, filter_pattern=None, parallel=False,
                coverage_out=None, docker=False)
    assert code == 1


# ---------------------------------------------------------------------------
# AC2: pytest cwd is Dia/DiaCLI inside repo root
# ---------------------------------------------------------------------------

@patch("dia_cli.commands.test.cli_runner.subprocess.run")
def test_run_uses_cli_dir_as_cwd(mock_run, tmp_path):
    mock_run.return_value = MagicMock(returncode=0)
    run(repo_root=tmp_path, filter_pattern=None, parallel=False,
        coverage_out=None, docker=False)
    cwd = mock_run.call_args[1]["cwd"]
    assert cwd == str(tmp_path / "Dia" / "DiaCLI")


# ---------------------------------------------------------------------------
# AC3: --filter passes -k
# ---------------------------------------------------------------------------

@patch("dia_cli.commands.test.cli_runner.subprocess.run")
def test_run_filter_passes_k(mock_run, tmp_path):
    mock_run.return_value = MagicMock(returncode=0)
    run(repo_root=tmp_path, filter_pattern="deps_restore", parallel=False,
        coverage_out=None, docker=False)
    args = mock_run.call_args[0][0]
    joined = " ".join(args)
    assert "-k" in joined
    assert "deps_restore" in joined


# ---------------------------------------------------------------------------
# AC4: --parallel passes -n auto
# ---------------------------------------------------------------------------

@patch("dia_cli.commands.test.cli_runner.subprocess.run")
def test_run_parallel_passes_n_auto(mock_run, tmp_path):
    mock_run.return_value = MagicMock(returncode=0)
    run(repo_root=tmp_path, filter_pattern=None, parallel=True,
        coverage_out=None, docker=False)
    args = mock_run.call_args[0][0]
    joined = " ".join(args)
    assert "-n" in joined
    assert "auto" in joined


# ---------------------------------------------------------------------------
# AC5: --coverage-out passes --cov-report=xml:<path>
# ---------------------------------------------------------------------------

@patch("dia_cli.commands.test.cli_runner.subprocess.run")
def test_run_coverage_out_passes_cov_report(mock_run, tmp_path):
    mock_run.return_value = MagicMock(returncode=0)
    out_path = str(tmp_path / "coverage.xml")
    run(repo_root=tmp_path, filter_pattern=None, parallel=False,
        coverage_out=out_path, docker=False)
    args = mock_run.call_args[0][0]
    joined = " ".join(args)
    assert "--cov" in joined
    assert out_path in joined


# ---------------------------------------------------------------------------
# AC10: --docker image missing → exit 3
# ---------------------------------------------------------------------------

@patch("dia_cli.commands.test.cli_runner.subprocess.run")
def test_run_docker_missing_image_exits_3(mock_run, tmp_path, capsys):
    mock_run.return_value = MagicMock(returncode=1)
    code = run(repo_root=tmp_path, filter_pattern=None, parallel=False,
               coverage_out=None, docker=True)
    assert code == 3
    captured = capsys.readouterr()
    assert "not found" in captured.out


# ---------------------------------------------------------------------------
# --docker forwards all flags
# ---------------------------------------------------------------------------

@patch("dia_cli.commands.test.cli_runner.subprocess.run")
def test_run_docker_forwards_flags(mock_run, tmp_path):
    mock_run.side_effect = [
        MagicMock(returncode=0),  # image inspect
        MagicMock(returncode=0),  # docker run
    ]
    run(repo_root=tmp_path, filter_pattern="foo", parallel=True,
        coverage_out="out/coverage.xml", docker=True)
    docker_cmd = mock_run.call_args_list[1][0][0]
    joined = " ".join(docker_cmd)
    assert "--filter" in joined
    assert "foo" in joined
    assert "--parallel" in joined
    assert "--coverage-out" in joined
    assert "out/coverage.xml" in joined


# ---------------------------------------------------------------------------
# tests_subpath ends up in pytest command
# ---------------------------------------------------------------------------

@patch("dia_cli.commands.test.cli_runner.subprocess.run")
def test_run_includes_tests_dir(mock_run, tmp_path):
    mock_run.return_value = MagicMock(returncode=0)
    run(repo_root=tmp_path, filter_pattern=None, parallel=False,
        coverage_out=None, docker=False)
    args = mock_run.call_args[0][0]
    assert "tests" in args
