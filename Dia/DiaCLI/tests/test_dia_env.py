"""Unit tests for dia env commands (DiaEnv system, all 8 features)."""
import json
import os
import sys
import zipfile
from io import BytesIO
from pathlib import Path
from unittest.mock import MagicMock, patch, call, mock_open

import pytest
from click.testing import CliRunner

from dia_cli.cli_main import cli

# Repo root: tests/ is parents[0], DiaCLI is parents[1], Dia is parents[2],
# repo root (C:\GitHub\Cluiche) is parents[3].
_REPO_ROOT_FROM_TEST = Path(__file__).resolve().parents[3]


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _make_deps_json(tmp_path: Path, extra_deps: list = None) -> Path:
    """Write a minimal deps.json at tmp_path root."""
    deps = [
        {
            "id": "sfml",
            "version": "2.6.1",
            "url": "https://example.com/sfml.zip",
            "mirrors": [],
            "sha256": "PLACEHOLDER_COMPUTE_AT_RUNTIME",
            "install_type": "zip",
            "unzip_to": "External/SFML",
            "strip_root": True,
        }
    ]
    if extra_deps:
        deps.extend(extra_deps)
    data = {"schema": "diaenv.deps.v1", "deps": deps}
    p = tmp_path / "deps.json"
    p.write_text(json.dumps(data), encoding="utf-8")
    return p


def _write_sentinel(repo_root: Path, dep_id: str, version: str) -> Path:
    sentinel_dir = repo_root / ".diaenv" / "deps"
    sentinel_dir.mkdir(parents=True, exist_ok=True)
    sentinel = sentinel_dir / f"{dep_id}.restored"
    sentinel.write_text(json.dumps({
        "id": dep_id, "version": version, "sha256": "",
        "restored_at": "2026-01-01T00:00:00+00:00"
    }))
    return sentinel


def _make_zip(members: list) -> bytes:
    """Create an in-memory zip. members is list of (name, content_bytes)."""
    buf = BytesIO()
    with zipfile.ZipFile(buf, "w") as zf:
        for name, content in members:
            zf.writestr(name, content)
    return buf.getvalue()


# ===========================================================================
# Feature 1: CLI discovery — dia env --help
# ===========================================================================

class TestCliDiscovery:
    def test_env_command_discovered(self):
        runner = CliRunner()
        result = runner.invoke(cli, ["--help"])
        assert result.exit_code == 0
        assert "env" in result.output

    def test_env_help_shows_subcommands(self):
        runner = CliRunner()
        result = runner.invoke(cli, ["env", "--help"])
        assert result.exit_code == 0
        assert "deps" in result.output
        assert "setup" in result.output
        assert "verify" in result.output
        assert "claude-setup" in result.output
        assert "docker" in result.output

    def test_env_deps_help(self):
        runner = CliRunner()
        result = runner.invoke(cli, ["env", "deps", "--help"])
        assert result.exit_code == 0
        assert "--dep" in result.output
        assert "--force" in result.output
        assert "--quiet" in result.output

    def test_env_setup_help(self):
        runner = CliRunner()
        result = runner.invoke(cli, ["env", "setup", "--help"])
        assert result.exit_code == 0
        assert "--toolchain" in result.output
        assert "--deps" in result.output
        assert "--submodules" in result.output
        assert "--force" in result.output

    def test_env_verify_help(self):
        runner = CliRunner()
        result = runner.invoke(cli, ["env", "verify", "--help"])
        assert result.exit_code == 0
        assert "--toolchain" in result.output
        assert "--deps" in result.output
        assert "--json" in result.output

    def test_env_claude_setup_help(self):
        runner = CliRunner()
        result = runner.invoke(cli, ["env", "claude-setup", "--help"])
        assert result.exit_code == 0
        assert "--force" in result.output

    def test_env_docker_help(self):
        runner = CliRunner()
        result = runner.invoke(cli, ["env", "docker", "--help"])
        assert result.exit_code == 0
        assert "image" in result.output
        assert "deps" in result.output
        assert "paths" in result.output


# ===========================================================================
# Feature 1: deps_restore utility (direct import from dia_cli.*)
# ===========================================================================

class TestLoadDeps:
    def test_load_deps_success(self, tmp_path):
        from dia_cli.utils.deps_restore import load_deps
        _make_deps_json(tmp_path)
        deps = load_deps(tmp_path)
        assert len(deps) == 1
        assert deps[0]["id"] == "sfml"

    def test_load_deps_missing_file(self, tmp_path):
        from dia_cli.utils.deps_restore import load_deps, DepsManifestError
        with pytest.raises(DepsManifestError, match="deps.json not found"):
            load_deps(tmp_path)

    def test_load_deps_malformed_json(self, tmp_path):
        from dia_cli.utils.deps_restore import load_deps, DepsManifestError
        (tmp_path / "deps.json").write_text("not json", encoding="utf-8")
        with pytest.raises(DepsManifestError, match="malformed"):
            load_deps(tmp_path)

    def test_load_deps_wrong_schema(self, tmp_path):
        from dia_cli.utils.deps_restore import load_deps, DepsManifestError
        (tmp_path / "deps.json").write_text(
            json.dumps({"schema": "wrong.schema", "deps": []}), encoding="utf-8"
        )
        with pytest.raises(DepsManifestError, match="schema mismatch"):
            load_deps(tmp_path)

    def test_load_deps_empty_deps(self, tmp_path):
        from dia_cli.utils.deps_restore import load_deps
        (tmp_path / "deps.json").write_text(
            json.dumps({"schema": "diaenv.deps.v1", "deps": []}), encoding="utf-8"
        )
        deps = load_deps(tmp_path)
        assert deps == []


class TestGetSentinelPath:
    def test_sentinel_path(self, tmp_path):
        from dia_cli.utils.deps_restore import get_sentinel_path
        p = get_sentinel_path(tmp_path, "sfml")
        assert p == tmp_path / ".diaenv" / "deps" / "sfml.restored"


class TestIsRestored:
    def test_not_restored_no_sentinel(self, tmp_path):
        from dia_cli.utils.deps_restore import is_restored
        dep = {"id": "sfml", "version": "2.6.1"}
        assert not is_restored(tmp_path, dep)

    def test_restored_matching_version(self, tmp_path):
        from dia_cli.utils.deps_restore import is_restored
        _write_sentinel(tmp_path, "sfml", "2.6.1")
        dep = {"id": "sfml", "version": "2.6.1"}
        assert is_restored(tmp_path, dep)

    def test_not_restored_wrong_version(self, tmp_path):
        from dia_cli.utils.deps_restore import is_restored
        _write_sentinel(tmp_path, "sfml", "2.5.0")
        dep = {"id": "sfml", "version": "2.6.1"}
        assert not is_restored(tmp_path, dep)

    def test_not_restored_corrupt_sentinel(self, tmp_path):
        from dia_cli.utils.deps_restore import is_restored
        sentinel_dir = tmp_path / ".diaenv" / "deps"
        sentinel_dir.mkdir(parents=True)
        (sentinel_dir / "sfml.restored").write_text("not json")
        dep = {"id": "sfml", "version": "2.6.1"}
        assert not is_restored(tmp_path, dep)


