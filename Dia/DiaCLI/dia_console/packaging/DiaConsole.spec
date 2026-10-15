# -*- mode: python ; coding: utf-8 -*-
"""PyInstaller spec for DiaConsole.exe (Goal 3, console-desktop-install.md).

``dia_cli``'s own ``DiaCLI.get_command()`` (dia_cli_main.py) loads every
individual command module (``dia_cli/cli/*.py``, and whatever
``dia_cli/commands/**`` they import) dynamically at runtime via
``os.walk`` + ``importlib.machinery.SourceFileLoader`` against real files on
disk -- invisible to PyInstaller's static import-graph analysis, since
nothing in the statically-reachable ``entrypoint.py`` -> ``dia_console.*``
import chain ever references most of those command modules by name.
Confirmed by an actual build+run: dozens of real commands (``agent``,
``asset``, ``codegen``, ``docs``, ``fix``, ``reflect``, ``scaffold``, ...)
failed with ``ModuleNotFoundError`` for their own internal
``dia_cli.commands.*`` imports, because PyInstaller never bundled those
submodules at all. ``collect_submodules`` below forces them all in.

Windowed (console=False) one-file build targeting entrypoint.py.

``dia_console/web/static/*`` is not Python, so PyInstaller does not
auto-discover it from ``Analysis``'s imports -- it must be listed explicitly
in ``datas`` so ``dia_console/web/app.py``'s
``STATIC_DIR = Path(__file__).parent / "static"`` still finds
index.html/app.js/styles.css once frozen. Paths here are relative to this
spec file's own directory (``dia_console/packaging/``); the destination
mirrors the source package layout (``dia_console/web/static``) so that
``Path(__file__).parent``-relative lookups in app.py keep working unchanged.

``pathex=['../..']`` resolves to ``Dia/DiaCLI`` (two levels up from this
spec's own directory) -- without it, PyInstaller's Analysis cannot resolve
``import dia_console``/``import dia_cli`` at all (confirmed by an actual
build: it silently produced a several-MB exe missing the entire application,
logging only a easy-to-miss "missing module named 'dia_console'" line in
warn-DiaConsole.txt, not a build failure).
"""

from PyInstaller.utils.hooks import collect_submodules

a = Analysis(
    ['entrypoint.py'],
    pathex=['../..'],
    binaries=[],
    datas=[('../web/static', 'dia_console/web/static')],
    hiddenimports=[
        *collect_submodules('dia_cli.cli'),
        *collect_submodules('dia_cli.commands'),
        *collect_submodules('dia_console'),
    ],
    hookspath=[],
    runtime_hooks=[],
    excludes=[],
    noarchive=False,
)
pyz = PYZ(a.pure)
exe = EXE(
    pyz,
    a.scripts,
    a.binaries,
    a.zipfiles,
    a.datas,
    [],
    name='DiaConsole',
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=False,
    upx_exclude=[],
    runtime_tmpdir=None,
    console=False,
    icon='DiaConsole.ico',
)
