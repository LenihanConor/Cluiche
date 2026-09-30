# -*- mode: python ; coding: utf-8 -*-
"""PyInstaller spec for DiaConsole.exe (Goal 3, console-desktop-install.md).

Windowed (console=False) one-file build targeting entrypoint.py.

``dia_console/web/static/*`` is not Python, so PyInstaller does not
auto-discover it from ``Analysis``'s imports -- it must be listed explicitly
in ``datas`` so ``dia_console/web/app.py``'s
``STATIC_DIR = Path(__file__).parent / "static"`` still finds
index.html/app.js/styles.css once frozen. Paths here are relative to this
spec file's own directory (``dia_console/packaging/``); the destination
mirrors the source package layout (``dia_console/web/static``) so that
``Path(__file__).parent``-relative lookups in app.py keep working unchanged.
"""

a = Analysis(
    ['entrypoint.py'],
    pathex=[],
    binaries=[],
    datas=[('../web/static', 'dia_console/web/static')],
    hiddenimports=[],
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