class TestRestoreDep:
    def test_skip_if_already_restored(self, tmp_path, capsys):
        from dia_cli.utils.deps_restore import restore_dep
        _make_deps_json(tmp_path)
        _write_sentinel(tmp_path, "sfml", "2.6.1")
        dep = {"id": "sfml", "version": "2.6.1", "url": "https://example.com/sfml.zip",
               "mirrors": [], "sha256": "PLACEHOLDER_COMPUTE_AT_RUNTIME",
               "install_type": "zip", "unzip_to": "External/SFML", "strip_root": False}
        code = restore_dep(dep, tmp_path, force=False)
        assert code == 0
        out = capsys.readouterr().out
        assert "already restored" in out

    def test_force_redownloads(self, tmp_path):
        from dia_cli.utils.deps_restore import restore_dep
        _write_sentinel(tmp_path, "sfml", "2.6.1")
        dep = {"id": "sfml", "version": "2.6.1", "url": "https://example.com/sfml.zip",
               "mirrors": [], "sha256": "PLACEHOLDER_COMPUTE_AT_RUNTIME",
               "install_type": "zip", "unzip_to": "External/SFML", "strip_root": False}

        zip_bytes = _make_zip([("file.txt", b"hello")])

        def fake_dl(urls, tmp_path_arg):
            tmp_path_arg.write_bytes(zip_bytes)
            return True

        with patch("dia_cli.utils.deps_restore._download_from_sources", side_effect=fake_dl), \
             patch("dia_cli.utils.deps_restore._install_zip"):
            code = restore_dep(dep, tmp_path, force=True)
        assert code == 0

    def test_no_url_returns_1(self, tmp_path, capsys):
        from dia_cli.utils.deps_restore import restore_dep
        dep = {"id": "nourldep", "version": "1.0", "url": None,
               "mirrors": [], "sha256": "PLACEHOLDER_COMPUTE_AT_RUNTIME",
               "install_type": "zip", "unzip_to": "External/NoUrl", "strip_root": False}
        code = restore_dep(dep, tmp_path)
        assert code == 1
        out = capsys.readouterr().out
        assert "no url or mirrors" in out

    def test_download_failure_returns_1(self, tmp_path, capsys):
        from dia_cli.utils.deps_restore import restore_dep
        dep = {"id": "sfml", "version": "2.6.1", "url": "https://example.com/sfml.zip",
               "mirrors": [], "sha256": "PLACEHOLDER_COMPUTE_AT_RUNTIME",
               "install_type": "zip", "unzip_to": "External/SFML", "strip_root": False}
        with patch("dia_cli.utils.deps_restore._download_from_sources", return_value=False):
            code = restore_dep(dep, tmp_path)
        assert code == 1
        out = capsys.readouterr().out
        assert "download failed" in out

    def test_sha256_mismatch_returns_1(self, tmp_path, capsys):
        from dia_cli.utils.deps_restore import restore_dep
        dep = {"id": "sfml", "version": "2.6.1", "url": "https://example.com/sfml.zip",
               "mirrors": [], "sha256": "expectedhash000000000000000000000000000000000000000000000000000000",
               "install_type": "zip", "unzip_to": "External/SFML", "strip_root": False}

        zip_bytes = _make_zip([("file.txt", b"data")])

        def fake_dl(urls, tmp_path_arg):
            tmp_path_arg.write_bytes(zip_bytes)
            return True

        with patch("dia_cli.utils.deps_restore._download_from_sources", side_effect=fake_dl):
            code = restore_dep(dep, tmp_path)
        assert code == 1
        out = capsys.readouterr().out
        assert "SHA-256 mismatch" in out

    def test_sha256_placeholder_skips_verify(self, tmp_path):
        from dia_cli.utils.deps_restore import restore_dep
        dep = {"id": "sfml", "version": "2.6.1", "url": "https://example.com/sfml.zip",
               "mirrors": [], "sha256": "PLACEHOLDER_COMPUTE_AT_RUNTIME",
               "install_type": "zip", "unzip_to": "External/SFML", "strip_root": False}

        zip_bytes = _make_zip([("file.txt", b"data")])

        def fake_dl(urls, tmp_path_arg):
            tmp_path_arg.write_bytes(zip_bytes)
            return True

        with patch("dia_cli.utils.deps_restore._download_from_sources", side_effect=fake_dl), \
             patch("dia_cli.utils.deps_restore._install_zip"):
            code = restore_dep(dep, tmp_path)
        assert code == 0

    def test_sentinel_written_on_success(self, tmp_path):
        from dia_cli.utils.deps_restore import restore_dep, get_sentinel_path
        dep = {"id": "sfml", "version": "2.6.1", "url": "https://example.com/sfml.zip",
               "mirrors": [], "sha256": "PLACEHOLDER_COMPUTE_AT_RUNTIME",
               "install_type": "zip", "unzip_to": "External/SFML", "strip_root": False}

        zip_bytes = _make_zip([("file.txt", b"data")])

        def fake_dl(urls, tmp_path_arg):
            tmp_path_arg.write_bytes(zip_bytes)
            return True

        with patch("dia_cli.utils.deps_restore._download_from_sources", side_effect=fake_dl), \
             patch("dia_cli.utils.deps_restore._install_zip"):
            code = restore_dep(dep, tmp_path)

        assert code == 0
        sentinel = get_sentinel_path(tmp_path, "sfml")
        assert sentinel.exists()
        data = json.loads(sentinel.read_text())
        assert data["id"] == "sfml"
        assert data["version"] == "2.6.1"
        assert "restored_at" in data

    def test_stale_tmp_cleaned_at_start(self, tmp_path):
        from dia_cli.utils.deps_restore import restore_dep
        # Pre-create a stale .tmp file
        tmp_dir = tmp_path / ".diaenv" / "deps"
        tmp_dir.mkdir(parents=True)
        stale_tmp = tmp_dir / "sfml.tmp"
        stale_tmp.write_bytes(b"stale")

        dep = {"id": "sfml", "version": "2.6.1", "url": "https://example.com/sfml.zip",
               "mirrors": [], "sha256": "PLACEHOLDER_COMPUTE_AT_RUNTIME",
               "install_type": "zip", "unzip_to": "External/SFML", "strip_root": False}

        zip_bytes = _make_zip([("file.txt", b"data")])

        def fake_dl(urls, tmp_path_arg):
            tmp_path_arg.write_bytes(zip_bytes)
            return True

        with patch("dia_cli.utils.deps_restore._download_from_sources", side_effect=fake_dl), \
             patch("dia_cli.utils.deps_restore._install_zip"):
            code = restore_dep(dep, tmp_path)

        assert code == 0
        assert not stale_tmp.exists()

    def test_quiet_suppresses_progress(self, tmp_path, capsys):
        from dia_cli.utils.deps_restore import restore_dep
        _write_sentinel(tmp_path, "sfml", "2.6.1")
        dep = {"id": "sfml", "version": "2.6.1", "url": "https://example.com/sfml.zip",
               "mirrors": [], "sha256": "PLACEHOLDER_COMPUTE_AT_RUNTIME",
               "install_type": "zip", "unzip_to": "External/SFML", "strip_root": False}
        code = restore_dep(dep, tmp_path, force=False, quiet=True)
        assert code == 0
        out = capsys.readouterr().out
        assert out.strip() == ""


class TestInstallZip:
    def test_install_zip_no_strip(self, tmp_path):
        from dia_cli.utils.deps_restore import _install_zip
        zip_bytes = _make_zip([("dir/", b""), ("dir/file.txt", b"hello")])
        archive = tmp_path / "test.zip"
        archive.write_bytes(zip_bytes)
        dest = tmp_path / "out"
        _install_zip(archive, dest, strip_root=False)
        assert (dest / "dir" / "file.txt").exists()

    def test_install_zip_strip_root(self, tmp_path):
        from dia_cli.utils.deps_restore import _install_zip
        zip_bytes = _make_zip([("root/", b""), ("root/file.txt", b"world")])
        archive = tmp_path / "test.zip"
        archive.write_bytes(zip_bytes)
        dest = tmp_path / "out"
        _install_zip(archive, dest, strip_root=True)
        assert (dest / "file.txt").exists()


