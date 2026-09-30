"""Tests for dia_cli/cli/console.py (console-desktop-install.md).

``dia console`` was a bare ``@click.command()`` before this feature; it is
now a ``@click.group(invoke_without_command=True)`` with two new
subcommands (``build``, ``install-shortcut``). The one hard regression risk
is Goal 4/Task 8: ``dia console`` with *no* subcommand must keep calling
``launch()`` exactly as before.

Nothing here ever actually invokes PyInstaller or touches a real Desktop
``.lnk`` -- ``build``/``install-shortcut``'s Click wrappers are tested with
their injectable dependencies mocked (same DI pattern as
``dia_cli/cli/preset.py``'s ``run_preset``); the one real end-to-end
PyInstaller build is marked ``@pytest.mark.integration`` and is not part of
the default ``pytest`` run.
"""
from __future__ import annotations

import io
import json
import subprocess
import sys
import time
import urllib.request

import click
import pytest
from click.testing import CliRunner

from dia_cli.cli import console as console_module
from dia_console.packaging import entrypoint as entrypoint_module


# ===========================================================================
# `dia console` with no subcommand -- regression guard (Task 8)
# ===========================================================================

def test_no_subcommand_still_calls_launch(monkeypatch):
    called = {"value": False}

    def fake_launch():
        called["value"] = True

    monkeypatch.setattr("dia_console.shell.launch", fake_launch)

    runner = CliRunner()
    result = runner.invoke(console_module.cli, [])

    assert result.exit_code == 0, result.output
    assert called["value"] is True


def test_no_subcommand_does_not_touch_build_or_install_shortcut(monkeypatch):
    """A no-subcommand invocation must not accidentally reach the new subcommands."""
    build_called = {"value": False}
    install_called = {"value": False}
    monkeypatch.setattr("dia_console.shell.launch", lambda: None)
    monkeypatch.setattr(console_module, "run_build", lambda repo_root: build_called.update(value=True) or 0)
    monkeypatch.setattr(
        console_module, "create_desktop_shortcut",
        lambda repo_root: install_called.update(value=True) or None,
    )

    runner = CliRunner()
    result = runner.invoke(console_module.cli, [])

    assert result.exit_code == 0, result.output
    assert build_called["value"] is False
    assert install_called["value"] is False


# ===========================================================================
# pyinstaller_argv_prefix -- mirrors execution.py's default_argv_prefix
# ===========================================================================

def test_pyinstaller_argv_prefix_prefers_installed_launcher(monkeypatch):
    monkeypatch.setattr(
        console_module.shutil, "which",
        lambda name: r"C:\fake\pyinstaller.exe" if name == "pyinstaller" else None,
    )
    assert console_module.pyinstaller_argv_prefix() == [r"C:\fake\pyinstaller.exe"]


def test_pyinstaller_argv_prefix_falls_back_to_module_invocation(monkeypatch):
    monkeypatch.setattr(console_module.shutil, "which", lambda name: None)
    assert console_module.pyinstaller_argv_prefix() == [sys.executable, "-m", "PyInstaller"]


# ===========================================================================
# build_pyinstaller_argv (Task 9)
# ===========================================================================

def test_build_pyinstaller_argv_includes_spec_file_and_flags(monkeypatch, tmp_path):
    monkeypatch.setattr(console_module, "pyinstaller_argv_prefix", lambda: ["pyinstaller"])
    argv = console_module.build_pyinstaller_argv(tmp_path)

    assert argv[0] == "pyinstaller"
    assert "DiaConsole.spec" in argv
    assert "--noconfirm" in argv

    out_dir = tmp_path / "Cluiche" / "out" / "DiaCLI" / "DiaConsole"
    assert argv[argv.index("--distpath") + 1] == str(out_dir)
    assert argv[argv.index("--workpath") + 1] == str(out_dir / "build")


def test_build_pyinstaller_argv_uses_fallback_prefix_when_not_installed(monkeypatch, tmp_path):
    monkeypatch.setattr(console_module.shutil, "which", lambda name: None)
    argv = console_module.build_pyinstaller_argv(tmp_path)
    assert argv[:3] == [sys.executable, "-m", "PyInstaller"]


# ===========================================================================
# run_build -- injectable subprocess runner (Task 5/9)
# ===========================================================================

