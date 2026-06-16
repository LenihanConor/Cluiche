"""Tests for the dia test editor-ui subcommand and its underlying ui_runner."""
from unittest.mock import patch, MagicMock
from click.testing import CliRunner
from dia_cli.cli_main import cli


# ── CLI discovery ─────────────────────────────────────────────────────────────

def test_editor_ui_discovered_under_test():
    """dia test --help lists 'editor-ui' as a subcommand."""
    runner = CliRunner()
    result = runner.invoke(cli, ['test', '--help'])
    assert result.exit_code == 0
    assert 'editor-ui' in result.output


def test_editor_ui_help_shows_watch_and_coverage():
    """dia test editor-ui --help shows both --watch and --coverage flags."""
    runner = CliRunner()
    result = runner.invoke(cli, ['test', 'editor-ui', '--help'])
    assert result.exit_code == 0
    assert '--watch' in result.output
    assert '--coverage' in result.output


def test_editor_ui_help_shows_vitest():
    """dia test editor-ui --help mentions Vitest in its description."""
    runner = CliRunner()
    result = runner.invoke(cli, ['test', 'editor-ui', '--help'])
    assert result.exit_code == 0
    assert 'Vitest' in result.output


# ── ui_runner implementation unit tests ───────────────────────────────────────

def _make_run_kwargs(
    filter_pattern=None,
    watch=False,
    coverage=False,
    docker=False,
    repo_root=None,
):
    """Helper: build the kwargs dict for ui_runner.run()."""
    from pathlib import Path
    return dict(
        repo_root=Path(__file__).parents[3],  # repo root for tests
        ui_subpath="Dia/DiaApplicationFlowEditor/UI",
        docker_subcmd="editor-ui",
        filter_pattern=filter_pattern,
        watch=watch,
        coverage=coverage,
        docker=docker,
    )


@patch('dia_cli.commands.test.ui_runner.check_node_modules', return_value=False)
def test_run_returns_2_when_node_modules_missing(mock_check):
    """Returns exit code 2 when node_modules directory is absent."""
    from dia_cli.commands.test.ui_runner import run
    code = run(**_make_run_kwargs())
    assert code == 2


@patch('dia_cli.commands.test.ui_runner.check_node_modules', return_value=True)
@patch('dia_cli.commands.test.ui_runner.subprocess.run')
def test_run_includes_coverage_flag(mock_subproc, mock_check):
    """--coverage causes '--coverage' to appear in the npm command args."""
    mock_subproc.return_value = MagicMock(returncode=0)
    from dia_cli.commands.test.ui_runner import run
    run(**_make_run_kwargs(coverage=True))
    cmd_arg = mock_subproc.call_args[0][0]
    assert '--coverage' in cmd_arg


@patch('dia_cli.commands.test.ui_runner.check_node_modules', return_value=True)
@patch('dia_cli.commands.test.ui_runner.subprocess.run')
def test_run_no_coverage_flag_by_default(mock_subproc, mock_check):
    """--coverage is NOT passed when coverage=False."""
    mock_subproc.return_value = MagicMock(returncode=0)
    from dia_cli.commands.test.ui_runner import run
    run(**_make_run_kwargs(coverage=False))
    cmd_arg = mock_subproc.call_args[0][0]
    assert '--coverage' not in cmd_arg


@patch('dia_cli.commands.test.ui_runner.check_node_modules', return_value=True)
@patch('dia_cli.commands.test.ui_runner.subprocess.run')
def test_run_uses_test_watch_script_in_watch_mode(mock_subproc, mock_check):
    """watch=True selects the 'test:watch' npm script."""
    mock_subproc.return_value = MagicMock(returncode=0)
    from dia_cli.commands.test.ui_runner import run
    run(**_make_run_kwargs(watch=True))
    cmd_arg = mock_subproc.call_args[0][0]
    assert 'test:watch' in cmd_arg


@patch('dia_cli.commands.test.ui_runner.check_node_modules', return_value=True)
@patch('dia_cli.commands.test.ui_runner.subprocess.run')
def test_run_uses_test_script_without_watch(mock_subproc, mock_check):
    """watch=False selects the plain 'test' npm script (not 'test:watch')."""
    mock_subproc.return_value = MagicMock(returncode=0)
    from dia_cli.commands.test.ui_runner import run
    run(**_make_run_kwargs(watch=False))
    cmd_arg = mock_subproc.call_args[0][0]
    assert 'test:watch' not in cmd_arg
    assert 'test' in cmd_arg


@patch('dia_cli.commands.test.ui_runner.check_node_modules', return_value=True)
@patch('dia_cli.commands.test.ui_runner.subprocess.run')
def test_run_returns_0_on_success(mock_subproc, mock_check):
    """Returns 0 when subprocess exits successfully."""
    mock_subproc.return_value = MagicMock(returncode=0)
    from dia_cli.commands.test.ui_runner import run
    assert run(**_make_run_kwargs()) == 0


@patch('dia_cli.commands.test.ui_runner.check_node_modules', return_value=True)
@patch('dia_cli.commands.test.ui_runner.subprocess.run')
def test_run_returns_nonzero_on_failure(mock_subproc, mock_check):
    """Returns the subprocess returncode on failure."""
    mock_subproc.return_value = MagicMock(returncode=1)
    from dia_cli.commands.test.ui_runner import run
    assert run(**_make_run_kwargs()) == 1


@patch('dia_cli.commands.test.ui_runner.check_node_modules', return_value=True)
@patch('dia_cli.commands.test.ui_runner.subprocess.run')
def test_run_passes_filter_to_vitest(mock_subproc, mock_check):
    """--filter value is forwarded to vitest via -t."""
    mock_subproc.return_value = MagicMock(returncode=0)
    from dia_cli.commands.test.ui_runner import run
    run(**_make_run_kwargs(filter_pattern="MyTestSuite"))
    cmd_arg = mock_subproc.call_args[0][0]
    assert '-t' in cmd_arg
    assert 'MyTestSuite' in cmd_arg
