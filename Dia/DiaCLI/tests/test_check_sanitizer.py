"""Tests for `dia check sanitizer` result reporting."""
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

from click.testing import CliRunner

from dia_cli.cli import check as check_mod

GT_PASS = "[==========] 5 tests ran.\n[  PASSED  ] 5 tests.\n"


def _invoke(tmp_path, returncode, stdout="", stderr=""):
    exe = tmp_path / "Cluiche" / "bin" / "GoogleTests" / "Debug-Asan" / "x64" / "GoogleTests.exe"
    exe.parent.mkdir(parents=True)
    exe.write_bytes(b"")

    def fake_run(cmd, **kw):
        if cmd[0] == "dia":
            return SimpleNamespace(returncode=0)
        return SimpleNamespace(returncode=returncode, stdout=stdout, stderr=stderr)

    with patch.object(check_mod, "find_repo_root", return_value=tmp_path), \
         patch.object(check_mod.subprocess, "run", side_effect=fake_run):
        res = CliRunner().invoke(check_mod.cli, ["sanitizer"])
    out_file = tmp_path / "Cluiche" / "out" / "check" / "sanitizer-asan.txt"
    return res, out_file


def test_clean_pass(tmp_path):
    res, f = _invoke(tmp_path, 0, stdout=GT_PASS)
    assert res.exit_code == 0
    assert "No sanitizer findings detected." in res.output
    assert "[  PASSED  ] 5 tests" in res.output
    assert "PASSED" in f.read_text()


def test_dll_not_found_is_failure(tmp_path):
    res, f = _invoke(tmp_path, 3221225781)
    assert res.exit_code != 0
    assert "FAILED" in res.output
    assert "STATUS_DLL_NOT_FOUND" in res.output
    assert "0xC0000135" in res.output
    assert "No sanitizer findings detected." not in res.output
    assert "exit" in f.read_text().splitlines()[0].lower()


def test_negative_code_rendered_unsigned(tmp_path):
    res, _ = _invoke(tmp_path, -1073741819)
    assert "0xC0000005" in res.output
    assert res.exit_code != 0


def test_finding_counted(tmp_path):
    err = "==123==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x1\n"
    res, f = _invoke(tmp_path, 1, stderr=err)
    assert res.exit_code != 0
    assert "1 sanitizer finding" in res.output
    assert "heap-buffer-overflow" in res.output
    assert "No sanitizer findings detected." not in res.output
    assert "heap-buffer-overflow" in f.read_text()


def test_exit_zero_without_gtest_summary_not_clean(tmp_path):
    res, _ = _invoke(tmp_path, 0, stdout="hello\n")
    assert res.exit_code != 0
    assert "No sanitizer findings detected." not in res.output