def test_run_build_runs_in_packaging_dir_and_returns_returncode(tmp_path):
    calls = {}

    class _FakeResult:
        returncode = 3

    def fake_runner(argv, cwd=None):
        calls["argv"] = argv
        calls["cwd"] = cwd
        return _FakeResult()

    code = console_module.run_build(tmp_path, runner=fake_runner)

    assert code == 3
    expected_cwd = tmp_path / "Dia" / "DiaCLI" / "dia_console" / "packaging"
    assert calls["cwd"] == str(expected_cwd)
    assert "DiaConsole.spec" in calls["argv"]


def test_run_build_defaults_to_real_subprocess_run(monkeypatch, tmp_path):
    """Never actually invoked in this test -- just confirms the wiring."""
    calls = {}

    class _FakeResult:
        returncode = 0

    def fake_run(argv, cwd=None):
        calls["argv"] = argv
        calls["cwd"] = cwd
        return _FakeResult()

    monkeypatch.setattr(console_module.subprocess, "run", fake_run)
    code = console_module.run_build(tmp_path)
    assert code == 0
    assert calls["argv"]


# ===========================================================================
# `dia console build` -- Click wrapper (Task 5)
# ===========================================================================

def test_build_command_prints_exe_path_on_success(monkeypatch, tmp_path):
    monkeypatch.setattr(console_module, "_running_interpreter_is_64bit", lambda: True)
    monkeypatch.setattr(console_module, "find_repo_root", lambda anchor: tmp_path)
    monkeypatch.setattr(console_module, "run_build", lambda repo_root: 0)

    runner = CliRunner()
    result = runner.invoke(console_module.cli, ["build"])

    assert result.exit_code == 0, result.output
    assert str(console_module.exe_path(tmp_path)) in result.output


def test_build_command_exits_with_pyinstallers_returncode_on_failure(monkeypatch, tmp_path):
    monkeypatch.setattr(console_module, "_running_interpreter_is_64bit", lambda: True)
    monkeypatch.setattr(console_module, "find_repo_root", lambda anchor: tmp_path)
    monkeypatch.setattr(console_module, "run_build", lambda repo_root: 5)

    runner = CliRunner()
    result = runner.invoke(console_module.cli, ["build"])

    assert result.exit_code == 5


# ===========================================================================
# `dia console build` -- 64-bit interpreter guard (PD-005)
# ===========================================================================

def test_check_64bit_interpreter_raises_on_32bit(monkeypatch):
    monkeypatch.setattr(console_module, "_running_interpreter_is_64bit", lambda: False)

    with pytest.raises(click.ClickException, match="64-bit"):
        console_module.check_64bit_interpreter()


def test_check_64bit_interpreter_passes_on_64bit(monkeypatch):
    monkeypatch.setattr(console_module, "_running_interpreter_is_64bit", lambda: True)

    console_module.check_64bit_interpreter()  # must not raise


def test_build_command_fails_fast_on_32bit_without_invoking_pyinstaller(monkeypatch, tmp_path):
    monkeypatch.setattr(console_module, "_running_interpreter_is_64bit", lambda: False)
    monkeypatch.setattr(console_module, "find_repo_root", lambda anchor: tmp_path)
    called = {"value": False}
    monkeypatch.setattr(
        console_module, "run_build",
        lambda repo_root: called.update(value=True) or 0,
    )

    runner = CliRunner()
    result = runner.invoke(console_module.cli, ["build"])

    assert result.exit_code != 0
    assert "64-bit" in result.output
    assert called["value"] is False, "must fail before ever spawning PyInstaller"


# ===========================================================================
# exe_path / shortcut_path -- fixed paths (Data Contracts)
# ===========================================================================

def test_exe_path_matches_data_contract(tmp_path):
    assert console_module.exe_path(tmp_path) == (
        tmp_path / "Cluiche" / "out" / "DiaCLI" / "DiaConsole" / "DiaConsole.exe"
    )


def test_shortcut_path_matches_data_contract():
    from pathlib import Path

    assert console_module.shortcut_path() == Path.home() / "Desktop" / "Dia Console.lnk"


# ===========================================================================
# create_desktop_shortcut -- injectable win32com shell (Task 6/10)
# ===========================================================================

def test_create_desktop_shortcut_raises_when_exe_missing(tmp_path):
    with pytest.raises(click.ClickException) as exc_info:
        console_module.create_desktop_shortcut(tmp_path, shell_factory=lambda prog_id: None)

    message = str(exc_info.value)
    assert "DiaConsole.exe not found" in message
    assert str(console_module.exe_path(tmp_path)) in message
    assert "dia console build" in message


class _FakeShortcut:
    def __init__(self):
        self.TargetPath = None
        self.IconLocation = None
        self.WorkingDirectory = None
        self.saved = False

    def save(self):
        self.saved = True


