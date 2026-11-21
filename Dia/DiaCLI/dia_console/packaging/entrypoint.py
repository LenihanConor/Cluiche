"""PyInstaller's Analysis target for the packaged DiaConsole.exe (Goal 1).

PyInstaller needs a plain Python entry script, not a Click command object --
``dia_cli/cli/console.py``'s ``cli`` group is not usable as an Analysis
target. This is a thin wrapper around the exact same ``launch()`` that
``dia console`` (no subcommand) already calls, so the packaged exe and the
Poetry-installed ``dia console`` command behave identically.

The ``sys.std*`` stubbing below is required, not optional -- confirmed by an
actual double-clicked run: with ``console=False`` (windowed, no console
subsystem), Windows never attaches ``sys.stdout``/``sys.stderr`` at all, so
they are ``None`` rather than merely closed. The first thing ``launch()``
does is construct a ``uvicorn.Config``, whose ``configure_logging()`` builds
a ``StreamHandler`` over ``sys.stderr`` unconditionally -- with a ``None``
stream this raised ``ValueError: Unable to configure formatter 'default'``
(root cause: ``ValueError: I/O operation on closed file`` inside
``uvicorn/logging.py``), crashing before the window ever appeared. This is a
standard, well-known pattern for any windowed/frozen Python app that touches
logging -- redirect to ``os.devnull`` so anything that writes (uvicorn,
``print()``, ``click.echo()``) has a real, harmless file object instead of
``None``.
"""
from __future__ import annotations

import os
import sys


def _ensure_std_streams() -> None:
    """Stub ``sys.stdout``/``sys.stderr`` to ``os.devnull`` when ``None``.

    Extracted as its own function (rather than inlined in the ``__main__``
    guard below) so a test can call it directly with ``sys.stdout``/
    ``sys.stderr`` monkeypatched to ``None`` -- reproducing the exact
    windowed/frozen crash condition without needing a real PyInstaller build.
    """
    if sys.stdout is None:
        sys.stdout = open(os.devnull, "w")
    if sys.stderr is None:
        sys.stderr = open(os.devnull, "w")


def _ensure_dia_cli_config() -> None:
    """Set ``DIA_CLI_CONFIG`` so ``dia_cli_main``'s own command discovery finds
    the real ``dia_cli/cli/`` tree, regardless of the frozen exe's working
    directory.

    ``dia_cli_main.py`` resolves its own separate ``_root_path`` (used only
    for finding CLI command modules, via ``os.walk`` for a directory named
    ``cli``) either from this env var, or else by walking *up* from the
    process's cwd looking for ``dia_cli_prime_config.json``. Confirmed by an
    actual double-click of the installed Desktop shortcut: the nav column
    showed **zero** commands, because the shortcut's ``WorkingDirectory``
    (``Cluiche/out/DiaCLI/DiaConsole/``, per console-desktop-install.md's own
    Goal 6) is not an ancestor of ``Dia/DiaCLI/`` at all -- they are sibling
    subtrees under the repo root -- so that upward walk can never reach
    ``dia_cli_prime_config.json`` for this exe, no matter what. It only
    happened to work during this session's own build+launch testing because
    the dev shell those tests ran from already had ``DIA_CLI_CONFIG`` set.

    ``find_repo_root`` already resolves correctly in this exact scenario
    (``dia_console/execution.py`` relies on it for the same reason): a
    frozen module's own ``__file__`` points into PyInstaller's throwaway
    ``_MEIPASS`` extraction dir, so its parents-walk fails, but it then falls
    back to the process's real cwd and cwd's parents -- which *does* reach
    the repo root, because ``Cluiche/Cluiche.sln`` (the marker it looks for)
    lives there, unlike ``dia_cli_prime_config.json``.
    """
    if "DIA_CLI_CONFIG" in os.environ:
        return
    from dia_cli.utils.repo_root import find_repo_root

    try:
        repo_root = find_repo_root(__file__)
    except RuntimeError:
        return
    config_path = repo_root / "Dia" / "DiaCLI" / "dia_cli_prime_config.json"
    if config_path.is_file():
        os.environ["DIA_CLI_CONFIG"] = str(config_path)


if __name__ == "__main__":
    _ensure_std_streams()
    _ensure_dia_cli_config()

    from dia_console.shell import launch

    launch()
