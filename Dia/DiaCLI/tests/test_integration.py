"""Integration tests that hit real machine state.

Run with:  pytest -m integration
Skip with: pytest -m 'not integration'

These tests verify that the full CLI wiring works against the actual repo
and installed toolchain. They do NOT modify the repo — read-only checks only.
"""
import json
import subprocess
import sys
from pathlib import Path

import pytest

pytestmark = pytest.mark.integration

_REPO_ROOT = Path(__file__).resolve().parents[2]  # Dia/DiaCLI/tests → Dia/DiaCLI → repo root is 3 up? No.
# tests/ is at Dia/DiaCLI/tests/, parents[0]=tests, parents[1]=DiaCLI, parents[2]=Dia, parents[3]=repo
# But find_repo_root walks looking for Cluiche/Cluiche.sln, so let's just use it:
from dia_cli.utils.repo_root import find_repo_root

_REPO_ROOT = find_repo_root(__file__)
_CLI_ROOT = _REPO_ROOT / "Dia" / "DiaCLI"


def _run_dia(*args, timeout=30):
    """Run `python -m dia_cli <args>` from the CLI root directory."""
    cmd = [sys.executable, "-m", "dia_cli"] + list(args)
    result = subprocess.run(
        cmd,
        capture_output=True,
        text=True,
        cwd=str(_CLI_ROOT),
        timeout=timeout,
    )
    return result


# ---------------------------------------------------------------------------
# find_repo_root resolves correctly from various file locations
# ---------------------------------------------------------------------------

class TestRepoRoot:
    def test_from_test_file(self):
        root = find_repo_root(__file__)
        assert (root / "Cluiche" / "Cluiche.sln").exists()

    def test_from_utils_module(self):
        from dia_cli.utils import repo_root as mod
        root = find_repo_root(mod.__file__)
        assert (root / "Cluiche" / "Cluiche.sln").exists()

    def test_from_runner_file_path(self):
        runner_path = _CLI_ROOT / "dia_cli" / "commands" / "test" / "cli_runner.py"
        assert runner_path.exists(), f"runner not found at {runner_path}"
        root = find_repo_root(str(runner_path))
        assert (root / "Cluiche" / "Cluiche.sln").exists()

    def test_raises_on_bogus_path(self, tmp_path):
        bogus = tmp_path / "nowhere" / "test.py"
        bogus.parent.mkdir(parents=True)
        bogus.write_text("")
        with pytest.raises(RuntimeError, match="Could not find repo root"):
            find_repo_root(str(bogus))


# ---------------------------------------------------------------------------
# deps.json schema and content
# ---------------------------------------------------------------------------

class TestDepsJsonIntegrity:
    @pytest.fixture(autouse=True)
    def load_deps(self):
        from dia_cli.utils.deps_restore import load_deps
        self.deps = load_deps(_REPO_ROOT)

    def test_loads_without_error(self):
        assert isinstance(self.deps, list)
        assert len(self.deps) > 0

    def test_all_entries_have_required_fields(self):
        required = {"id", "version", "unzip_to"}
        for dep in self.deps:
            missing = required - dep.keys()
            assert not missing, f"dep '{dep.get('id', '?')}' missing fields: {missing}"

    def test_no_duplicate_ids(self):
        ids = [d["id"] for d in self.deps]
        assert len(ids) == len(set(ids)), f"duplicate dep ids: {ids}"

    def test_all_unzip_to_are_under_external(self):
        for dep in self.deps:
            assert dep["unzip_to"].startswith("External/"), \
                f"dep '{dep['id']}' unzip_to '{dep['unzip_to']}' is not under External/"


# ---------------------------------------------------------------------------
# pipeline.toml schema and content
# ---------------------------------------------------------------------------

class TestPipelineTomlIntegrity:
    @pytest.fixture(autouse=True)
    def load_config(self):
        from dia_cli.commands.pipeline.pipeline_config import load_pipeline_config
        self.config = load_pipeline_config(_REPO_ROOT)

    def test_loads_without_error(self):
        assert self.config is not None

    def test_has_googletest_target(self):
        assert "googletest" in self.config.targets

    def test_all_targets_have_project(self):
        for name, target in self.config.targets.items():
            assert target.project, f"target '{name}' has empty project"

    def test_all_target_projects_exist(self):
        for name, target in self.config.targets.items():
            project_path = _REPO_ROOT / target.project
            assert project_path.exists(), \
                f"target '{name}' project not found: {target.project}"

    def test_all_target_stages_are_valid(self):
        from dia_cli.commands.pipeline.pipeline_config import VALID_STAGES
        for name, target in self.config.targets.items():
            invalid = [s for s in target.stages if s not in VALID_STAGES]
            assert not invalid, f"target '{name}' has invalid stages: {invalid}"


# ---------------------------------------------------------------------------
# dia env verify — full CLI round-trip
# ---------------------------------------------------------------------------