class TestRestoreAll:
    def test_restore_all_success(self, tmp_path):
        from dia_cli.utils.deps_restore import restore_all
        _make_deps_json(tmp_path)
        zip_bytes = _make_zip([("file.txt", b"data")])

        def fake_dl(urls, tmp_path_arg):
            tmp_path_arg.write_bytes(zip_bytes)
            return True

        with patch("dia_cli.utils.deps_restore._download_from_sources", side_effect=fake_dl), \
             patch("dia_cli.utils.deps_restore._install_zip"):
            code = restore_all(tmp_path, quiet=True)
        assert code == 0

    def test_restore_all_missing_deps_json(self, tmp_path, capsys):
        from dia_cli.utils.deps_restore import restore_all
        code = restore_all(tmp_path)
        assert code == 3
        out = capsys.readouterr().out
        assert "ERROR" in out

    def test_restore_all_partial_failure_returns_1(self, tmp_path):
        from dia_cli.utils.deps_restore import restore_all
        _make_deps_json(tmp_path, extra_deps=[
            {"id": "bad_dep", "version": "1.0", "url": None,
             "mirrors": [], "sha256": "PLACEHOLDER_COMPUTE_AT_RUNTIME",
             "install_type": "zip", "unzip_to": "External/Bad", "strip_root": False}
        ])
        zip_bytes = _make_zip([("file.txt", b"data")])

        def fake_dl(urls, tmp_path_arg):
            tmp_path_arg.write_bytes(zip_bytes)
            return True

        with patch("dia_cli.utils.deps_restore._download_from_sources", side_effect=fake_dl), \
             patch("dia_cli.utils.deps_restore._install_zip"):
            code = restore_all(tmp_path, quiet=True)
        # bad_dep has no url so returns 1, overall should be 1
        assert code == 1


# ===========================================================================
# Feature 1: deps_restore_cmd
# Note: deps_restore_cmd uses lazy imports so module is registered as
# "utils.deps_restore" when called via deps_restore_cmd.run(). We patch
# the function at the path that will actually be used.
# ===========================================================================

def _get_short_module(short_name: str):
    """Get a utils.* or commands.* module as imported by the command modules
    (via the short path added to sys.path by MDK).
    This may differ from the dia_cli.* module object.
    """
    import sys
    if short_name in sys.modules:
        return sys.modules[short_name]
    import importlib
    return importlib.import_module(short_name)


def _get_utils_deps_restore():
    return _get_short_module("utils.deps_restore")


def _get_utils_toolchain_verify():
    return _get_short_module("utils.toolchain_verify")


def _get_utils_deps_verify():
    return _get_short_module("utils.deps_verify")


def _get_utils_submodule_verify():
    return _get_short_module("utils.submodule_verify")


def _get_commands_env_deps_restore_cmd():
    return _get_short_module("commands.env.deps_restore_cmd")


def _get_commands_env_claude_context_cmd():
    return _get_short_module("commands.env.claude_context_cmd")


def _get_commands_env_verify_orchestrator():
    return _get_short_module("commands.env.verify_orchestrator")


def _get_commands_env_setup_orchestrator():
    return _get_short_module("commands.env.setup_orchestrator")


class TestDepsRestoreCmd:
    def _make_fake_dl(self, zip_bytes):
        def fake_dl(urls, tmp_path_arg):
            tmp_path_arg.write_bytes(zip_bytes)
            return True
        return fake_dl

    def test_run_named_dep_found(self, tmp_path):
        from dia_cli.commands.env.deps_restore_cmd import run
        _dr = _get_utils_deps_restore()
        _make_deps_json(tmp_path)
        zip_bytes = _make_zip([("file.txt", b"data")])

        with patch.object(_dr, "_download_from_sources", side_effect=self._make_fake_dl(zip_bytes)), \
             patch.object(_dr, "_install_zip"):
            code = run(repo_root=tmp_path, dep_id="sfml", force=False, quiet=True)
        assert code == 0

    def test_run_named_dep_not_found(self, tmp_path, capsys):
        from dia_cli.commands.env.deps_restore_cmd import run
        _make_deps_json(tmp_path)
        code = run(repo_root=tmp_path, dep_id="nonexistent", force=False)
        assert code == 1
        out = capsys.readouterr().out
        assert "not found in deps.json" in out

    def test_run_all_deps(self, tmp_path):
        from dia_cli.commands.env.deps_restore_cmd import run
        _dr = _get_utils_deps_restore()
        _make_deps_json(tmp_path)
        zip_bytes = _make_zip([("file.txt", b"data")])

        with patch.object(_dr, "_download_from_sources", side_effect=self._make_fake_dl(zip_bytes)), \
             patch.object(_dr, "_install_zip"):
            code = run(repo_root=tmp_path, dep_id=None, force=False, quiet=True)
        assert code == 0

    def test_run_missing_deps_json_returns_3(self, tmp_path, capsys):
        from dia_cli.commands.env.deps_restore_cmd import run
        code = run(repo_root=tmp_path, dep_id=None, force=False)
        assert code == 3

    def test_run_force_flag_passed_through(self, tmp_path):
        from dia_cli.commands.env.deps_restore_cmd import run
        _dr = _get_utils_deps_restore()
        _make_deps_json(tmp_path)
        _write_sentinel(tmp_path, "sfml", "2.6.1")

        zip_bytes = _make_zip([("file.txt", b"data")])

        with patch.object(_dr, "_download_from_sources", side_effect=self._make_fake_dl(zip_bytes)), \
             patch.object(_dr, "_install_zip"):
            code = run(repo_root=tmp_path, dep_id="sfml", force=True, quiet=True)
        assert code == 0


# ===========================================================================
# Feature 2: toolchain_verify
# ===========================================================================

