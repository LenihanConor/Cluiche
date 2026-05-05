"""Handler for dia fix — builds and runs the aider test-fix loop."""
import shutil
import subprocess
from typing import Optional


def run_fix(
    target: str,
    filter_pattern: Optional[str],
    model: str,
    config: str,
    max_iterations: int,
    dry_run: bool,
) -> int:
    if not shutil.which("aider"):
        print("ERROR: aider not found on PATH. Run: dia env setup --local-llm")
        return 1

    test_cmd = f"dia run {target} --config {config}"
    if filter_pattern:
        test_cmd += f" --filter={filter_pattern}"

    cmd = [
        "aider",
        "--test-cmd", test_cmd,
        "--model", model,
        "--auto-test",
        "--yes",
        "--iterations", str(max_iterations),
    ]

    if dry_run:
        print(f"[dry-run] Would run: {' '.join(cmd)}")
        return 0

    result = subprocess.run(cmd)

    _print_summary(result.returncode)
    return result.returncode


def _print_summary(returncode: int) -> None:
    if returncode == 0:
        print("\n[dia fix] All tests passing.")
    else:
        print(f"\n[dia fix] Loop ended with failures (exit {returncode}). Run again or review manually.")
