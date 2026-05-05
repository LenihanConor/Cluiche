"""Handler for dia fix — builds and runs the aider test-fix loop."""
import shutil
import subprocess
import sys
from typing import Optional


def run_fix(target: str, filter_pattern: Optional[str], model: str, config: str) -> int:
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
    ]

    result = subprocess.run(cmd)
    return result.returncode
