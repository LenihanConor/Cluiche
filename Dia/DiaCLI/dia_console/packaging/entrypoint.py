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


if __name__ == "__main__":
    _ensure_std_streams()

    from dia_console.shell import launch

    launch()