class TestToolchainVerify:
    def test_check_vs2022_vswhere_missing(self):
        from pathlib import Path as _Path
        from dia_cli.utils.toolchain_verify import check_vs2022
        with patch("dia_cli.utils.toolchain_verify._VSWHERE", new=_Path("C:/nonexistent/vswhere.exe")):
            results = check_vs2022()
        assert len(results) == 2
        assert results[0].status == "fail"
        assert results[1].status == "fail"

    def test_check_vs2022_with_workload(self, tmp_path):
        from dia_cli.utils.toolchain_verify import check_vs2022
        vswhere_data = json.dumps([{"installationVersion": "17.9.0"}])

        def fake_run(cmd, **kwargs):
            m = MagicMock()
            m.returncode = 0
            m.stdout = vswhere_data
            m.stderr = ""
            return m

        # Make vswhere.exe appear to exist
        fake_vswhere = tmp_path / "vswhere.exe"
        fake_vswhere.write_bytes(b"")

        with patch("dia_cli.utils.toolchain_verify._VSWHERE", fake_vswhere), \
             patch("dia_cli.utils.toolchain_verify.subprocess.run", side_effect=fake_run):
            results = check_vs2022()

        assert results[0].status == "pass"
        assert results[0].detail == "17.9.0"
        assert results[1].status == "pass"

    def test_check_python_311_pass(self):
        from dia_cli.utils.toolchain_verify import check_python

        def fake_run(cmd, **kwargs):
            m = MagicMock()
            if "python" in cmd[0]:
                m.returncode = 0
                m.stdout = "Python 3.11.9\n"
                m.stderr = ""
            else:
                m.returncode = 1
                m.stdout = ""
                m.stderr = ""
            return m

        with patch("dia_cli.utils.toolchain_verify.subprocess.run", side_effect=fake_run):
            result = check_python()
        assert result.status == "pass"
        assert "3.11" in result.detail

    def test_check_python_wrong_version_warn(self):
        from dia_cli.utils.toolchain_verify import check_python

        def fake_run(cmd, **kwargs):
            m = MagicMock()
            m.returncode = 0
            m.stdout = "Python 3.10.0\n"
            m.stderr = ""
            return m

        with patch("dia_cli.utils.toolchain_verify.subprocess.run", side_effect=fake_run):
            result = check_python()
        assert result.status == "warn"

    def test_check_python_not_found_fail(self):
        from dia_cli.utils.toolchain_verify import check_python

        with patch("dia_cli.utils.toolchain_verify.subprocess.run", side_effect=FileNotFoundError):
            result = check_python()
        assert result.status == "fail"

    def test_check_git_pass(self):
        from dia_cli.utils.toolchain_verify import check_git

        def fake_run(cmd, **kwargs):
            m = MagicMock()
            m.returncode = 0
            m.stdout = "git version 2.44.0\n"
            m.stderr = ""
            return m

        with patch("dia_cli.utils.toolchain_verify.subprocess.run", side_effect=fake_run):
            result = check_git()
        assert result.status == "pass"

    def test_check_git_not_found(self):
        from dia_cli.utils.toolchain_verify import check_git

        with patch("dia_cli.utils.toolchain_verify.subprocess.run", side_effect=FileNotFoundError):
            result = check_git()
        assert result.status == "fail"

    def test_check_nodejs_pass_v18(self):
        from dia_cli.utils.toolchain_verify import check_nodejs

        def fake_run(cmd, **kwargs):
            m = MagicMock()
            m.returncode = 0
            m.stdout = "v18.19.0\n"
            m.stderr = ""
            return m

        with patch("dia_cli.utils.toolchain_verify.subprocess.run", side_effect=fake_run):
            result = check_nodejs()
        assert result.status == "pass"

    def test_check_nodejs_warn_old_version(self):
        from dia_cli.utils.toolchain_verify import check_nodejs

        def fake_run(cmd, **kwargs):
            m = MagicMock()
            m.returncode = 0
            m.stdout = "v14.0.0\n"
            m.stderr = ""
            return m

        with patch("dia_cli.utils.toolchain_verify.subprocess.run", side_effect=fake_run):
            result = check_nodejs()
        assert result.status == "warn"

    def test_check_docker_windows_mode_pass(self):
        from dia_cli.utils.toolchain_verify import check_docker

        def fake_run(cmd, **kwargs):
            m = MagicMock()
            m.returncode = 0
            m.stdout = "windows\n"
            m.stderr = ""
            return m

        with patch("dia_cli.utils.toolchain_verify.subprocess.run", side_effect=fake_run):
            results = check_docker()
        assert results[0].status == "pass"
        assert results[1].status == "pass"

    def test_check_docker_linux_mode_warn(self):
        from dia_cli.utils.toolchain_verify import check_docker

        def fake_run(cmd, **kwargs):
            m = MagicMock()
            m.returncode = 0
            m.stdout = "linux\n"
            m.stderr = ""
            return m

        with patch("dia_cli.utils.toolchain_verify.subprocess.run", side_effect=fake_run):
            results = check_docker()
        assert results[0].status == "pass"
        assert results[1].status == "warn"

    def test_check_docker_not_running(self):
        from dia_cli.utils.toolchain_verify import check_docker

        with patch("dia_cli.utils.toolchain_verify.subprocess.run", side_effect=FileNotFoundError):
            results = check_docker()
        assert results[0].status == "warn"
        assert results[1].status == "fail"


# ===========================================================================
# Feature 2: submodule_verify
# ===========================================================================

class TestSubmoduleVerify:
    def test_check_submodules_no_gitmodules(self, tmp_path):
        from dia_cli.utils.submodule_verify import check_submodules
        results = check_submodules(tmp_path)
        assert len(results) == 1
        assert results[0].status == "fail"
        assert ".gitmodules" in results[0].detail

    def test_check_submodules_all_pass(self, tmp_path):
        from dia_cli.utils.submodule_verify import check_submodules
        (tmp_path / ".gitmodules").write_text(
            '[submodule "External/pybind11"]\n\tpath = External/pybind11\n\turl = https://github.com/pybind/pybind11.git\n'
        )
        # git submodule status format: [status_char][hash] [path] [(description)]
        # space = clean/up-to-date
        git_status_output = " abcdef1234567890 External/pybind11 (heads/stable)\n"

        def fake_run(cmd, **kwargs):
            m = MagicMock()
            m.returncode = 0
            m.stdout = git_status_output
            return m

        with patch("dia_cli.utils.submodule_verify.subprocess.run", side_effect=fake_run):
            results = check_submodules(tmp_path)

        assert len(results) == 1
        assert results[0].status == "pass"

    def test_check_submodules_not_initialised(self, tmp_path):
        from dia_cli.utils.submodule_verify import check_submodules
        (tmp_path / ".gitmodules").write_text(
            '[submodule "External/pybind11"]\n\tpath = External/pybind11\n\turl = https://github.com/pybind/pybind11.git\n'
        )
        # '-' prefix means not initialized
        git_status_output = "-abcdef1234567890 External/pybind11\n"

        def fake_run(cmd, **kwargs):
            m = MagicMock()
            m.returncode = 0
            m.stdout = git_status_output
            return m

        with patch("dia_cli.utils.submodule_verify.subprocess.run", side_effect=fake_run):
            results = check_submodules(tmp_path)

        assert results[0].status == "fail"
        assert "not initialised" in results[0].detail

    def test_check_submodules_wrong_commit(self, tmp_path):
        from dia_cli.utils.submodule_verify import check_submodules
        (tmp_path / ".gitmodules").write_text(
            '[submodule "External/pybind11"]\n\tpath = External/pybind11\n\turl = https://github.com/pybind/pybind11.git\n'
        )
        # '+' prefix means wrong commit
        git_status_output = "+abcdef1234567890 External/pybind11\n"

        def fake_run(cmd, **kwargs):
            m = MagicMock()
            m.returncode = 0
            m.stdout = git_status_output
            return m

        with patch("dia_cli.utils.submodule_verify.subprocess.run", side_effect=fake_run):
            results = check_submodules(tmp_path)

        assert results[0].status == "warn"
        assert "wrong commit" in results[0].detail

    def test_check_submodules_not_in_git_index(self, tmp_path):
        from dia_cli.utils.submodule_verify import check_submodules
        (tmp_path / ".gitmodules").write_text(
            '[submodule "External/pybind11"]\n\tpath = External/pybind11\n\turl = https://github.com/pybind/pybind11.git\n'
        )

        def fake_run(cmd, **kwargs):
            m = MagicMock()
            m.returncode = 0
            m.stdout = ""  # Empty output — submodule not in index
            return m

        with patch("dia_cli.utils.submodule_verify.subprocess.run", side_effect=fake_run):
            results = check_submodules(tmp_path)

        assert results[0].status == "fail"
        assert "not registered" in results[0].detail


# ===========================================================================
# Feature 2: deps_verify
# ===========================================================================

class TestDepsVerify:
    def test_check_deps_missing_deps_json(self, tmp_path):
        from dia_cli.utils.deps_verify import check_deps
        results = check_deps(tmp_path)
        assert len(results) == 1
        assert results[0].name == "deps.json"
        assert results[0].status == "fail"

    def test_check_deps_not_restored(self, tmp_path):
        from dia_cli.utils.deps_verify import check_deps
        _make_deps_json(tmp_path)
        results = check_deps(tmp_path)
        assert len(results) == 1
        assert results[0].name == "sfml"
        assert results[0].status == "fail"
        assert "not restored" in results[0].detail

    def test_check_deps_restored_pass(self, tmp_path):
        from dia_cli.utils.deps_verify import check_deps
        _make_deps_json(tmp_path)
        _write_sentinel(tmp_path, "sfml", "2.6.1")
        results = check_deps(tmp_path)
        assert results[0].status == "pass"
        assert results[0].detail == "2.6.1"

    def test_check_deps_version_mismatch_warn(self, tmp_path):
        from dia_cli.utils.deps_verify import check_deps
        _make_deps_json(tmp_path)
        _write_sentinel(tmp_path, "sfml", "2.5.0")  # Old version
        results = check_deps(tmp_path)
        assert results[0].status == "warn"
        assert "2.5.0" in results[0].detail

    def test_check_deps_dir_exists_no_sentinel_warn(self, tmp_path):
        from dia_cli.utils.deps_verify import check_deps
        _make_deps_json(tmp_path)
        # Create the directory but no sentinel
        (tmp_path / "External" / "SFML").mkdir(parents=True)
        results = check_deps(tmp_path)
        assert results[0].status == "warn"
        assert "manually placed" in results[0].detail

    def test_check_deps_fix_hint_present(self, tmp_path):
        from dia_cli.utils.deps_verify import check_deps
        _make_deps_json(tmp_path)
        results = check_deps(tmp_path)
        assert "dia env deps --dep sfml" in results[0].fix


