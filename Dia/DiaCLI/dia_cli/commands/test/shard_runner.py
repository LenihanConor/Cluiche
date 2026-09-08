"""Run GoogleTests in parallel shards and merge results."""
import os
import subprocess
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path
from typing import Optional

from .xml_merger import merge_xml



def _run_shard(binary: Path, out_dir: Path, shard_index: int, total_shards: int,
               xml_path: Path, filter_pattern: Optional[str], run_all: bool, verbose: bool) -> int:
    cmd = [str(binary), f"--gtest_output=xml:{xml_path}"]
    if filter_pattern:
        cmd.append(f"--gtest_filter={filter_pattern}")
    elif not run_all:
        cmd.append("--gtest_filter=-SLOW_*")
    if verbose:
        cmd.append("--gtest_print_time=1")
    env = os.environ.copy()
    env["GTEST_TOTAL_SHARDS"] = str(total_shards)
    env["GTEST_SHARD_INDEX"] = str(shard_index)
    try:
        result = subprocess.run(cmd, cwd=str(out_dir), env=env, timeout=300)
        return result.returncode
    except subprocess.TimeoutExpired:
        print(f"ERROR: shard {shard_index} timed out after 300s")
        return 1


def run_shards(
    binary: Path,
    out_dir: Path,
    repo_root: Path,
    num_shards: int,
    filter_pattern: Optional[str],
    run_all: bool,
    verbose: bool,
) -> int:
    out_base = repo_root / "Cluiche" / "out" / "GoogleTests"
    out_base.mkdir(parents=True, exist_ok=True)

    shard_xml_paths = [out_base / f"shard_{i}.xml" for i in range(num_shards)]

    exit_codes = []
    with ThreadPoolExecutor(max_workers=num_shards) as pool:
        futures = {
            pool.submit(
                _run_shard,
                binary,
                out_dir,
                i,
                num_shards,
                shard_xml_paths[i],
                filter_pattern,
                run_all,
                verbose,
            ): i
            for i in range(num_shards)
        }
        for future in as_completed(futures):
            exit_codes.append(future.result())

    merged_path = out_base / "merged.xml"
    merge_xml(shard_xml_paths, merged_path)

    return 0 if all(c == 0 for c in exit_codes) else 1