class _FakeShell:
    def __init__(self):
        self.shortcuts: dict[str, _FakeShortcut] = {}

    def CreateShortCut(self, path):
        shortcut = _FakeShortcut()
        self.shortcuts[path] = shortcut
        return shortcut


def test_create_desktop_shortcut_builds_expected_shortcut_when_exe_exists(tmp_path):
    exe = console_module.exe_path(tmp_path)
    exe.parent.mkdir(parents=True)
    exe.write_bytes(b"fake exe")

    fake_shell = _FakeShell()
    target = console_module.create_desktop_shortcut(tmp_path, shell_factory=lambda prog_id: fake_shell)

    assert target == console_module.shortcut_path()
    shortcut = fake_shell.shortcuts[str(target)]
    assert shortcut.TargetPath == str(exe)
    assert shortcut.IconLocation == str(exe)
    assert shortcut.WorkingDirectory == str(exe.parent)
    assert shortcut.saved is True


def test_create_desktop_shortcut_uses_wscript_shell_prog_id(tmp_path):
    exe = console_module.exe_path(tmp_path)
    exe.parent.mkdir(parents=True)
    exe.write_bytes(b"fake exe")

    seen_prog_ids = []

    def fake_shell_factory(prog_id):
        seen_prog_ids.append(prog_id)
        return _FakeShell()

    console_module.create_desktop_shortcut(tmp_path, shell_factory=fake_shell_factory)
    assert seen_prog_ids == ["WScript.Shell"]


# ===========================================================================
# `dia console install-shortcut` -- Click wrapper (Task 6)
# ===========================================================================

def test_install_shortcut_command_reports_missing_exe(monkeypatch, tmp_path):
    monkeypatch.setattr(console_module, "find_repo_root", lambda anchor: tmp_path)

    runner = CliRunner()
    result = runner.invoke(console_module.cli, ["install-shortcut"])

    assert result.exit_code != 0
    assert "DiaConsole.exe not found" in result.output
    assert "dia console build" in result.output


def test_install_shortcut_command_prints_target_on_success(monkeypatch, tmp_path):
    fake_target = tmp_path / "Desktop" / "Dia Console.lnk"
    monkeypatch.setattr(console_module, "find_repo_root", lambda anchor: tmp_path)
    monkeypatch.setattr(console_module, "create_desktop_shortcut", lambda repo_root: fake_target)

    runner = CliRunner()
    result = runner.invoke(console_module.cli, ["install-shortcut"])

    assert result.exit_code == 0, result.output
    assert str(fake_target) in result.output


# ===========================================================================
# Frozen sys.std* crash guard -- regression test for a real bug found by
# actually double-clicking the packaged exe (git history: "stub
# sys.stdout/stderr in the frozen entrypoint"). Fast: no PyInstaller build
# needed, just monkeypatches sys.stdout/stderr to None the way Windows
# itself does for a console=False build with no attached console.
# ===========================================================================

def test_ensure_std_streams_stubs_none_stdout_and_stderr(monkeypatch):
    monkeypatch.setattr(sys, "stdout", None)
    monkeypatch.setattr(sys, "stderr", None)

    entrypoint_module._ensure_std_streams()

    assert sys.stdout is not None
    assert sys.stderr is not None
    sys.stdout.write("smoke")  # must not raise ValueError: I/O operation on closed file
    sys.stderr.write("smoke")


def test_ensure_std_streams_leaves_real_streams_untouched(monkeypatch):
    fake_out, fake_err = io.StringIO(), io.StringIO()
    monkeypatch.setattr(sys, "stdout", fake_out)
    monkeypatch.setattr(sys, "stderr", fake_err)

    entrypoint_module._ensure_std_streams()

    assert sys.stdout is fake_out
    assert sys.stderr is fake_err


# ===========================================================================
# DiaConsole.spec static guard -- regression test for the pathex/hiddenimports
# bugs found by an actual PyInstaller build (git history: "bundle
# dia_console/dia_cli and every dynamically-loaded command"). Fast: a plain
# text check, no PyInstaller invocation.
# ===========================================================================

def test_diaconsole_spec_bundles_dia_console_and_dynamic_commands():
    from dia_cli.utils.repo_root import find_repo_root

    repo_root = find_repo_root(__file__)
    spec_path = repo_root / "Dia" / "DiaCLI" / "dia_console" / "packaging" / "DiaConsole.spec"
    text = spec_path.read_text(encoding="utf-8")

    assert "pathex=['../..']" in text, (
        "pathex must resolve to Dia/DiaCLI -- empty pathex means PyInstaller's "
        "Analysis can never resolve `import dia_console`/`import dia_cli` at all"
    )
    for package in ("dia_cli.cli", "dia_cli.commands", "dia_console"):
        assert f"collect_submodules('{package}')" in text, (
            f"hiddenimports must force-bundle {package} -- its submodules are loaded "
            "dynamically at runtime (os.walk + SourceFileLoader), invisible to "
            "PyInstaller's static import-graph analysis"
        )


