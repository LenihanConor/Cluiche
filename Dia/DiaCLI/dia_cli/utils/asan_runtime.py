"""Locate the MSVC AddressSanitizer runtime DLL and expose it to child processes.

Executables built in Debug-Asan import clang_rt.asan_dbg_dynamic-x86_64.dll, which
lives in the MSVC bin folder and is normally only on PATH inside a vcvars shell.
"""
from __future__ import annotations

import os
import re
import subprocess
import sys
from pathlib import Path
from typing import Optional

_VSWHERE = Path("C:/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe")

ASAN_CONFIG = "Debug-Asan"
_DBG_DLL = "clang_rt.asan_dbg_dynamic-x86_64.dll"
_REL_DLL = "clang_rt.asan_dynamic-x86_64.dll"


def _vs_install_path() -> Optional[Path]:
    if not _VSWHERE.exists():
        return None
    try:
        r = subprocess.run(
            [str(_VSWHERE), "-latest", "-products", "*", "-property", "installationPath"],
            capture_output=True, text=True,
        )
    except OSError:
        return None
    line = r.stdout.strip().splitlines()[0].strip() if r.returncode == 0 and r.stdout.strip() else ""
    return Path(line) if line else None


def _version_key(p: Path) -> tuple:
    return tuple(int(x) for x in re.findall(r"\d+", p.name))


def find_asan_runtime_dir(debug_crt: bool = True) -> Optional[Path]:
    """Return the newest MSVC Hostx64/x64 bin dir containing the ASan runtime DLL."""
    root = _vs_install_path()
    if root is None:
        return None
    dll = _DBG_DLL if debug_crt else _REL_DLL
    msvc = root / "VC" / "Tools" / "MSVC"
    if not msvc.is_dir():
        return None
    for ver in sorted((d for d in msvc.iterdir() if d.is_dir()), key=_version_key, reverse=True):
        cand = ver / "bin" / "Hostx64" / "x64"
        if (cand / dll).is_file():
            return cand
    return None


def with_asan_runtime_on_path(env: Optional[dict], config: str) -> dict:
    """Return a copy of env; for Debug-Asan prepend the ASan runtime dir to PATH."""
    out = dict(os.environ if env is None else env)
    if config != ASAN_CONFIG:
        return out
    d = find_asan_runtime_dir(debug_crt=True)
    if d is None:
        print(f"[dia] WARNING: could not find {_DBG_DLL} in any installed MSVC toolset; "
              f"{ASAN_CONFIG} executables may fail with STATUS_DLL_NOT_FOUND (0xC0000135).",
              file=sys.stderr)
        return out
    out["PATH"] = str(d) + os.pathsep + out.get("PATH", "")
    return out
