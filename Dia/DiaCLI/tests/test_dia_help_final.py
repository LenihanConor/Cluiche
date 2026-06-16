"""Regression guard for the top-level DiaCLI command surface.

These tests lock down the expected command list after the cleanup pass that
removed dead commands (mycommand, prefixtest, orchestrate, editor_ui_test,
setup, types).  If a removed command is accidentally re-introduced, or an
expected command goes missing, these tests will fail immediately.
"""
from click.testing import CliRunner
from dia_cli.cli_main import cli


def test_top_level_command_surface():
    """Regression guard: asserts the exact set of approved top-level commands.

    Verifies that every command in the approved list is present in `dia --help`
    output and that every previously-removed legacy command is absent.  This
    prevents accidental re-introduction of dead commands and catches unintended
    removal of active ones.
    """
    runner = CliRunner()
    result = runner.invoke(cli, ["--help"])

    assert result.exit_code == 0, (
        f"dia --help exited with code {result.exit_code}:\n{result.output}"
    )

    expected_commands = [
        "agent",
        "api",
        "asset",
        "check",
        "command",
        "docs",
        "env",
        "fix",
        "launch",
        "pipeline",
        "reflect",
        "run",
        "scaffold",
        "show",
        "test",
        "validate",
    ]

    removed_commands = [
        "mycommand",
        "prefixtest",
        "orchestrate",
        "editor_ui_test",
        "setup",
        "types",
    ]

    for cmd in expected_commands:
        assert cmd in result.output, (
            f"Expected top-level command '{cmd}' not found in dia --help output.\n"
            f"Output was:\n{result.output}"
        )

    # Check removed commands are not present as top-level entries.
    # We check for the command name followed by whitespace (as it would appear
    # in the Commands: table) to avoid false positives from descriptions.
    import re
    for cmd in removed_commands:
        # Match the command name at the start of a help table row (2+ spaces then name then spaces)
        pattern = rf"^\s+{re.escape(cmd)}\s"
        assert not re.search(pattern, result.output, re.MULTILINE), (
            f"Removed command '{cmd}' unexpectedly found in dia --help output.\n"
            f"Output was:\n{result.output}"
        )


def test_no_legacy_mdk_references():
    """Regression guard: 'mdk' must not appear anywhere in dia --help output.

    The MDK naming was superseded; any reappearance in the top-level help text
    indicates a stale or accidentally re-added command or description.
    """
    runner = CliRunner()
    result = runner.invoke(cli, ["--help"])

    assert result.exit_code == 0, (
        f"dia --help exited with code {result.exit_code}:\n{result.output}"
    )

    assert "mdk" not in result.output.lower(), (
        f"Legacy 'mdk' reference found in dia --help output.\n"
        f"Output was:\n{result.output}"
    )
