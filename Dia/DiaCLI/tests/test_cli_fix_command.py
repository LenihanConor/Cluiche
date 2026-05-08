"""Tests for dia fix command and fix_handler."""
from __future__ import annotations

from unittest.mock import MagicMock, patch

import pytest
from click.testing import CliRunner

from dia_cli.cli_main import cli
from dia_cli.commands.fix.fix_handler import run_fix


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _invoke_fix(target="googletest", filter_pattern=None,
                model="ollama/qwen2.5-coder:14b", config="Debug",
                max_iterations=10, dry_run=False, returncode=0):
    """Run run_fix with aider mocked out. Returns (exit_code, cmd_list)."""
    with patch("dia_cli.commands.fix.fix_handler.shutil.which", return_value="/usr/bin/aider"), \
         patch("dia_cli.commands.fix.fix_handler.subprocess.run") as mock_run:
        mock_run.return_value = MagicMock(returncode=returncode)
        code = run_fix(target=target, filter_pattern=filter_pattern,
                       model=model, config=config,
                       max_iterations=max_iterations, dry_run=dry_run)
        cmd = mock_run.call_args[0][0] if mock_run.called else None
        return code, cmd


# ---------------------------------------------------------------------------
# fix_handler — argument construction
# ---------------------------------------------------------------------------

class TestRunFix:

    def test_basic_invocation(self):
        code, cmd = _invoke_fix()
        assert code == 0
        assert cmd[0] == "aider"
        assert "--test-cmd" in cmd
        assert "--auto-test" in cmd
        assert "--yes" in cmd

    def test_test_cmd_contains_target(self):
        _, cmd = _invoke_fix(target="googletest")
        idx = cmd.index("--test-cmd")
        assert "googletest" in cmd[idx + 1]

    def test_test_cmd_contains_config(self):
        _, cmd = _invoke_fix(config="Release")
        idx = cmd.index("--test-cmd")
        assert "Release" in cmd[idx + 1]

    def test_filter_appended_when_provided(self):
        _, cmd = _invoke_fix(filter_pattern="SomeModule*")
        idx = cmd.index("--test-cmd")
        assert "--filter=SomeModule*" in cmd[idx + 1]

    def test_no_filter_when_not_provided(self):
        _, cmd = _invoke_fix(filter_pattern=None)
        idx = cmd.index("--test-cmd")
        assert "--filter" not in cmd[idx + 1]

    def test_model_passed_to_aider(self):
        _, cmd = _invoke_fix(model="claude-sonnet-4-6")
        idx = cmd.index("--model")
        assert cmd[idx + 1] == "claude-sonnet-4-6"

    def test_default_model_is_local(self):
        _, cmd = _invoke_fix()
        idx = cmd.index("--model")
        assert cmd[idx + 1] == "ollama/qwen2.5-coder:14b"

    def test_propagates_nonzero_exit_code(self):
        code, _ = _invoke_fix(returncode=42)
        assert code == 42

    def test_returns_1_when_aider_not_on_path(self):
        with patch("dia_cli.commands.fix.fix_handler.shutil.which", return_value=None):
            code = run_fix(target="googletest", filter_pattern=None,
                           model="ollama/qwen2.5-coder:14b", config="Debug",
                           max_iterations=10, dry_run=False)
            assert code == 1


# ---------------------------------------------------------------------------
# fix_handler — max_iterations
# ---------------------------------------------------------------------------

class TestMaxIterations:

    def test_default_iterations_passed_to_aider(self):
        _, cmd = _invoke_fix(max_iterations=10)
        assert "--iterations" in cmd
        idx = cmd.index("--iterations")
        assert cmd[idx + 1] == "10"

    def test_custom_iterations_passed_to_aider(self):
        _, cmd = _invoke_fix(max_iterations=3)
        idx = cmd.index("--iterations")
        assert cmd[idx + 1] == "3"

    def test_iterations_1_stops_after_one_attempt(self):
        _, cmd = _invoke_fix(max_iterations=1)
        idx = cmd.index("--iterations")
        assert cmd[idx + 1] == "1"


# ---------------------------------------------------------------------------
# fix_handler — dry_run
# ---------------------------------------------------------------------------

class TestDryRun:

    def test_dry_run_does_not_call_subprocess(self, capsys):
        code, cmd = _invoke_fix(dry_run=True)
        assert code == 0
        assert cmd is None  # subprocess.run was never called

    def test_dry_run_prints_command(self, capsys):
        with patch("dia_cli.commands.fix.fix_handler.shutil.which", return_value="/usr/bin/aider"), \
             patch("dia_cli.commands.fix.fix_handler.subprocess.run") as mock_run:
            run_fix(target="googletest", filter_pattern=None,
                    model="ollama/qwen2.5-coder:14b", config="Debug",
                    max_iterations=5, dry_run=True)
            mock_run.assert_not_called()
        out = capsys.readouterr().out
        assert "[dry-run]" in out
        assert "aider" in out
        assert "googletest" in out

    def test_dry_run_includes_all_options_in_output(self, capsys):
        with patch("dia_cli.commands.fix.fix_handler.shutil.which", return_value="/usr/bin/aider"), \
             patch("dia_cli.commands.fix.fix_handler.subprocess.run"):
            run_fix(target="googletest", filter_pattern="MyModule*",
                    model="claude-sonnet-4-6", config="Release",
                    max_iterations=3, dry_run=True)
        out = capsys.readouterr().out
        assert "MyModule*" in out
        assert "claude-sonnet-4-6" in out
        assert "Release" in out
        assert "3" in out


