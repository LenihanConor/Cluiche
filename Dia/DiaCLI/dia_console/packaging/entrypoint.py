"""PyInstaller's Analysis target for the packaged DiaConsole.exe (Goal 1).

PyInstaller needs a plain Python entry script, not a Click command object --
``dia_cli/cli/console.py``'s ``cli`` group is not usable as an Analysis
target. This is a thin wrapper around the exact same ``launch()`` that
``dia console`` (no subcommand) already calls, so the packaged exe and the
Poetry-installed ``dia console`` command behave identically.
"""
from __future__ import annotations

if __name__ == "__main__":
    from dia_console.shell import launch

    launch()
