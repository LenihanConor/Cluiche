"""Tests for dia fix command and fix_handler."""
from __future__ import annotations

from unittest.mock import MagicMock, patch

import pytest
from click.testing import CliRunner

from dia_cli.cli_main import cli
from dia_cli.commands.fix.fix_handler import run_fix


# ---------------------------------------------------------------------------
# fix_handler — argument construction
# ---------------------------------------------------------------------------

class TestRunFix:

    def _invoke(self, target="googletest", filter_pattern=None,
                model="ollama/qwen2.5-coder:14b", config="Debug"):
        with patch("dia_cli.commands.fix.fix_handler.shutil.which", return_value="/usr/bin/aider"), \
             patch("dia_cli.commands.fix.fix_handler.subprocess.run") as mock_run:
            mock_run.return_value = MagicMock(returncode=0)
            result = run_fix(target=target, filter_pattern=filter_pattern,
                             model=model, config=config)
            return result, mock_run.call_args[0][0]  # (exit_code, cmd_list)

    def test_basic_invocation(self):
        code, cmd = self._invoke()
        assert code == 0
        assert cmd[0] == "aider"
        assert "--test-cmd" in cmd
        assert "--auto-test" in cmd
        assert "--yes" in cmd

    def test_test_cmd_contains_target(self):
        _, cmd = self._invoke(target="googletest")
        idx = cmd.index("--test-cmd")
        assert "googletest" in cmd[idx + 1]

    def test_test_cmd_contains_config(self):
        _, cmd = self._invoke(config="Release")
        idx = cmd.index("--test-cmd")
        assert "Release" in cmd[idx + 1]

    def test_filter_appended_when_provided(self):
        _, cmd = self._invoke(filter_pattern="SomeModule*")
        idx = cmd.index("--test-cmd")
        assert "--filter=SomeModule*" in cmd[idx + 1]

    def test_no_filter_when_not_provided(self):
        _, cmd = self._invoke(filter_pattern=None)
        idx = cmd.index("--test-cmd")
        assert "--filter" not in cmd[idx + 1]

    def test_model_passed_to_aider(self):
        _, cmd = self._invoke(model="claude-sonnet-4-6")
        assert "--model" in cmd
        idx = cmd.index("--model")
        assert cmd[idx + 1] == "claude-sonnet-4-6"

    def test_default_model_is_local(self):
        _, cmd = self._invoke()
        idx = cmd.index("--model")
        assert cmd[idx + 1] == "ollama/qwen2.5-coder:14b"

    def test_propagates_nonzero_exit_code(self):
        with patch("dia_cli.commands.fix.fix_handler.shutil.which", return_value="/usr/bin/aider"), \
             patch("dia_cli.commands.fix.fix_handler.subprocess.run") as mock_run:
            mock_run.return_value = MagicMock(returncode=42)
            code = run_fix(target="googletest", filter_pattern=None,
                           model="ollama/qwen2.5-coder:14b", config="Debug")
            assert code == 42

    def test_returns_1_when_aider_not_on_path(self):
        with patch("dia_cli.commands.fix.fix_handler.shutil.which", return_value=None):
            code = run_fix(target="googletest", filter_pattern=None,
                           model="ollama/qwen2.5-coder:14b", config="Debug")
            assert code == 1


# ---------------------------------------------------------------------------
# CLI — command is discoverable
# ---------------------------------------------------------------------------

def test_fix_command_discovered():
    runner = CliRunner()
    result = runner.invoke(cli, ["fix", "--help"])
    assert result.exit_code == 0
    assert "TARGET" in result.output
    assert "ollama/qwen2.5-coder:14b" in result.output


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
        ])
        assert result.exit_code == 0
        cmd = mock_run.call_args[0][0]
        idx = cmd.index("--test-cmd")
        test_cmd = cmd[idx + 1]
        assert "googletest" in test_cmd
        assert "Release" in test_cmd
        assert "--filter=MyModule*" in test_cmd
        idx_model = cmd.index("--model")
        assert cmd[idx_model + 1] == "claude-sonnet-4-6"


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
