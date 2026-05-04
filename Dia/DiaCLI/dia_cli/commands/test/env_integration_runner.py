"""Loop orchestration for dia test env-integration."""
import os
import subprocess
import sys
from pathlib import Path
from typing import Optional

from dia_cli.utils.repo_root import find_repo_root

_DOCKER_IMAGE = "cluiche-build-env"

# Failure classification patterns
_LOGIC_BUG_PATTERNS = [
    "error C",       # MSVC compiler error
    "error LNK",     # MSVC linker error
    "FAILED tests/", # pytest assertion failure
    "FAILED Dia\\",  # pytest assertion failure (Windows path)
    ".vcxproj",      # project reference failure
    "MSB3073",       # msbuild exec task failed (catch-all for logic errors)
]

_ENV_PATTERNS = [
    ("is not recognized", "PATH not configured correctly"),
    ("not found in PATH", "PATH not configured correctly"),
    ("No such file or directory", "missing file or dependency"),
    ("SHA-256 mismatch", "dep integrity failure — re-download required"),
    ("submodule not initialised", "git submodule not initialised"),
    ("image not found", "Docker image missing — rebuild required"),
    ("deps.json not found", "deps.json missing"),
    ("ModuleNotFoundError", "Python package missing"),
    ("ImportError", "Python package missing"),
]

_REPO_ROOT = find_repo_root(__file__)
_CLI_ROOT = _REPO_ROOT / "Dia" / "DiaCLI"


def _classify_failure(output: str) -> tuple:
    """Returns ("logic", reason) or ("env", reason)."""
    for pattern in _LOGIC_BUG_PATTERNS:
        if pattern in output:
            return ("logic", f"logic bug detected: '{pattern}'")
    for pattern, reason in _ENV_PATTERNS:
        if pattern in output:
            return ("env", reason)
    return ("env", "unknown failure — attempting environment fix")


def _run_stage(label: str, cmd: list, repo_root: Path, cli_root: Path, inject_fault: Optional[str] = None) -> tuple:
    """Run a stage command. Returns (exit_code, combined_output)."""
    print(f"\n  Stage: {label}")
    print(f"  cmd: {' '.join(cmd)}")

    env = os.environ.copy()
    if inject_fault == "path" and label == "pipeline":
        # Simulate PATH failure by removing Python from PATH
        paths = env.get("PATH", "").split(os.pathsep)
        env["PATH"] = os.pathsep.join(p for p in paths if "python" not in p.lower())

    # Run dia CLI commands from the DiaCLI directory so MDK's sys.path discovery
    # starts from there (not repo root, which would pick up outer venv packages).
    cwd = str(cli_root) if cmd and "dia_cli" in " ".join(cmd) else str(repo_root)

    output_lines = []
    try:
        proc = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            cwd=cwd,
            env=env,
        )
        for line in proc.stdout:
            print(f"    {line}", end="")
            output_lines.append(line)
        proc.wait()
        return proc.returncode, "".join(output_lines)
    except FileNotFoundError as e:
        msg = f"command not found: {e}"
        print(f"  ERROR: {msg}")
        return 1, msg


def _generate_fix(stage: str, failure_output: str, reason: str) -> str:
    """Call Claude API to generate a fix proposal. Returns a human-readable fix string."""
    api_key = os.environ.get("ANTHROPIC_API_KEY")
    if not api_key:
        return (
            f"No ANTHROPIC_API_KEY set — cannot generate automatic fix.\n"
            f"Failure reason: {reason}\n"
            f"Manual investigation required."
        )

    try:
        import anthropic
        client = anthropic.Anthropic(api_key=api_key)
        prompt = (
            f"You are a build environment repair assistant for the Cluiche C++ game engine project.\n"
            f"A stage called '{stage}' failed with the following output:\n\n"
            f"```\n{failure_output[-3000:]}\n```\n\n"
            f"Failure classification: {reason}\n\n"
            f"Propose a concrete, minimal fix for this environment/infrastructure failure. "
            f"You may suggest changes to: Directory.Build.targets, deps.json, winget.json, "
            f"DiaCLI Python modules, or container PATH configuration. "
            f"You must NOT suggest changes to .vcxproj files or C++ source code.\n\n"
            f"Give a short (3-5 sentence) actionable fix. Be specific about what file to change and how."
        )
        message = client.messages.create(
            model="claude-sonnet-4-6",
            max_tokens=512,
            messages=[{"role": "user", "content": prompt}],
        )
        return message.content[0].text
    except Exception as e:
        return f"Fix generation failed: {e}\nManual investigation required."


