"""Run GoogleTests in parallel shards and merge results."""
import os
import subprocess
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path
from typing import Optional

from .xml_merger import merge_xml


def _list_tests(binary: Path, out_dir: Path, filter_pattern: Optional[str]) -> list:
    """Return list of test names via --gtest_list_tests."""
    cmd = [str(binary), "--gtest_list_tests"]
    if filter_pattern:
        cmd.append(f"--gtest_filter={filter_pattern}")
    result = subprocess.run(cmd, cwd=str(out_dir), capture_output=True, text=True)
    tests = []
    current_suite = None
    for line in result.stdout.splitlines():
        if line.endswith("."):
            current_suite = line.rstrip(".")
        elif line.startswith("  ") and current_suite:
            test_name = line.strip().split(" ")[0]  # strip any trailing comments
            tests.append(f"{current_suite}.{test_name}")
    return tests


def _split(items: list, n: int) -> list:
    """Split items into n roughly equal chunks."""
    if n <= 0:
        return [items]
    k, rem = divmod(len(items), n)
    chunks, start = [], 0
    for i in range(n):
        end = start + k + (1 if i < rem else 0)
        chunks.append(items[start:end])
        start = end
    return [c for c in chunks if c]


def _run_shard(binary: Path, out_dir: Path, shard_filter: str, xml_path: Path, verbose: bool) -> int:
    cmd = [
        str(binary),
        f"--gtest_filter={shard_filter}",
        f"--gtest_output=xml:{xml_path}",
    ]
    if verbose:
        cmd.append("--gtest_print_time=1")
    result = subprocess.run(cmd, cwd=str(out_dir))
    return result.returncode


def run_shards(
    binary: Path,
    out_dir: Path,
    repo_root: Path,
    num_shards: int,
    filter_pattern: Optional[str],
    run_all: bool,
    verbose: bool,
) -> int:
    effective_filter = filter_pattern if filter_pattern else (None if run_all else "-SLOW_*")
    tests = _list_tests(binary, out_dir, effective_filter)
    if not tests:
        print("WARNING: no tests found for sharding; falling back to single run")
        return 1

    actual_shards = min(num_shards, len(tests))
    chunks = _split(tests, actual_shards)

    out_base = repo_root / "Cluiche" / "out" / "GoogleTests"
    out_base.mkdir(parents=True, exist_ok=True)

    shard_xml_paths = [out_base / f"shard_{i}.xml" for i in range(len(chunks))]

    exit_codes = []
    with ThreadPoolExecutor(max_workers=len(chunks)) as pool:
        futures = {
            pool.submit(
                _run_shard,
                binary,
                out_dir,
                ":".join(c),
                shard_xml_paths[i],
                verbose,
            ): i
            for i, c in enumerate(chunks)
        }
        for future in as_completed(futures):
            exit_codes.append(future.result())

    merged_path = out_base / "merged.xml"
    merge_xml(shard_xml_paths, merged_path)

    return 0 if all(c == 0 for c in exit_codes) else 1
