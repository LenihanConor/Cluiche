"""dia console -- launch DiaConsole's native window, or package/install it.

Additive (SD-CONSOLE-008): this is one more file under dia_cli/cli/,
auto-discovered the same way as every other command. Omitting it from a
build leaves the rest of DiaCLI working unchanged.

``cli`` used to be a bare ``@click.command()``. It is now a
``@click.group(invoke_without_command=True)`` so that ``dia console``
(SD-CONSOLE-012's original entry point) keeps launching the shell exactly as
before while also hosting two new subcommands (console-desktop-install.md):

* ``dia console build`` -- packages DiaConsole as a windowed one-file
  ``DiaConsole.exe`` via PyInstaller.
* ``dia console install-shortcut`` -- creates/overwrites a Desktop ``.lnk``
  pointing at that exe.
"""
from __future__ import annotations

import shutil
import struct
import subprocess
import sys
from pathlib import Path
from typing import Callable

import click
from loguru import logger

from dia_cli.utils.repo_root import find_repo_root

#: Fixed paths (console-desktop-install.md's Data Contracts), relative to repo_root.
_OUT_SUBDIR = ("Cluiche", "out", "DiaCLI", "DiaConsole")
_PACKAGING_SUBDIR = ("Dia", "DiaCLI", "dia_console", "packaging")


def exe_path(repo_root: Path) -> Path:
    """Where ``dia console build`` places the packaged exe (Goals 5-6)."""
    return repo_root.joinpath(*_OUT_SUBDIR) / "DiaConsole.exe"


def shortcut_path() -> Path:
    """The Desktop shortcut ``install-shortcut`` creates/overwrites (Goal 6)."""
    return Path.home() / "Desktop" / "Dia Console.lnk"


def _packaging_dir(repo_root: Path) -> Path:
    """Where ``DiaConsole.spec`` lives -- ``build``'s subprocess cwd, so the
    spec file's own relative ``datas`` paths resolve (Goal 5)."""
    return repo_root.joinpath(*_PACKAGING_SUBDIR)


# --------------------------------------------------------------------------- #
# `dia console build`
# --------------------------------------------------------------------------- #

def _running_interpreter_is_64bit() -> bool:
    return struct.calcsize("P") * 8 == 64


def check_64bit_interpreter() -> None:
    """Fail loudly (PD-005) rather than silently freeze a 32-bit DiaConsole.exe.

    PyInstaller bundles whichever interpreter invokes it. This repo's own
    default ``.venv`` was found to be 32-bit Python during a real build+launch
    pass this session -- with no check, ``dia console build`` would silently
    produce a 32-bit exe (violating PD-005: x64 is the only supported build
    target) and nobody would notice until double-clicking the result. Checked
    at the Click command level, before ``run_build`` ever spawns PyInstaller,
    so a 32-bit run fails fast instead of wasting 1-3 minutes on a build
    that's non-compliant regardless of whether it "works."
    """
    if not _running_interpreter_is_64bit():
        raise click.ClickException(
            "dia console build requires a 64-bit Python interpreter (PD-005: "
            "x64 is the only supported build target). The interpreter running "
            f"this command ({sys.executable}) is 32-bit -- re-run this command "
            "from a 64-bit Python environment."
        )


def pyinstaller_argv_prefix() -> list[str]:
    """The argv words that start a PyInstaller invocation.

    Mirrors ``dia_console/execution.py``'s ``default_argv_prefix()``: prefers
    the installed ``pyinstaller`` launcher, falls back to running it as a
    module with the current interpreter when it's not on PATH.
    """
    executable = shutil.which("pyinstaller")
    if executable:
        return [executable]
    return [sys.executable, "-m", "PyInstaller"]


def build_pyinstaller_argv(repo_root: Path) -> list[str]:
    """The full argv for ``dia console build``'s PyInstaller subprocess (Goal 5).

    A plain, pure function -- no subprocess is spawned here -- so tests can
    assert on its output without ever invoking PyInstaller.
    """
    out_dir = repo_root.joinpath(*_OUT_SUBDIR)
    return [
        *pyinstaller_argv_prefix(),
        "DiaConsole.spec",
        "--distpath", str(out_dir),
        "--workpath", str(out_dir / "build"),
        "--noconfirm",
    ]


