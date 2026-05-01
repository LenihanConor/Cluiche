"""Toolchain install via winget for dia env setup --toolchain."""
import ctypes
import subprocess
from pathlib import Path
from typing import Optional

from utils.repo_root import find_repo_root

_REPO_ROOT = find_repo_root(__file__)


def _is_admin() -> bool:
    try:
        return bool(ctypes.windll.shell32.IsUserAnAdmin())
    except Exception:
        return False


def run(repo_root: Optional[Path], force: bool = False) -> int:
    root = repo_root if repo_root is not None else _REPO_ROOT

    if not _is_admin():
        print("ERROR: dia env setup --toolchain requires administrator privileges")
        print("Re-run: dia env setup --toolchain  (as Administrator)")
        return 1

    winget_json = root / "winget.json"
    if not winget_json.exists():
        print(f"ERROR: winget.json not found at {winget_json}")
        return 1

    # Check if VS devenv is running
    try:
        result = subprocess.run(["tasklist", "/fi", "imagename eq devenv.exe"],
                                capture_output=True, text=True)
        if "devenv.exe" in result.stdout.lower():
            print("WARN: Visual Studio (devenv.exe) is running. Close VS before installing workloads.")
    except Exception:
        pass

    print("Running: winget import ...")
    cmd = ["winget", "import", str(winget_json),
           "--accept-package-agreements", "--accept-source-agreements"]
    result = subprocess.run(cmd)
    if result.returncode != 0:
        return result.returncode

    print("Installing VS C++ Desktop workload...")
    vs_installer = Path("C:/Program Files (x86)/Microsoft Visual Studio/Installer/vs_installer.exe")
    if vs_installer.exists():
        vs_cmd = [
            str(vs_installer), "modify",
            "--installPath", "C:/Program Files/Microsoft Visual Studio/2022/Community",
            "--add", "Microsoft.VisualStudio.Workload.NativeDesktop",
            "--includeRecommended", "--quiet", "--wait"
        ]
        subprocess.run(vs_cmd)

    print("NOTE: Switch Docker Desktop to Windows Containers mode:")
    print("      Right-click Docker tray icon -> Switch to Windows containers")

    return 0