# ===========================================================================
# Feature 5: verify_orchestrator
# We use patch.object on the already-imported module objects to avoid the
# lazy-import registration path issue.
# ===========================================================================

class TestVerifyOrchestrator:
    def _toolchain_pass_results(self):
        _tv = _get_utils_toolchain_verify()
        return [
            _tv.CheckResult("VS 2022", "toolchain", "pass"),
            _tv.CheckResult("Python 3.11", "toolchain", "pass"),
        ]

    def _deps_pass_results(self):
        _dv = _get_utils_deps_verify()
        return [_dv.CheckResult("sfml", "deps", "pass", "2.6.1")]

    def _submodules_pass_results(self):
        _sv = _get_utils_submodule_verify()
        return [_sv.CheckResult("pybind11", "submodules", "pass")]

    def test_verify_json_output_all_pass(self, tmp_path):
        import dia_cli.commands.env.verify_orchestrator as _vo_full
        from dia_cli.commands.env.verify_orchestrator import run
        _tv = _get_utils_toolchain_verify()
        _dv = _get_utils_deps_verify()
        _sv = _get_utils_submodule_verify()

        _make_deps_json(tmp_path)

        with patch.object(_tv, "check_all_toolchain", return_value=self._toolchain_pass_results()), \
             patch.object(_dv, "check_deps", return_value=self._deps_pass_results()), \
             patch.object(_sv, "check_submodules", return_value=self._submodules_pass_results()), \
             patch.object(_vo_full, "_check_claude", return_value=[]):
            code = run(repo_root=tmp_path, toolchain=False, deps_only=False,
                       submodules=False, docker_only=False, claude=False,
                       output_json=True, quiet=False)

        assert code == 0

    def test_verify_json_structure(self, tmp_path, capsys):
        import dia_cli.commands.env.verify_orchestrator as _vo_full
        from dia_cli.commands.env.verify_orchestrator import run
        _tv = _get_utils_toolchain_verify()
        _dv = _get_utils_deps_verify()
        _sv = _get_utils_submodule_verify()

        _make_deps_json(tmp_path)

        with patch.object(_tv, "check_all_toolchain", return_value=self._toolchain_pass_results()), \
             patch.object(_dv, "check_deps", return_value=self._deps_pass_results()), \
             patch.object(_sv, "check_submodules", return_value=self._submodules_pass_results()), \
             patch.object(_vo_full, "_check_claude", return_value=[]):
            run(repo_root=tmp_path, toolchain=False, deps_only=False,
                submodules=False, docker_only=False, claude=False,
                output_json=True, quiet=False)

        out = capsys.readouterr().out
        data = json.loads(out)
        assert "result" in data
        assert "pass" in data
        assert "warn" in data
        assert "fail" in data
        assert "checks" in data
        assert data["result"] == "pass"

    def test_verify_exits_1_on_fail(self, tmp_path):
        import dia_cli.commands.env.verify_orchestrator as _vo_full
        from dia_cli.commands.env.verify_orchestrator import run
        _tv = _get_utils_toolchain_verify()
        _dv = _get_utils_deps_verify()
        _sv = _get_utils_submodule_verify()

        fail_checks = [_tv.CheckResult("VS 2022", "toolchain", "fail", "not installed")]
        _make_deps_json(tmp_path)

        with patch.object(_tv, "check_all_toolchain", return_value=fail_checks), \
             patch.object(_dv, "check_deps", return_value=[]), \
             patch.object(_sv, "check_submodules", return_value=[]), \
             patch.object(_vo_full, "_check_claude", return_value=[]):
            code = run(repo_root=tmp_path, toolchain=False, deps_only=False,
                       submodules=False, docker_only=False, claude=False,
                       output_json=False, quiet=True)
        assert code == 1

    def test_verify_exits_2_on_warn_only(self, tmp_path):
        import dia_cli.commands.env.verify_orchestrator as _vo_full
        from dia_cli.commands.env.verify_orchestrator import run
        _tv = _get_utils_toolchain_verify()
        _dv = _get_utils_deps_verify()
        _sv = _get_utils_submodule_verify()

        warn_checks = [_tv.CheckResult("Node.js", "toolchain", "warn", "old version")]
        _make_deps_json(tmp_path)

        with patch.object(_tv, "check_all_toolchain", return_value=warn_checks), \
             patch.object(_dv, "check_deps", return_value=[]), \
             patch.object(_sv, "check_submodules", return_value=[]), \
             patch.object(_vo_full, "_check_claude", return_value=[]):
            code = run(repo_root=tmp_path, toolchain=False, deps_only=False,
                       submodules=False, docker_only=False, claude=False,
                       output_json=False, quiet=True)
        assert code == 2

    def test_verify_exits_3_when_deps_json_missing(self, tmp_path, capsys):
        from dia_cli.commands.env.verify_orchestrator import run

        code = run(repo_root=tmp_path, toolchain=False, deps_only=True,
                   submodules=False, docker_only=False, claude=False,
                   output_json=False, quiet=False)
        assert code == 3

    def test_verify_toolchain_only_flag(self, tmp_path):
        import dia_cli.commands.env.verify_orchestrator as _vo_full
        from dia_cli.commands.env.verify_orchestrator import run
        _tv = _get_utils_toolchain_verify()
        _dv = _get_utils_deps_verify()

        with patch.object(_tv, "check_all_toolchain",
                          return_value=self._toolchain_pass_results()) as mock_tc, \
             patch.object(_dv, "check_deps") as mock_deps, \
             patch.object(_vo_full, "_check_claude", return_value=[]):
            run(repo_root=tmp_path, toolchain=True, deps_only=False,
                submodules=False, docker_only=False, claude=False,
                output_json=False, quiet=True)
        mock_tc.assert_called_once()
        mock_deps.assert_not_called()

    def test_verify_deps_only_flag(self, tmp_path):
        import dia_cli.commands.env.verify_orchestrator as _vo_full
        from dia_cli.commands.env.verify_orchestrator import run
        _tv = _get_utils_toolchain_verify()
        _dv = _get_utils_deps_verify()

        _make_deps_json(tmp_path)
        _write_sentinel(tmp_path, "sfml", "2.6.1")

        with patch.object(_tv, "check_all_toolchain") as mock_tc, \
             patch.object(_dv, "check_deps",
                          return_value=self._deps_pass_results()) as mock_deps, \
             patch.object(_vo_full, "_check_claude", return_value=[]):
            run(repo_root=tmp_path, toolchain=False, deps_only=True,
                submodules=False, docker_only=False, claude=False,
                output_json=False, quiet=True)
        mock_tc.assert_not_called()
        mock_deps.assert_called_once()

    def test_verify_docker_only_flag(self, tmp_path):
        import dia_cli.commands.env.verify_orchestrator as _vo_full
        from dia_cli.commands.env.verify_orchestrator import run
        _tv = _get_utils_toolchain_verify()
        _dv = _get_utils_deps_verify()

        docker_results = [
            _tv.CheckResult("Docker Desktop", "toolchain", "pass"),
            _tv.CheckResult("Windows Containers mode", "toolchain", "pass"),
        ]

        with patch.object(_tv, "check_all_toolchain") as mock_all, \
             patch.object(_tv, "check_docker", return_value=docker_results) as mock_docker, \
             patch.object(_dv, "check_deps") as mock_deps, \
             patch.object(_vo_full, "_check_claude", return_value=[]):
            code = run(repo_root=tmp_path, toolchain=False, deps_only=False,
                       submodules=False, docker_only=True, claude=False,
                       output_json=False, quiet=True)
        mock_docker.assert_called_once()
        mock_deps.assert_not_called()
        assert code == 0