class TestEnvVerify:
    def test_verify_json_produces_valid_json(self):
        result = _run_dia("env", "verify", "--json")
        assert result.returncode in (0, 1, 2), \
            f"unexpected exit code {result.returncode}: {result.stderr}"
        data = json.loads(result.stdout)
        assert "result" in data
        assert "checks" in data
        assert isinstance(data["checks"], list)
        assert data["result"] in ("pass", "warn", "fail")

    def test_verify_json_has_toolchain_checks(self):
        result = _run_dia("env", "verify", "--json")
        data = json.loads(result.stdout)
        categories = {c["category"] for c in data["checks"]}
        assert "toolchain" in categories

    def test_verify_json_has_deps_checks(self):
        result = _run_dia("env", "verify", "--json")
        data = json.loads(result.stdout)
        categories = {c["category"] for c in data["checks"]}
        assert "deps" in categories

    def test_verify_json_has_submodules_checks(self):
        result = _run_dia("env", "verify", "--json")
        data = json.loads(result.stdout)
        categories = {c["category"] for c in data["checks"]}
        assert "submodules" in categories

    def test_verify_json_check_fields(self):
        result = _run_dia("env", "verify", "--json")
        data = json.loads(result.stdout)
        for check in data["checks"]:
            assert "name" in check
            assert "category" in check
            assert "status" in check
            assert check["status"] in ("pass", "warn", "fail")

    def test_verify_text_does_not_crash(self):
        result = _run_dia("env", "verify")
        assert result.returncode in (0, 1, 2), \
            f"unexpected exit code {result.returncode}: {result.stderr}"
        assert "Result:" in result.stdout

    def test_verify_toolchain_only_flag(self):
        result = _run_dia("env", "verify", "--toolchain", "--json")
        data = json.loads(result.stdout)
        categories = {c["category"] for c in data["checks"]}
        assert categories == {"toolchain"}

    def test_verify_deps_only_flag(self):
        result = _run_dia("env", "verify", "--deps", "--json")
        data = json.loads(result.stdout)
        categories = {c["category"] for c in data["checks"]}
        assert categories == {"deps"}


# ---------------------------------------------------------------------------
# dia pipeline — real protoc and msbuild
# ---------------------------------------------------------------------------

class TestPipelineIntegration:
    def test_pipeline_help_exits_0(self):
        result = _run_dia("pipeline", "--help")
        assert result.returncode == 0
        assert "pipeline" in result.stdout.lower()

    def test_proto_compile_runs(self):
        result = _run_dia("pipeline", "--stage", "proto-compile", timeout=60)
        assert result.returncode == 0, \
            f"proto-compile failed (exit {result.returncode}):\n{result.stdout}\n{result.stderr}"

    def test_compile_code_builds_diacore(self):
        """Compile DiaCore via the pipeline — validates MSBuild resolution and Directory.Build.targets."""
        result = _run_dia(
            "pipeline", "--stage", "compile-code",
            "--target", "googletest", "--config", "Debug",
            timeout=120,
        )
        assert result.returncode == 0, \
            f"compile-code failed (exit {result.returncode}):\n{result.stdout[-2000:]}\n{result.stderr[-500:]}"


# ---------------------------------------------------------------------------
# dia test cli — the runner can invoke itself
# ---------------------------------------------------------------------------

class TestTestCliIntegration:
    def test_test_cli_exits_0(self):
        """Run the full pytest suite via `dia test cli` and verify it passes."""
        result = _run_dia("test", "cli", timeout=60)
        assert result.returncode == 0, \
            f"dia test cli failed (exit {result.returncode}):\n{result.stdout[-2000:]}\n{result.stderr[-500:]}"

    def test_test_cli_filter_runs_subset(self):
        """Run only the plugin discovery tests via --filter."""
        result = _run_dia("test", "cli", "--filter", "test_plugin_discovery", timeout=30)
        assert result.returncode == 0
        assert "passed" in result.stdout


# ---------------------------------------------------------------------------
# CheckResult shared type is consistent across all consumers
# ---------------------------------------------------------------------------

class TestCheckResultConsistency:
    def test_all_verify_categories_produce_consistent_check_fields(self):
        """Verify all check categories produce results with consistent fields via CLI JSON output."""
        result = _run_dia("env", "verify", "--json")
        data = json.loads(result.stdout)
        required_fields = {"name", "category", "status"}
        valid_statuses = {"pass", "warn", "fail"}
        for check in data["checks"]:
            missing = required_fields - check.keys()
            assert not missing, f"check '{check.get('name', '?')}' missing fields: {missing}"
            assert check["status"] in valid_statuses, \
                f"check '{check['name']}' has invalid status: {check['status']}"

    def test_all_three_categories_present(self):
        """All verify categories (toolchain, deps, submodules) return results."""
        result = _run_dia("env", "verify", "--json")
        data = json.loads(result.stdout)
        categories = {c["category"] for c in data["checks"]}
        for expected in ("toolchain", "deps", "submodules"):
            assert expected in categories, f"missing category: {expected}"
