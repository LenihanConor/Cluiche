"""Surface tests for dia reflect command group."""
import pytest
from click.testing import CliRunner

from dia_cli.commands.reflect.group import reflect_group


def test_reflect_command_discovered():
    """reflect --help lists export-types as a subcommand."""
    runner = CliRunner()
    result = runner.invoke(reflect_group, ["--help"])
    assert result.exit_code == 0
    assert "export-types" in result.output


def test_reflect_export_types_help():
    """reflect export-types --help shows expected options."""
    runner = CliRunner()
    result = runner.invoke(reflect_group, ["export-types", "--help"])
    assert result.exit_code == 0
    assert "--output" in result.output
    assert "--config" in result.output


def test_reflect_types_command_gone():
    """The top-level 'types' command no longer appears in dia --help."""
    from dia_cli.cli_main import cli
    runner = CliRunner()
    result = runner.invoke(cli, ["--help"])
    # Check that 'types' does not appear as a top-level command entry.
    # Commands are listed as "  <name>  <description>" with leading spaces.
    # "registered-types" may appear in descriptions, so match the command column only.
    lines = result.output.splitlines()
    command_names = [line.split()[0] for line in lines if line.startswith("  ") and line.split()]
    assert "types" not in command_names
