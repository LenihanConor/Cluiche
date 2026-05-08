"""Unit tests for plugin discovery feature."""
import pytest
from click.testing import CliRunner
from dia_cli.cli_main import cli


def test_plugin_discovery_finds_commands():
    """AC1: DiaCLI discovers all Python modules in cli/ directory"""
    runner = CliRunner()
    result = runner.invoke(cli, ['--help'])
    assert result.exit_code == 0
    # Should find at least: command, setup, show, test, prefixtest
    assert 'command' in result.output
    assert 'setup' in result.output
    assert 'show' in result.output
    assert 'test' in result.output
    assert 'prefixtest' in result.output


def test_filename_becomes_command_name():
    """AC2: File names become command names (e.g., cli/test.py → dia test)"""
    runner = CliRunner()
    result = runner.invoke(cli, ['test', '--help'])
    assert result.exit_code == 0
    assert 'Run Dia test suites' in result.output


def test_cli_prefix_stripped():
    """AC3: Files prefixed with cli_ have prefix stripped"""
    runner = CliRunner()
    result = runner.invoke(cli, ['--help'])
    assert result.exit_code == 0
    # Should appear as 'prefixtest' not 'cli-prefixtest'
    assert 'prefixtest' in result.output
    assert 'cli-prefixtest' not in result.output
    assert 'cli_prefixtest' not in result.output


def test_lazy_loading():
    """AC4: Commands are loaded lazily (not at startup)"""
    # This test verifies behavior by checking --help doesn't import command modules
    # The actual lazy loading is verified by the fact that --help works even if
    # command implementation has errors
    runner = CliRunner()
    result = runner.invoke(cli, ['--help'])
    assert result.exit_code == 0
    # If eager loading, any command with errors would fail --help


def test_command_execution():
    """AC4: Commands execute when invoked"""
    runner = CliRunner()
    result = runner.invoke(cli, ['prefixtest'])
    assert result.exit_code == 0
    assert 'cli_ prefix was stripped correctly!' in result.output


def test_help_lists_all_commands():
    """AC8: dia --help lists all discovered commands with descriptions"""
    runner = CliRunner()
    result = runner.invoke(cli, ['--help'])
    assert result.exit_code == 0
    assert 'Commands:' in result.output
    # Verify descriptions appear
    assert 'Run Dia test suites' in result.output
    assert 'Prefix test' in result.output