def _prompt_operator(fix_proposal: str, attempt: int) -> str:
    """Print fix and prompt. Returns 'y', 'n', or 'abort'."""
    print(f"\n  Fix proposal (attempt {attempt}):")
    print("  " + "\n  ".join(fix_proposal.splitlines()))
    print()
    while True:
        try:
            answer = input("  Apply this fix and retry? [y/N/abort]: ").strip().lower()
        except EOFError:
            return "abort"
        if answer in ("y", "yes"):
            return "y"
        if answer in ("abort",):
            return "abort"
        return "n"


def run(
    repo_root: Optional[Path],
    skip_env: bool,
    max_auto_fixes: int,
    no_fix: bool,
    inject_fault: Optional[str],
    docker: bool,
) -> int:
    root = repo_root if repo_root is not None else _REPO_ROOT
    cli_root = root / "Dia" / "DiaCLI"

    print("[dia test env-integration]")

    # Define the three stages
    stages = []

    if not skip_env:
        stages.append(("env provisioning", [
            sys.executable, "-m", "dia_cli", "env", "docker",
        ]))

    stages.append(("pipeline", [
        sys.executable, "-m", "dia_cli", "pipeline",
    ]))

    stages.append(("test cli", [
        sys.executable, "-m", "dia_cli", "test", "cli",
    ]))

    # If running inside docker, wrap commands
    if docker:
        check = subprocess.run(
            ["docker", "image", "inspect", _DOCKER_IMAGE],
            capture_output=True,
        )
        if check.returncode != 0:
            print(f"ERROR: Docker image '{_DOCKER_IMAGE}' not found.\nRun: dia env docker image")
            return 3

        def wrap_docker(cmd):
            return [
                "docker", "run", "--rm",
                "--volume", f"{root}:C:/repo",
                "--workdir", "C:/repo",
                "--env", "ANTHROPIC_API_KEY",
                _DOCKER_IMAGE,
            ] + cmd
        stages = [(label, wrap_docker(cmd)) for label, cmd in stages]

    # Track fix history per stage
    fix_attempts: dict = {label: 0 for label, _ in stages}
    auto_fixes_used = 0

    stage_idx = 0
    overall_attempt = 1

    while stage_idx < len(stages):
        label, cmd = stages[stage_idx]
        print(f"\n[attempt {overall_attempt}]")

        exit_code, output = _run_stage(label, cmd, root, cli_root, inject_fault=inject_fault)

        if exit_code == 0:
            print(f"\n  Stage '{label}': PASS")
            stage_idx += 1
            overall_attempt += 1
            continue

        # Stage failed
        print(f"\n  Stage '{label}': FAILED (exit {exit_code})")

        if no_fix:
            print("  --no-fix set: exiting without fix attempt.")
            return 1

        kind, reason = _classify_failure(output)
        print(f"  Classification: {kind} — {reason}")

        if kind == "logic":
            print("  This is a logic bug, not an environment issue. Cannot auto-fix.")
            print("  Fix the underlying code and re-run.")
            return 1

        # Environment failure — attempt fix
        fix_attempts[label] += 1
        attempt_num = fix_attempts[label]

        if attempt_num > 3:
            print(f"  Stage '{label}' has failed {attempt_num} times. Giving up.")
            _print_summary(fix_attempts)
            return 4

        print(f"\n  Generating fix (stage attempt {attempt_num}/3)...")
        fix_proposal = _generate_fix(label, output, reason)

        if auto_fixes_used < max_auto_fixes:
            # Auto-apply without prompting
            print(f"\n  Auto-fix attempt {attempt_num}:")
            print("  " + "\n  ".join(fix_proposal.splitlines()))
            print("  Applying automatically (within auto-fix limit)...")
            auto_fixes_used += 1
        else:
            # Prompt operator
            answer = _prompt_operator(fix_proposal, attempt_num)
            if answer == "abort":
                print("  Aborted by operator.")
                _print_summary(fix_attempts)
                return 1
            if answer == "n":
                print("  Fix declined by operator.")
                _print_summary(fix_attempts)
                return 1

        overall_attempt += 1
        # Re-run from the same stage (don't advance stage_idx)

    print("\nAll stages green. Environment validated.")
    _print_summary(fix_attempts)
    return 0


def _print_summary(fix_attempts: dict) -> None:
    total_fixes = sum(fix_attempts.values())
    if total_fixes:
        print(f"\n  Fix attempts: {total_fixes} total")
        for stage, count in fix_attempts.items():
            if count:
                print(f"    {stage}: {count} attempt(s)")
