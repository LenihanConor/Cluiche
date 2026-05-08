"""Toolchain verification utilities for dia env verify."""
import os
import shutil
import subprocess
from pathlib import Path
from typing import Optional

from dia_cli.utils.check_result import CheckResult


_VSWHERE = Path("C:/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe")
_REQUIRED_WORKLOAD = "Microsoft.VisualStudio.Workload.NativeDesktop"


def _run(cmd: list, **kwargs) -> tuple:
    """Returns (returncode, stdout, stderr)."""
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, **kwargs)
        return result.returncode, result.stdout, result.stderr
    except (FileNotFoundError, OSError):
        return -1, "", "not found"


def check_vs2022() -> list:
    results = []
    if not _VSWHERE.exists():
        results.append(CheckResult("VS 2022", "toolchain", "fail",
                                   "vswhere.exe not found — VS 2022 not installed",
                                   "dia env setup --toolchain"))
        results.append(CheckResult("C++ Desktop workload", "toolchain", "fail",
                                   "vswhere.exe not found",
                                   "dia env setup --toolchain"))
        return results

    rc, out, _ = _run([str(_VSWHERE), "-latest", "-format", "json", "-requires", _REQUIRED_WORKLOAD])
    if rc == 0 and out.strip() and out.strip() != "[]":
        import json
        try:
            data = json.loads(out)
            version = data[0].get("installationVersion", "") if data else ""
            results.append(CheckResult("VS 2022", "toolchain", "pass", version))
            results.append(CheckResult("C++ Desktop workload", "toolchain", "pass"))
        except Exception:
            results.append(CheckResult("VS 2022", "toolchain", "warn", "installed but could not parse version"))
            results.append(CheckResult("C++ Desktop workload", "toolchain", "warn", "could not verify"))
    else:
        # Check if VS installed at all without the workload
        rc2, out2, _ = _run([str(_VSWHERE), "-latest", "-format", "json"])
        if rc2 == 0 and out2.strip() and out2.strip() != "[]":
            results.append(CheckResult("VS 2022", "toolchain", "pass", "installed"))
            results.append(CheckResult("C++ Desktop workload", "toolchain", "fail",
                                       "C++ Desktop workload missing",
                                       "dia env setup --toolchain"))
        else:
            results.append(CheckResult("VS 2022", "toolchain", "fail",
                                       "not installed",
                                       "dia env setup --toolchain"))
            results.append(CheckResult("C++ Desktop workload", "toolchain", "fail",
                                       "VS not installed",
                                       "dia env setup --toolchain"))
    return results


def check_python() -> CheckResult:
    rc, out, _ = _run(["python", "--version"])
    if rc == 0 and "3.11" in out:
        return CheckResult("Python 3.11", "toolchain", "pass", out.strip())
    # Try python3
    rc2, out2, _ = _run(["python3", "--version"])
    if rc2 == 0 and "3.11" in out2:
        return CheckResult("Python 3.11", "toolchain", "pass", out2.strip())
    if rc == 0:
        return CheckResult("Python 3.11", "toolchain", "warn",
                           f"Python installed but not 3.11: {out.strip()}",
                           "dia env setup --toolchain")
    return CheckResult("Python 3.11", "toolchain", "fail",
                       "Python 3.11 not found on PATH",
                       "dia env setup --toolchain")


def check_git() -> CheckResult:
    rc, out, _ = _run(["git", "--version"])
    if rc == 0:
        return CheckResult("Git", "toolchain", "pass", out.strip())
    return CheckResult("Git", "toolchain", "fail", "not installed", "dia env setup --toolchain")


def check_nodejs() -> CheckResult:
    from dia_cli.utils.node_resolve import find_node

    node_path = find_node()
    if node_path is None:
        return CheckResult("Node.js", "toolchain", "fail", "not installed", "dia env setup --toolchain")

    rc, out, _ = _run([str(node_path), "--version"])
    if rc == 0:
        ver = out.strip().lstrip("v")
        detail = out.strip()
        if node_path != Path(shutil.which("node") or ""):
            detail += f" (at {node_path})"
        try:
            major = int(ver.split(".")[0])
            if major >= 18:
                return CheckResult("Node.js", "toolchain", "pass", detail)
            elif major >= 10:
                return CheckResult("Node.js", "toolchain", "warn",
                                   f"Node.js {detail} installed; recommend >= 18",
                                   "dia env setup --toolchain")
            else:
                return CheckResult("Node.js", "toolchain", "fail",
                                   f"Node.js {detail} too old (< 10)",
                                   "dia env setup --toolchain")
        except ValueError:
            return CheckResult("Node.js", "toolchain", "pass", detail)
    return CheckResult("Node.js", "toolchain", "fail", "found but could not run", "dia env setup --toolchain")


def check_poetry() -> CheckResult:
    rc, out, _ = _run(["poetry", "--version"])
    if rc == 0:
        return CheckResult("Poetry", "toolchain", "pass", out.strip())
    return CheckResult("Poetry", "toolchain", "fail", "not installed", "pip install poetry")


def check_docker() -> list:
    results = []
    # Check if docker is available
    rc, out, _ = _run(["docker", "info", "--format", "{{.OSType}}"])
    if rc != 0:
        results.append(CheckResult("Docker Desktop", "toolchain", "warn",
                                   "Docker not running or not installed",
                                   "Start Docker Desktop"))
        results.append(CheckResult("Windows Containers mode", "toolchain", "fail",
                                   "Docker not running",
                                   "Right-click Docker tray -> Switch to Windows containers"))
        return results

    results.append(CheckResult("Docker Desktop", "toolchain", "pass"))
    os_type = out.strip().lower()
    if os_type == "windows":
        results.append(CheckResult("Windows Containers mode", "toolchain", "pass"))
    else:
        results.append(CheckResult("Windows Containers mode", "toolchain", "warn",
                                   f"Docker running in {os_type} mode",
                                   "Right-click Docker tray -> Switch to Windows containers"))
    return results


def check_all_toolchain() -> list:
    results = []
    results.extend(check_vs2022())
    results.append(check_python())
    results.append(check_git())
    results.append(check_nodejs())
    results.append(check_poetry())
    results.extend(check_docker())
    return results