# ===========================================================================
# Real end-to-end PyInstaller build -- opt-in only (Task 11)
# ===========================================================================

def _powershell_exe() -> str:
    """Resolve a real powershell executable path.

    Bare ``"powershell"`` can fail to resolve via ``CreateProcess`` depending
    on how the *current* process's own ``PATH`` was constructed (observed
    when pytest itself is launched from a Git Bash shell) -- fall back to
    Windows' well-known, always-present System32 location.
    """
    import shutil

    return shutil.which("powershell") or r"C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe"


def _find_diaconsole_listening_port(timeout: float) -> int | None:
    """Poll (via PowerShell) for any running DiaConsole.exe's listening TCP port.

    Queried by process *name*, not the PID `subprocess.Popen` returns: an
    actual double-clicked run showed PyInstaller's onefile bootloader spawns
    a second child process that does the real work (binds the port, owns the
    window) -- the parent Popen handle's own PID never listens on anything.
    """
    deadline = time.monotonic() + timeout
    ps_cmd = (
        "Get-Process -Name DiaConsole -ErrorAction SilentlyContinue | "
        "ForEach-Object { Get-NetTCPConnection -OwningProcess $_.Id -State Listen "
        "-ErrorAction SilentlyContinue } | Select-Object -First 1 -ExpandProperty LocalPort"
    )
    while time.monotonic() < deadline:
        result = subprocess.run(
            [_powershell_exe(), "-NoProfile", "-Command", ps_cmd],
            capture_output=True, text=True, timeout=10,
        )
        output = result.stdout.strip()
        if output.isdigit():
            return int(output)
        time.sleep(0.5)
    return None


def _kill_all_diaconsole_processes() -> None:
    subprocess.run(
        [_powershell_exe(), "-NoProfile", "-Command",
         "Get-Process -Name DiaConsole -ErrorAction SilentlyContinue | "
         "Stop-Process -Force -ErrorAction SilentlyContinue"],
        capture_output=True, timeout=10,
    )


@pytest.mark.integration
def test_build_end_to_end_produces_a_real_exe():
    """Slow (~1-3 min) and produces a 80-150MB binary -- opt-in via -m integration."""
    from dia_cli.utils.repo_root import find_repo_root

    repo_root = find_repo_root(__file__)
    returncode = console_module.run_build(repo_root)

    assert returncode == 0
    assert console_module.exe_path(repo_root).exists()


@pytest.mark.integration
def test_built_exe_launches_and_serves_real_commands_with_no_output_redirection():
    """The authoritative regression guard for all three real bugs found this
    session (pathex, hiddenimports, sys.std* stubbing): actually launch the
    packaged exe -- deliberately *not* redirecting stdout/stderr, since doing
    so is exactly what masked the sys.stdout-is-None crash during manual
    testing -- and confirm it serves every previously-broken command group
    through its own real HTTP API.

    Depends on test_build_end_to_end_produces_a_real_exe having already run
    in the same session (pytest runs a module's tests in definition order).
    """
    from dia_cli.utils.repo_root import find_repo_root

    repo_root = find_repo_root(__file__)
    exe = console_module.exe_path(repo_root)
    assert exe.exists(), "run test_build_end_to_end_produces_a_real_exe first"

    _kill_all_diaconsole_processes()
    process = subprocess.Popen([str(exe)], cwd=str(exe.parent))
    try:
        port = _find_diaconsole_listening_port(timeout=20.0)
        assert port is not None, "DiaConsole.exe never opened a listening port"

        with urllib.request.urlopen(f"http://127.0.0.1:{port}/api/commands", timeout=5) as resp:
            commands = json.loads(resp.read())
        ids = {c["id"] for c in commands}
        assert len(ids) >= 60, f"expected ~65 reflected commands, got {len(ids)}"
        for group in ("agent", "asset", "scaffold", "docs", "fix", "reflect", "codegen"):
            assert any(i == group or i.startswith(group + ".") for i in ids), (
                f"'{group}' missing from the packaged exe's reflected commands "
                f"-- regression in pathex/hiddenimports bundling: {sorted(ids)}"
            )
    finally:
        process.terminate()
        _kill_all_diaconsole_processes()