# ---------------------------------------------------------------------------
# fix_handler — summary output
# ---------------------------------------------------------------------------

class TestSummaryOutput:

    def test_success_prints_all_passing(self, capsys):
        _invoke_fix(returncode=0)
        out = capsys.readouterr().out
        assert "All tests passing" in out

    def test_failure_prints_failure_summary(self, capsys):
        _invoke_fix(returncode=1)
        out = capsys.readouterr().out
        assert "failures" in out.lower()

    def test_failure_includes_exit_code_in_summary(self, capsys):
        _invoke_fix(returncode=5)
        out = capsys.readouterr().out
        assert "5" in out

    def test_dry_run_does_not_print_summary(self, capsys):
        with patch("dia_cli.commands.fix.fix_handler.shutil.which", return_value="/usr/bin/aider"), \
             patch("dia_cli.commands.fix.fix_handler.subprocess.run"):
            run_fix(target="googletest", filter_pattern=None,
                    model="ollama/qwen2.5-coder:14b", config="Debug",
                    max_iterations=10, dry_run=True)
        out = capsys.readouterr().out
        assert "All tests passing" not in out
        assert "failures" not in out.lower()


# ---------------------------------------------------------------------------
# CLI — command discoverable and flags wired
# ---------------------------------------------------------------------------

def test_fix_command_discovered():
    runner = CliRunner()
    result = runner.invoke(cli, ["fix", "--help"])
    assert result.exit_code == 0
    assert "TARGET" in result.output
    assert "ollama/qwen2.5-coder:14b" in result.output
    assert "--max-iterations" in result.output
    assert "--dry-run" in result.output


def test_fix_command_passes_args_to_handler():
    runner = CliRunner()
    with patch("dia_cli.commands.fix.fix_handler.shutil.which", return_value="/usr/bin/aider"), \
         patch("dia_cli.commands.fix.fix_handler.subprocess.run") as mock_run:
        mock_run.return_value = MagicMock(returncode=0)
        result = runner.invoke(cli, [
            "fix", "googletest",
            "--filter=MyModule*",
            "--model", "claude-sonnet-4-6",
            "--config", "Release",
            "--max-iterations", "5",
        ])
        assert result.exit_code == 0
        cmd = mock_run.call_args[0][0]
        idx = cmd.index("--test-cmd")
        test_cmd = cmd[idx + 1]
        assert "googletest" in test_cmd
        assert "Release" in test_cmd
        assert "--filter=MyModule*" in test_cmd
        assert cmd[cmd.index("--model") + 1] == "claude-sonnet-4-6"
        assert cmd[cmd.index("--iterations") + 1] == "5"


def test_fix_dry_run_flag_via_cli(capsys):
    runner = CliRunner()
    with patch("dia_cli.commands.fix.fix_handler.shutil.which", return_value="/usr/bin/aider"), \
         patch("dia_cli.commands.fix.fix_handler.subprocess.run") as mock_run:
        result = runner.invoke(cli, ["fix", "googletest", "--dry-run"])
        mock_run.assert_not_called()
    assert result.exit_code == 0
    assert "[dry-run]" in result.output


# ---------------------------------------------------------------------------
# env verify — local-llm checks
# ---------------------------------------------------------------------------

class TestCheckLocalLlm:

    def _run_checks(self, aider_on_path, ollama_on_path, ollama_list_output=""):
        from dia_cli.commands.env.verify_orchestrator import _check_local_llm

        def which_side_effect(name):
            if name == "aider":
                return "/usr/bin/aider" if aider_on_path else None
            if name == "ollama":
                return "/usr/bin/ollama" if ollama_on_path else None
            return None

        mock_result = MagicMock()
        mock_result.stdout = ollama_list_output

        with patch("dia_cli.commands.env.verify_orchestrator.shutil.which",
                   side_effect=which_side_effect), \
             patch("dia_cli.commands.env.verify_orchestrator._sp.run",
                   return_value=mock_result):
            return _check_local_llm()

    def _status(self, checks, name):
        return next(c.status for c in checks if c.name == name)

    def test_all_present(self):
        checks = self._run_checks(
            aider_on_path=True,
            ollama_on_path=True,
            ollama_list_output="qwen2.5-coder:14b  abc123  9.0GB",
        )
        assert self._status(checks, "aider") == "pass"
        assert self._status(checks, "ollama") == "pass"
        assert self._status(checks, "qwen2.5-coder:14b") == "pass"

    def test_aider_missing(self):
        checks = self._run_checks(aider_on_path=False, ollama_on_path=True,
                                  ollama_list_output="qwen2.5-coder:14b  abc123")
        assert self._status(checks, "aider") == "fail"

    def test_ollama_missing(self):
        checks = self._run_checks(aider_on_path=True, ollama_on_path=False)
        assert self._status(checks, "ollama") == "fail"
        assert self._status(checks, "qwen2.5-coder:14b") == "fail"

    def test_model_not_pulled(self):
        checks = self._run_checks(aider_on_path=True, ollama_on_path=True,
                                  ollama_list_output="llama3:8b  xyz  4.7GB")
        assert self._status(checks, "ollama") == "pass"
        assert self._status(checks, "qwen2.5-coder:14b") == "fail"
