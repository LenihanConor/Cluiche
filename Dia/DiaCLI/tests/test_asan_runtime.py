"""Tests for dia_cli.utils.asan_runtime."""
import os
from pathlib import Path
from unittest.mock import patch

from dia_cli.utils import asan_runtime
from dia_cli.utils.asan_runtime import (
    find_asan_runtime_dir, with_asan_runtime_on_path,
)

DBG = "clang_rt.asan_dbg_dynamic-x86_64.dll"
REL = "clang_rt.asan_dynamic-x86_64.dll"


def _make(root: Path, version: str, *dlls: str) -> Path:
    d = root / "VC" / "Tools" / "MSVC" / version / "bin" / "Hostx64" / "x64"
    d.mkdir(parents=True)
    for dll in dlls:
        (d / dll).write_bytes(b"")
    return d


def _vs(tmp_path):
    return patch.object(asan_runtime, "_vs_install_path", return_value=tmp_path)


def test_finds_newest_version_with_dll(tmp_path):
    _make(tmp_path, "14.40.1", DBG)
    new = _make(tmp_path, "14.51.36231", DBG)
    _make(tmp_path, "14.60.0")  # newest but no DLL
    with _vs(tmp_path):
        assert find_asan_runtime_dir() == new


def test_version_sort_is_numeric(tmp_path):
    _make(tmp_path, "14.9.1", DBG)
    new = _make(tmp_path, "14.10.1", DBG)
    with _vs(tmp_path):
        assert find_asan_runtime_dir() == new


def test_none_when_absent(tmp_path):
    _make(tmp_path, "14.51.1")
    with _vs(tmp_path):
        assert find_asan_runtime_dir() is None


def test_none_when_no_vs():
    with patch.object(asan_runtime, "_vs_install_path", return_value=None):
        assert find_asan_runtime_dir() is None


def test_dbg_vs_release_dll(tmp_path):
    d = _make(tmp_path, "14.51.1", REL)
    with _vs(tmp_path):
        assert find_asan_runtime_dir(debug_crt=True) is None
        assert find_asan_runtime_dir(debug_crt=False) == d


def test_env_prepends_for_asan_only(tmp_path):
    d = _make(tmp_path, "14.51.1", DBG)
    base = {"PATH": "PATHX"}
    with _vs(tmp_path):
        out = with_asan_runtime_on_path(base, "Debug-Asan")
        assert out["PATH"] == str(d) + os.pathsep + "PATHX"
        assert base["PATH"] == "PATHX"
        for cfg in ("Debug", "Release", "Debug-Ubsan"):
            assert with_asan_runtime_on_path(base, cfg)["PATH"] == "PATHX"


def test_env_defaults_to_os_environ(tmp_path):
    with patch.dict(os.environ, {"PATH": "P"}):
        assert with_asan_runtime_on_path(None, "Debug")["PATH"] == "P"


def test_env_warns_when_missing(tmp_path, capsys):
    with patch.object(asan_runtime, "find_asan_runtime_dir", return_value=None):
        out = with_asan_runtime_on_path({"PATH": "P"}, "Debug-Asan")
    assert out["PATH"] == "P"
    assert DBG in capsys.readouterr().err
