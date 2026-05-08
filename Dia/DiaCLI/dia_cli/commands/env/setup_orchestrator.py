"""Orchestration for dia env setup command."""
import ctypes
import subprocess
from pathlib import Path
from typing import Optional

from dia_cli.utils.repo_root import find_repo_root

_REPO_ROOT = find_repo_root(__file__)


def _is_admin() -> bool:
    try:
        return bool(ctypes.windll.shell32.IsUserAnAdmin())
    except Exception:
        return False


def run(
    repo_root: Optional[Path],
    toolchain: bool,
    deps_only: bool,
    dep_id: Optional[str],
    submodules: bool,
    claude: bool,
    force: bool,
    fail_fast: bool,
    quiet: bool,
    local_llm: bool = False,
) -> int:
    root = repo_root if repo_root is not None else _REPO_ROOT

    # Validate --dep flag usage
    if dep_id and (toolchain or submodules or claude):
        print("ERROR: --dep can only be combined with --deps, not --toolchain/--submodules/--claude")
        return 2

    # Determine which steps to run
    run_all = not any([toolchain, deps_only, dep_id, submodules, claude, local_llm])
    steps = []
    if run_all or toolchain:
        steps.append("toolchain")
    if run_all or deps_only or dep_id:
        steps.append("deps")
    if run_all or submodules:
        steps.append("submodules")
    if run_all or claude:
        steps.append("claude")
    if run_all or local_llm:
        steps.append("local-llm")
    if run_all:
        steps.append("cli-env")

    total = len(steps)
    passed = 0
    warned = 0
    failed = 0
    admin = _is_admin()

    if not quiet:
        print("[dia env setup]")

    for i, step in enumerate(steps, 1):
        label = f"  [{i}/{total}] {step.capitalize():<12}"

        if step == "toolchain":
            if not admin:
                warned += 1
                print(f"{label} SKIPPED — not running as administrator")
                print(f"         Re-run: dia env setup --toolchain  (as Administrator)")
                if fail_fast:
                    break
                continue
            if not quiet:
                print(f"{label} installing via winget...")
            code = _run_toolchain(root, force, quiet)

        elif step == "deps":
            if not quiet:
                print(f"{label} restoring deps...")
            from dia_cli.commands.env.deps_restore_cmd import run as deps_run
            code = deps_run(repo_root=root, dep_id=dep_id, force=force, quiet=quiet)

        elif step == "submodules":
            if not quiet:
                print(f"{label} initialising submodules...")
            code = _run_submodules(root, quiet)

        elif step == "claude":
            if not quiet:
                print(f"{label} wiring AI context...")
            from dia_cli.commands.env.claude_context_cmd import run as claude_run
            code = claude_run(repo_root=root, force=force)

        elif step == "local-llm":
            if not quiet:
                print(f"{label} setting up local LLM tools...")
            code = _setup_local_llm(force, quiet)

        elif step == "cli-env":
            if not quiet:
                print(f"{label} setting DIA_CLI_CONFIG...")
            code = _set_cli_config_env(root, force, quiet)

        else:
            continue

        if code == 0:
            passed += 1
            if not quiet:
                print(f"{label} done")
        elif code == 2:
            warned += 1
            if not quiet:
                print(f"{label} WARN")
        else:
            failed += 1
            if not quiet:
                print(f"{label} FAILED (exit {code})")
            if fail_fast:
                break

    if not quiet:
        print(f"\nSetup complete: {passed} passed, {warned} warned, {failed} failed")
        print("Run `dia env verify` to confirm environment health.")

    if failed > 0:
        return 1
    if warned > 0:
        return 2
    return 0


def _run_toolchain(repo_root: Path, force: bool, quiet: bool) -> int:
    winget_json = repo_root / "winget.json"
    if not winget_json.exists():
        print(f"ERROR: winget.json not found at {winget_json}")
        return 1
    cmd = ["winget", "import", str(winget_json),
           "--accept-package-agreements", "--accept-source-agreements"]
    try:
        result = subprocess.run(cmd, timeout=600)
        return result.returncode
    except FileNotFoundError:
        print("ERROR: winget not found on PATH")
        return 1
    except subprocess.TimeoutExpired:
        print("ERROR: winget import timed out after 10 minutes")
        return 1


def _run_submodules(repo_root: Path, quiet: bool) -> int:
    cmd = ["git", "submodule", "update", "--init", "--recursive"]
    try:
        result = subprocess.run(cmd, cwd=str(repo_root), timeout=300)
        return result.returncode
    except FileNotFoundError:
        print("ERROR: git not found on PATH")
        return 1
    except subprocess.TimeoutExpired:
        print("ERROR: git submodule update timed out after 5 minutes")
        return 1