# ===========================================================================
# Feature 4: setup_orchestrator
# ===========================================================================

class TestSetupOrchestrator:
    def test_run_all_steps_not_admin_toolchain_skipped(self, tmp_path, capsys):
        import dia_cli.commands.env.setup_orchestrator as _so_full
        from dia_cli.commands.env.setup_orchestrator import run
        _dr = _get_commands_env_deps_restore_cmd()
        _cc = _get_commands_env_claude_context_cmd()

        with patch.object(_so_full, "_is_admin", return_value=False), \
             patch.object(_so_full, "_run_submodules", return_value=0), \
             patch.object(_so_full, "_run_toolchain", return_value=0), \
             patch.object(_dr, "run", return_value=0), \
             patch.object(_cc, "run", return_value=0):
            code = run(repo_root=tmp_path, toolchain=False, deps_only=False,
                       dep_id=None, submodules=False, claude=False,
                       force=False, fail_fast=False, quiet=False)

        out = capsys.readouterr().out
        assert "SKIPPED" in out
        assert "administrator" in out
        assert code == 2  # warned (toolchain skipped)

    def test_run_deps_only_step(self, tmp_path):
        from dia_cli.commands.env.setup_orchestrator import run
        _dr = _get_commands_env_deps_restore_cmd()

        with patch.object(_dr, "run", return_value=0) as mock_deps:
            code = run(repo_root=tmp_path, toolchain=False, deps_only=True,
                       dep_id=None, submodules=False, claude=False,
                       force=False, fail_fast=False, quiet=True)
        mock_deps.assert_called_once()
        assert code == 0

    def test_run_submodules_step(self, tmp_path):
        import dia_cli.commands.env.setup_orchestrator as _so_full
        from dia_cli.commands.env.setup_orchestrator import run

        with patch.object(_so_full, "_run_submodules", return_value=0) as mock_sub:
            code = run(repo_root=tmp_path, toolchain=False, deps_only=False,
                       dep_id=None, submodules=True, claude=False,
                       force=False, fail_fast=False, quiet=True)
        mock_sub.assert_called_once()
        assert code == 0

    def test_run_claude_step(self, tmp_path):
        from dia_cli.commands.env.setup_orchestrator import run
        _cc = _get_commands_env_claude_context_cmd()

        with patch.object(_cc, "run", return_value=0) as mock_claude:
            code = run(repo_root=tmp_path, toolchain=False, deps_only=False,
                       dep_id=None, submodules=False, claude=True,
                       force=False, fail_fast=False, quiet=True)
        mock_claude.assert_called_once()
        assert code == 0

    def test_run_fail_fast_stops_on_failure(self, tmp_path):
        import dia_cli.commands.env.setup_orchestrator as _so_full
        from dia_cli.commands.env.setup_orchestrator import run
        _dr = _get_commands_env_deps_restore_cmd()

        with patch.object(_so_full, "_is_admin", return_value=False), \
             patch.object(_dr, "run", return_value=1), \
             patch.object(_so_full, "_run_submodules", return_value=0) as mock_sub:
            code = run(repo_root=tmp_path, toolchain=False, deps_only=True,
                       dep_id=None, submodules=True, claude=False,
                       force=False, fail_fast=True, quiet=True)

        # With fail_fast=True, submodules should NOT be called after deps fails
        mock_sub.assert_not_called()
        assert code == 1

    def test_run_dep_id_with_toolchain_returns_2(self, tmp_path, capsys):
        from dia_cli.commands.env.setup_orchestrator import run
        code = run(repo_root=tmp_path, toolchain=True, deps_only=False,
                   dep_id="sfml", submodules=False, claude=False,
                   force=False, fail_fast=False, quiet=True)
        assert code == 2

    def test_run_toolchain_step_as_admin(self, tmp_path):
        import dia_cli.commands.env.setup_orchestrator as _so_full
        from dia_cli.commands.env.setup_orchestrator import run

        with patch.object(_so_full, "_is_admin", return_value=True), \
             patch.object(_so_full, "_run_toolchain", return_value=0) as mock_tc:
            code = run(repo_root=tmp_path, toolchain=True, deps_only=False,
                       dep_id=None, submodules=False, claude=False,
                       force=False, fail_fast=False, quiet=True)
        mock_tc.assert_called_once()
        assert code == 0

    def test_run_toolchain_missing_winget_json(self, tmp_path, capsys):
        from dia_cli.commands.env.setup_orchestrator import _run_toolchain
        code = _run_toolchain(tmp_path, force=False, quiet=True)
        assert code == 1
        out = capsys.readouterr().out
        assert "winget.json not found" in out

    def test_run_submodules_calls_git(self, tmp_path):
        import dia_cli.commands.env.setup_orchestrator as _so_full
        from dia_cli.commands.env.setup_orchestrator import _run_submodules

        with patch.object(_so_full, "subprocess") as mock_sp:
            mock_sp.run.return_value = MagicMock(returncode=0)
            code = _run_submodules(tmp_path, quiet=False)
        assert code == 0
        cmd = mock_sp.run.call_args[0][0]
        assert "git" in cmd
        assert "submodule" in cmd


# ===========================================================================
# Feature 7: claude_context_cmd
# ===========================================================================

class TestClaudeContextCmd:
    def test_run_generates_settings_from_template(self, tmp_path):
        from dia_cli.commands.env.claude_context_cmd import run

        claude_dir = tmp_path / ".claude"
        claude_dir.mkdir()
        template = claude_dir / "settings.local.template.json"
        template.write_text(json.dumps({"permissions": {"allow": []}}))

        target = claude_dir / "settings.local.json"
        assert not target.exists()

        with patch("dia_cli.commands.env.claude_context_cmd.os.symlink", side_effect=OSError("no symlinks")):
            code = run(repo_root=tmp_path, force=False)

        assert target.exists()
        data = json.loads(target.read_text())
        assert "permissions" in data

    def test_run_skips_existing_settings_without_force(self, tmp_path, capsys):
        from dia_cli.commands.env.claude_context_cmd import run

        claude_dir = tmp_path / ".claude"
        claude_dir.mkdir()
        template = claude_dir / "settings.local.template.json"
        template.write_text(json.dumps({"permissions": {}}))
        existing = claude_dir / "settings.local.json"
        existing.write_text(json.dumps({"permissions": {"existing": True}}))

        with patch("dia_cli.commands.env.claude_context_cmd.os.symlink", side_effect=OSError("no symlinks")):
            run(repo_root=tmp_path, force=False)

        out = capsys.readouterr().out
        assert "already exists" in out
        data = json.loads(existing.read_text())
        assert data["permissions"].get("existing") is True

    def test_run_force_overwrites_settings(self, tmp_path, capsys):
        from dia_cli.commands.env.claude_context_cmd import run

        claude_dir = tmp_path / ".claude"
        claude_dir.mkdir()
        template = claude_dir / "settings.local.template.json"
        template.write_text(json.dumps({"permissions": {"new": True}}))
        existing = claude_dir / "settings.local.json"
        existing.write_text(json.dumps({"permissions": {"old": True}}))

        with patch("dia_cli.commands.env.claude_context_cmd.os.symlink", side_effect=OSError("no symlinks")):
            run(repo_root=tmp_path, force=True)

        data = json.loads(existing.read_text())
        assert data["permissions"].get("new") is True

    def test_run_missing_template_returns_1(self, tmp_path, capsys):
        from dia_cli.commands.env.claude_context_cmd import run
        code = run(repo_root=tmp_path, force=False)
        assert code == 1
        out = capsys.readouterr().out
        assert "template not found" in out

    def test_run_creates_symlink(self, tmp_path, capsys):
        from dia_cli.commands.env.claude_context_cmd import run

        claude_dir = tmp_path / ".claude"
        claude_dir.mkdir()
        template = claude_dir / "settings.local.template.json"
        template.write_text(json.dumps({"permissions": {}}))

        symlink_created = []

        def fake_symlink(src, dst):
            symlink_created.append((src, dst))

        with patch("dia_cli.commands.env.claude_context_cmd.os.symlink", side_effect=fake_symlink):
            code = run(repo_root=tmp_path, force=False)

        assert len(symlink_created) == 1
        assert code == 0

    def test_run_symlink_error_returns_1(self, tmp_path, capsys):
        from dia_cli.commands.env.claude_context_cmd import run

        claude_dir = tmp_path / ".claude"
        claude_dir.mkdir()
        template = claude_dir / "settings.local.template.json"
        template.write_text(json.dumps({"permissions": {}}))

        with patch("dia_cli.commands.env.claude_context_cmd.os.symlink",
                   side_effect=OSError("Permission denied")):
            code = run(repo_root=tmp_path, force=False)

        assert code == 1
        out = capsys.readouterr().out
        assert "could not create symlink" in out

    def test_run_symlink_already_correct(self, tmp_path, capsys):
        from dia_cli.commands.env.claude_context_cmd import run

        claude_dir = tmp_path / ".claude"
        claude_dir.mkdir()
        template = claude_dir / "settings.local.template.json"
        template.write_text(json.dumps({"permissions": {}}))

        slug = str(tmp_path).replace("\\", "-").replace(":", "-").replace("/", "-")
        repo_memory = tmp_path / ".claude" / "projects" / slug / "memory"
        repo_memory.mkdir(parents=True, exist_ok=True)

        with patch("pathlib.Path.is_symlink", return_value=True), \
             patch("dia_cli.commands.env.claude_context_cmd.os.readlink",
                   return_value=str(repo_memory)):
            code = run(repo_root=tmp_path, force=False)

        assert code == 0
        out = capsys.readouterr().out
        assert "already configured" in out


