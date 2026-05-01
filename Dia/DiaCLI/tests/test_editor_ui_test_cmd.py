"""Unit tests for the editor-ui-test command."""
from unittest.mock import patch, MagicMock
from click.testing import CliRunner
from dia_cli.cli_main import cli
from dia_cli.commands.editor_ui_test_cmd import execute, _UI_DIR


# ── CLI discovery ─────────────────────────────────────────────────────────────

def test_editor_ui_test_discovered():
    """Command appears in dia --help (registered as editor_ui_test from filename)."""
    runner = CliRunner()
    result = runner.invoke(cli, ['--help'])
    assert result.exit_code == 0
    assert 'editor_ui_test' in result.output


def test_editor_ui_test_help():
    """Command has a description in its own --help."""
    runner = CliRunner()
    result = runner.invoke(cli, ['editor_ui_test', '--help'])
    assert result.exit_code == 0
    assert 'Vitest' in result.output


# ── Implementation unit tests ─────────────────────────────────────────────────

@patch('dia_cli.commands.editor_ui_test_cmd._node_available', return_value=False)
def test_execute_returns_1_when_node_missing(mock_node, capsys):
    code = execute()
    assert code == 1
    captured = capsys.readouterr()
    assert 'Node.js not found' in captured.out


@patch('dia_cli.commands.editor_ui_test_cmd._node_available', return_value=True)
@patch('dia_cli.commands.editor_ui_test_cmd.shell_run')
def test_execute_passes_run_flag(mock_shell, mock_node):
    mock_shell.return_value = MagicMock(ok=True, return_code=0)
    execute()
    cmd_arg = mock_shell.call_args[0][0]
    assert 'vitest' in cmd_arg
    assert 'run' in cmd_arg


@patch('dia_cli.commands.editor_ui_test_cmd._node_available', return_value=True)
@patch('dia_cli.commands.editor_ui_test_cmd.shell_run')
def test_execute_watch_mode_omits_run(mock_shell, mock_node):
    mock_shell.return_value = MagicMock(ok=True, return_code=0)
    execute(watch=True)
    cmd_arg = mock_shell.call_args[0][0]
    assert ' run' not in cmd_arg


@patch('dia_cli.commands.editor_ui_test_cmd._node_available', return_value=True)
@patch('dia_cli.commands.editor_ui_test_cmd.shell_run')
def test_execute_coverage_flag(mock_shell, mock_node):
    mock_shell.return_value = MagicMock(ok=True, return_code=0)
    execute(coverage=True)
    cmd_arg = mock_shell.call_args[0][0]
    assert '--coverage' in cmd_arg


@patch('dia_cli.commands.editor_ui_test_cmd._node_available', return_value=True)
@patch('dia_cli.commands.editor_ui_test_cmd.shell_run')
def test_execute_returns_0_on_success(mock_shell, mock_node):
    mock_shell.return_value = MagicMock(ok=True, return_code=0)
    assert execute() == 0


@patch('dia_cli.commands.editor_ui_test_cmd._node_available', return_value=True)
@patch('dia_cli.commands.editor_ui_test_cmd.shell_run')
def test_execute_returns_nonzero_on_failure(mock_shell, mock_node):
    mock_shell.return_value = MagicMock(ok=False, return_code=1)
    assert execute() == 1


@patch('dia_cli.commands.editor_ui_test_cmd._node_available', return_value=True)
@patch('dia_cli.commands.editor_ui_test_cmd.shell_run')
def test_execute_uses_ui_dir_as_cwd(mock_shell, mock_node):
    mock_shell.return_value = MagicMock(ok=True, return_code=0)
    execute()
    cwd_arg = mock_shell.call_args[1]['cwd']
    assert cwd_arg == _UI_DIR