def run_build(
    repo_root: Path,
    *,
    runner: Callable[..., subprocess.CompletedProcess] | None = None,
) -> int:
    """Run PyInstaller against ``DiaConsole.spec``; returns its returncode.

    ``runner`` is injectable (same DI pattern as ``dia_cli/cli/preset.py``'s
    ``run_preset``) so tests never actually invoke PyInstaller -- it defaults
    to ``None`` and resolves to ``subprocess.run`` here, at call time, rather
    than being bound as the parameter's default at import time, so tests can
    monkeypatch ``subprocess.run`` directly. Runs with ``cwd`` set to
    ``dia_console/packaging/`` so the spec file's own relative ``datas``
    entry resolves. Inherits stdio (no captured output) so the user sees
    PyInstaller's own progress; the caller decides what to do with a
    non-zero returncode rather than this function swallowing it.
    """
    if runner is None:
        runner = subprocess.run
    argv = build_pyinstaller_argv(repo_root)
    logger.info("dia console build: running PyInstaller ({})", " ".join(argv))
    result = runner(argv, cwd=str(_packaging_dir(repo_root)))
    if result.returncode == 0:
        logger.info("dia console build: succeeded -> {}", exe_path(repo_root))
    else:
        logger.error("dia console build: PyInstaller exited with code {}", result.returncode)
    return result.returncode


# --------------------------------------------------------------------------- #
# `dia console install-shortcut`
# --------------------------------------------------------------------------- #

def create_desktop_shortcut(
    repo_root: Path,
    *,
    shell_factory: Callable[[str], object] | None = None,
) -> Path:
    """Create/overwrite the Desktop ``.lnk`` pointing at ``DiaConsole.exe`` (Goals 6-7).

    Fails loudly with :class:`click.ClickException` if the exe hasn't been
    built yet -- this deliberately does not auto-trigger a build (explicit
    design decision, Goal 6). ``shell_factory`` is injectable (defaults to
    the real ``win32com.client.Dispatch``, imported lazily so importing this
    module never requires a Windows COM environment) so tests can supply a
    fake shell instead of needing real COM. ``CreateShortCut(path).save()``
    on an existing path overwrites in place, so this is idempotent with no
    special-case deletion logic (Goal 7).
    """
    exe = exe_path(repo_root)
    if not exe.exists():
        raise click.ClickException(
            f"DiaConsole.exe not found at {exe} — run 'dia console build' first."
        )

    if shell_factory is None:
        import win32com.client

        shell_factory = win32com.client.Dispatch

    shell = shell_factory("WScript.Shell")
    target = shortcut_path()
    shortcut = shell.CreateShortCut(str(target))
    shortcut.TargetPath = str(exe)
    shortcut.IconLocation = str(exe)
    shortcut.WorkingDirectory = str(exe.parent)
    shortcut.save()
    return target


# --------------------------------------------------------------------------- #
# Click wiring
# --------------------------------------------------------------------------- #

@click.group(invoke_without_command=True)
@click.pass_context
def cli(ctx: click.Context) -> None:
    """Launch DiaConsole -- a chromeless native window for browsing and
    running DiaCLI commands with a live log view -- or manage its packaged
    desktop install (``build`` / ``install-shortcut``)."""
    if ctx.invoked_subcommand is None:
        from dia_console.shell import launch

        launch()


@cli.command("build")
@click.pass_context
def build_command(ctx: click.Context) -> None:
    """Package DiaConsole as a windowed one-file DiaConsole.exe via PyInstaller."""
    check_64bit_interpreter()
    repo_root = find_repo_root(__file__)
    returncode = run_build(repo_root)
    if returncode != 0:
        ctx.exit(returncode)
    click.echo(str(exe_path(repo_root)))


@cli.command("install-shortcut")
def install_shortcut_command() -> None:
    """Create/overwrite the Desktop shortcut pointing at DiaConsole.exe."""
    repo_root = find_repo_root(__file__)
    target = create_desktop_shortcut(repo_root)
    click.echo(str(target))