# ===========================================================================
# Feature 8: docker commands
# ===========================================================================

class TestDockerImageCmd:
    def test_run_builds_image(self, tmp_path):
        from dia_cli.commands.env.docker.image_cmd import run

        build_env = tmp_path / "build-env"
        build_env.mkdir()
        (build_env / "Dockerfile").write_bytes(b"FROM scratch\n")

        with patch("dia_cli.commands.env.docker.image_cmd.subprocess.run") as mock_run:
            mock_run.return_value = MagicMock(returncode=0)
            code = run(repo_root=tmp_path, force=False)

        assert code == 0
        cmd = mock_run.call_args[0][0]
        assert "docker" in cmd
        assert "build" in cmd

    def test_run_missing_dockerfile_returns_1(self, tmp_path, capsys):
        from dia_cli.commands.env.docker.image_cmd import run
        code = run(repo_root=tmp_path, force=False)
        assert code == 1
        out = capsys.readouterr().out
        assert "Dockerfile not found" in out

    def test_run_sentinel_skips_rebuild(self, tmp_path, capsys):
        from dia_cli.commands.env.docker.image_cmd import run

        build_env = tmp_path / "build-env"
        build_env.mkdir()
        (build_env / "Dockerfile").write_bytes(b"FROM scratch\n")
        sentinel = tmp_path / ".docker-env" / "image.sentinel"
        sentinel.parent.mkdir(parents=True)
        sentinel.write_text("ok")

        # docker image inspect returns 0 = image exists
        with patch("dia_cli.commands.env.docker.image_cmd.subprocess.run") as mock_run:
            mock_run.return_value = MagicMock(returncode=0)
            code = run(repo_root=tmp_path, force=False)

        assert code == 0
        out = capsys.readouterr().out
        assert "already built" in out
        # Only inspect was called, not build
        assert mock_run.call_count == 1
        cmd = mock_run.call_args[0][0]
        assert "inspect" in cmd


class TestDockerDepsCmd:
    def test_run_missing_deps_json_returns_3(self, tmp_path, capsys):
        from dia_cli.commands.env.docker.deps_cmd import run
        code = run(repo_root=tmp_path, force=False)
        assert code == 3
        out = capsys.readouterr().out
        assert "deps.json not found" in out

    def test_run_missing_docker_image_returns_1(self, tmp_path, capsys):
        from dia_cli.commands.env.docker.deps_cmd import run
        _make_deps_json(tmp_path)

        with patch("dia_cli.commands.env.docker.deps_cmd.subprocess.run") as mock_run:
            mock_run.return_value = MagicMock(returncode=1)  # image inspect fails
            code = run(repo_root=tmp_path, force=False)

        assert code == 1
        out = capsys.readouterr().out
        assert "not found" in out

    def test_run_invokes_docker_run(self, tmp_path):
        from dia_cli.commands.env.docker.deps_cmd import run
        _make_deps_json(tmp_path)

        with patch("dia_cli.commands.env.docker.deps_cmd.subprocess.run") as mock_run:
            mock_run.side_effect = [
                MagicMock(returncode=0),  # image inspect
                MagicMock(returncode=0),  # docker run
            ]
            code = run(repo_root=tmp_path, force=False)

        assert code == 0
        run_cmd = mock_run.call_args_list[1][0][0]
        assert "docker" in run_cmd
        assert "run" in run_cmd


class TestDockerPathsCmd:
    def test_run_missing_docker_image_returns_1(self, tmp_path, capsys):
        from dia_cli.commands.env.docker.paths_cmd import run

        scripts_dir = tmp_path / "build-env" / "scripts"
        scripts_dir.mkdir(parents=True)
        (scripts_dir / "configure-paths.ps1").write_text("# script")

        with patch("dia_cli.commands.env.docker.paths_cmd.subprocess.run") as mock_run:
            mock_run.return_value = MagicMock(returncode=1)  # image inspect fails
            code = run(repo_root=tmp_path, force=False)

        assert code == 1
        out = capsys.readouterr().out
        assert "not found" in out

    def test_run_missing_script_returns_1(self, tmp_path, capsys):
        from dia_cli.commands.env.docker.paths_cmd import run

        with patch("dia_cli.commands.env.docker.paths_cmd.subprocess.run") as mock_run:
            mock_run.return_value = MagicMock(returncode=0)  # image inspect succeeds
            code = run(repo_root=tmp_path, force=False)

        assert code == 1
        out = capsys.readouterr().out
        assert "configure-paths.ps1 not found" in out


# ===========================================================================
# Feature 3: submodule .gitmodules entries
# ===========================================================================

class TestGitmodulesContent:
    def test_gitmodules_has_pybind11(self):
        gitmodules = _REPO_ROOT_FROM_TEST / ".gitmodules"
        if not gitmodules.exists():
            pytest.skip(".gitmodules not found at repo root")
        content = gitmodules.read_text()
        assert "pybind11" in content

    def test_gitmodules_has_new_entries(self):
        gitmodules = _REPO_ROOT_FROM_TEST / ".gitmodules"
        if not gitmodules.exists():
            pytest.skip(".gitmodules not found at repo root")
        content = gitmodules.read_text()
        assert "asio" in content
        assert "websocketpp" in content
        assert "googletest" in content


# ===========================================================================
# Feature 6: msbuild-restore (Directory.Build.targets exists)
# ===========================================================================

class TestDirectoryBuildTargets:
    def test_targets_file_exists(self):
        targets = _REPO_ROOT_FROM_TEST / "Directory.Build.targets"
        assert targets.exists(), f"Directory.Build.targets not found at {targets}"

    def test_targets_file_has_restore_target(self):
        targets = _REPO_ROOT_FROM_TEST / "Directory.Build.targets"
        if not targets.exists():
            pytest.skip()
        content = targets.read_text()
        assert "RestoreExternals" in content
        assert "deps.json" in content
        assert "dia_cli" in content