def _set_cli_config_env(repo_root: Path, force: bool, quiet: bool) -> int:
    """Set DIA_CLI_CONFIG and add venv Scripts to PATH as persistent user environment variables."""
    import os
    import sys

    config_path = repo_root / "Dia" / "DiaCLI" / "dia_cli_prime_config.json"
    if not config_path.exists():
        print(f"ERROR: prime config not found at {config_path}")
        return 1

    config_value = str(config_path.resolve())
    scripts_dir = str((repo_root / "Dia" / "DiaCLI" / ".venv" / "Scripts").resolve())

    current_config = os.environ.get("DIA_CLI_CONFIG")
    current_path = os.environ.get("PATH", "")
    scripts_on_path = scripts_dir.lower() in current_path.lower()

    config_ok = current_config == config_value and not force
    path_ok = scripts_on_path and not force

    if config_ok and path_ok:
        if not quiet:
            print(f"         DIA_CLI_CONFIG and PATH already set correctly")
        return 0

    if sys.platform == "win32":
        if not config_ok:
            cmd = ["setx", "DIA_CLI_CONFIG", config_value]
            try:
                result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
                if result.returncode != 0:
                    print(f"ERROR: setx DIA_CLI_CONFIG failed: {result.stderr.strip()}")
                    return 1
            except FileNotFoundError:
                print("ERROR: setx not found")
                return 1
            except subprocess.TimeoutExpired:
                print("ERROR: setx timed out")
                return 1

        if not path_ok:
            import winreg
            try:
                with winreg.OpenKey(winreg.HKEY_CURRENT_USER, r"Environment", 0,
                                    winreg.KEY_READ | winreg.KEY_WRITE) as key:
                    try:
                        user_path, _ = winreg.QueryValueEx(key, "Path")
                    except FileNotFoundError:
                        user_path = ""
                    if scripts_dir.lower() not in user_path.lower():
                        new_path = f"{user_path};{scripts_dir}" if user_path else scripts_dir
                        winreg.SetValueEx(key, "Path", 0, winreg.REG_EXPAND_SZ, new_path)
            except OSError as e:
                print(f"ERROR: failed to update user PATH in registry: {e}")
                return 1
    else:
        shell_rc = Path.home() / ".bashrc"
        export_config = f'export DIA_CLI_CONFIG="{config_value}"'
        export_path = f'export PATH="{scripts_dir}:$PATH"'
        lines_to_add = []
        if shell_rc.exists():
            content = shell_rc.read_text()
            if "DIA_CLI_CONFIG" not in content:
                lines_to_add.append(export_config)
            if scripts_dir not in content:
                lines_to_add.append(export_path)
        else:
            content = ""
            lines_to_add = [export_config, export_path]
        if lines_to_add:
            with open(shell_rc, "a") as f:
                f.write("\n" + "\n".join(lines_to_add) + "\n")

    os.environ["DIA_CLI_CONFIG"] = config_value
    if not quiet:
        print(f"         DIA_CLI_CONFIG={config_value}")
        if not path_ok:
            print(f"         Added to user PATH: {scripts_dir}")
    return 0


def _setup_local_llm(force: bool, quiet: bool) -> int:
    """Install aider-chat and add its Scripts dir to PATH. Print Ollama instructions."""
    import shutil
    import sys
    import winreg

    python_scripts = Path(sys.executable).parent / "Scripts"
    aider_exe = python_scripts / "aider.exe"

    # Install aider-chat if missing or forced
    if not aider_exe.exists() or force:
        if not quiet:
            print(f"         Installing aider-chat via pip...")
        try:
            result = subprocess.run(
                [sys.executable, "-m", "pip", "install", "aider-chat"],
                capture_output=True, text=True, timeout=300,
            )
            if result.returncode != 0:
                print(f"ERROR: pip install aider-chat failed:\n{result.stderr.strip()}")
                return 1
            if not quiet:
                print(f"         aider-chat installed")
        except subprocess.TimeoutExpired:
            print("ERROR: pip install aider-chat timed out")
            return 1
    elif not quiet:
        print(f"         aider-chat already installed")

    # Add python Scripts dir to user PATH if not present
    scripts_str = str(python_scripts.resolve())
    if sys.platform == "win32":
        try:
            with winreg.OpenKey(winreg.HKEY_CURRENT_USER, r"Environment", 0,
                                winreg.KEY_READ | winreg.KEY_WRITE) as key:
                try:
                    user_path, _ = winreg.QueryValueEx(key, "Path")
                except FileNotFoundError:
                    user_path = ""
                if scripts_str.lower() not in user_path.lower():
                    new_path = f"{user_path};{scripts_str}" if user_path else scripts_str
                    winreg.SetValueEx(key, "Path", 0, winreg.REG_EXPAND_SZ, new_path)
                    if not quiet:
                        print(f"         Added to user PATH: {scripts_str}")
                elif not quiet:
                    print(f"         PATH already contains: {scripts_str}")
        except OSError as e:
            print(f"ERROR: failed to update user PATH in registry: {e}")
            return 1

    # Ollama must be installed manually — print instructions
    if shutil.which("ollama") is None:
        print("         Ollama not found. Install manually:")
        print("           1. Download from https://ollama.com/download/windows")
        print("           2. Run the installer")
        print("           3. Then run: ollama pull qwen2.5-coder:14b")
        return 2  # warn — partial setup

    # Check if default model is pulled
    try:
        result = subprocess.run(["ollama", "list"], capture_output=True, text=True, timeout=10)
        if "qwen2.5-coder:14b" not in result.stdout:
            print("         Ollama installed but qwen2.5-coder:14b not pulled.")
            print("           Run: ollama pull qwen2.5-coder:14b")
            return 2  # warn
    except (FileNotFoundError, subprocess.TimeoutExpired):
        pass

    if not quiet:
        print("         local-llm setup complete")
    return 0