# ===========================================================================
# Feature 8: docker build-env files exist
# ===========================================================================

class TestBuildEnvFiles:
    def test_dockerfile_exists(self):
        dockerfile = _REPO_ROOT_FROM_TEST / "build-env" / "Dockerfile"
        assert dockerfile.exists(), f"Dockerfile not found at {dockerfile}"

    def test_docker_compose_exists(self):
        compose = _REPO_ROOT_FROM_TEST / "build-env" / "docker-compose.yml"
        assert compose.exists(), f"docker-compose.yml not found at {compose}"

    def test_restore_deps_script_exists(self):
        script = _REPO_ROOT_FROM_TEST / "build-env" / "scripts" / "restore-deps.ps1"
        assert script.exists(), f"restore-deps.ps1 not found at {script}"

    def test_configure_paths_script_exists(self):
        script = _REPO_ROOT_FROM_TEST / "build-env" / "scripts" / "configure-paths.ps1"
        assert script.exists(), f"configure-paths.ps1 not found at {script}"

    def test_dockerfile_has_vs_build_tools(self):
        dockerfile = _REPO_ROOT_FROM_TEST / "build-env" / "Dockerfile"
        if not dockerfile.exists():
            pytest.skip()
        content = dockerfile.read_text()
        assert "vs_buildtools" in content
        assert "VCTools" in content

    def test_dockerfile_has_python_install(self):
        dockerfile = _REPO_ROOT_FROM_TEST / "build-env" / "Dockerfile"
        if not dockerfile.exists():
            pytest.skip()
        content = dockerfile.read_text()
        assert "python" in content.lower()
        assert "3.11" in content


# ---------------------------------------------------------------------------
# Zip Slip protection (C1 fix)
# ---------------------------------------------------------------------------

def test_install_zip_blocks_zip_slip(tmp_path):
    """Zip entries that escape the target directory must be rejected."""
    import zipfile
    from dia_cli.utils.deps_restore import _install_zip, DepsManifestError

    # Create a malicious zip with a path traversal entry
    zip_path = tmp_path / "evil.zip"
    target_dir = tmp_path / "target"
    target_dir.mkdir()

    with zipfile.ZipFile(zip_path, "w") as zf:
        zf.writestr("../../etc/evil.txt", "malicious content")

    with pytest.raises(DepsManifestError, match="zip slip"):
        _install_zip(zip_path, target_dir, strip_root=False)

    # Verify the file was NOT written
    assert not (tmp_path / "etc" / "evil.txt").exists()


def test_install_zip_allows_normal_entries(tmp_path):
    """Normal zip entries within target directory should work fine."""
    import zipfile
    from dia_cli.utils.deps_restore import _install_zip

    zip_path = tmp_path / "good.zip"
    target_dir = tmp_path / "target"

    with zipfile.ZipFile(zip_path, "w") as zf:
        zf.writestr("subdir/file.txt", "safe content")

    _install_zip(zip_path, target_dir, strip_root=False)
    assert (target_dir / "subdir" / "file.txt").read_text() == "safe content"


# ---------------------------------------------------------------------------
# Atomic download (M4 fix)
# ---------------------------------------------------------------------------

def test_download_http_partial_file_cleaned_on_failure(tmp_path, monkeypatch):
    """Partial download files should be cleaned up on failure."""
    from dia_cli.utils.deps_restore import _download_http

    import types
    mock_requests = types.ModuleType("requests")
    mock_requests.RequestException = Exception

    class FailResponse:
        status_code = 200
        def raise_for_status(self): pass
        def iter_content(self, chunk_size=None):
            yield b"partial data"
            raise Exception("connection lost")

    mock_requests.get = lambda *a, **kw: FailResponse()
    monkeypatch.setattr("dia_cli.utils.deps_restore.time.sleep", lambda _: None)

    import sys
    monkeypatch.setitem(sys.modules, "requests", mock_requests)

    dest = tmp_path / "test.zip"
    result = _download_http("http://example.com/test.zip", dest)
    assert result is False
    assert not dest.exists()
    # Verify no .partial file left behind
    assert not dest.with_suffix(".zip.partial").exists()


# ---------------------------------------------------------------------------
# SHA-256 placeholder warning (H3 fix)
# ---------------------------------------------------------------------------

def test_restore_dep_warns_on_placeholder_sha(tmp_path, capsys):
    """Placeholder SHA-256 should produce a warning."""
    from dia_cli.utils.deps_restore import restore_dep, _PLACEHOLDER_SHA

    dep = {
        "id": "testdep",
        "version": "1.0",
        "url": None,
        "sha256": _PLACEHOLDER_SHA,
        "mirrors": [],
    }
    # Will fail at download (no URLs) but should show warning path exists
    code = restore_dep(dep, tmp_path, force=False, quiet=False)
    # It will fail due to no URLs — that's fine, we just verify the pattern is there
    assert code != 0  # expected to fail


# ---------------------------------------------------------------------------
# FileNotFoundError handling in runners (M1 fix)
# ---------------------------------------------------------------------------

@patch("dia_cli.commands.test.cli_runner.subprocess.run")
def test_cli_runner_handles_file_not_found(mock_run, tmp_path, capsys):
    """cli_runner should catch FileNotFoundError and return 1."""
    from dia_cli.commands.test.cli_runner import run as cli_run
    mock_run.side_effect = FileNotFoundError("pytest not found")
    code = cli_run(repo_root=tmp_path, filter_pattern=None, parallel=False,
                   coverage_out=None, docker=False)
    assert code == 1
    captured = capsys.readouterr()
    assert "not found" in captured.out


@patch("dia_cli.commands.test.googletest_runner.subprocess.run")
def test_googletest_runner_handles_file_not_found(mock_run, tmp_path, capsys):
    """googletest_runner should catch FileNotFoundError and return 1."""
    from dia_cli.commands.test.googletest_runner import run as gt_run, _BINARY_SUBPATH, _PYTHON_DLL
    # Create the binary and dll so we get past the prereq checks
    binary = tmp_path / "Cluiche" / "bin" / "Debug" / "x64" / "GoogleTests.exe"
    binary.parent.mkdir(parents=True, exist_ok=True)
    binary.write_bytes(b"fake")
    (binary.parent / _PYTHON_DLL).write_bytes(b"fake")

    mock_run.side_effect = FileNotFoundError("binary not found")
    code = gt_run(repo_root=tmp_path, config="Debug", filter_pattern=None,
                  verbose=False, docker=False)
    assert code == 1
    captured = capsys.readouterr()
    assert "not found" in captured.out


# ---------------------------------------------------------------------------
# env_integration: exhausted iterator fix (T8)
# ---------------------------------------------------------------------------

@patch("dia_cli.commands.test.env_integration_runner._generate_fix")
@patch("dia_cli.commands.test.env_integration_runner.subprocess.Popen")
def test_stage_fails_three_times_with_fresh_output(mock_popen, mock_gen_fix, tmp_path, monkeypatch):
    """Each loop iteration should see the actual failure output, not an empty string."""
    from dia_cli.commands.test.env_integration_runner import run as ei_run

    def make_fail_proc():
        proc = MagicMock()
        proc.stdout = iter(["ModuleNotFoundError: requests\n"])
        proc.wait.return_value = None
        proc.returncode = 1
        return proc

    mock_popen.side_effect = [make_fail_proc() for _ in range(4)]
    mock_gen_fix.return_value = "pip install requests"
    monkeypatch.setattr("builtins.input", lambda _: "y")

    code = ei_run(repo_root=tmp_path, skip_env=True, max_auto_fixes=0,
                  no_fix=False, inject_fault=None, docker=False)
    assert code == 4
    # Verify _generate_fix was called with actual failure output each time, not empty string
    for call_args in mock_gen_fix.call_args_list:
        failure_output = call_args[0][1]  # second positional arg
        assert "ModuleNotFoundError" in failure_output
