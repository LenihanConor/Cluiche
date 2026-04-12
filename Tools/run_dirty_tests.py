#!/usr/bin/env python3
"""
Run GoogleTests - dirty tests only or all tests.

Reads dirty_tests.json and executes GoogleTests.exe with --gtest_filter.
"""

import argparse
import json
import subprocess
import sys
from pathlib import Path


def main(argv):
    parser = argparse.ArgumentParser(
        description='Run GoogleTests (dirty only or all)',
        epilog='Examples:\n'
               '  Run dirty tests: %(prog)s --exe path/to/GoogleTests.exe\n'
               '  Run all tests:   %(prog)s --exe path/to/GoogleTests.exe --all\n',
        formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument('--exe', required=True, help='Path to GoogleTests.exe')
    parser.add_argument('--all', action='store_true', help='Run all tests (ignore dirty tracking)')
    parser.add_argument('--output', help='Output file for test results')
    parser.add_argument('--dirty-json', help='Path to dirty_tests.json (auto-detected if not specified)')
    args = parser.parse_args(argv)

    exe_path = Path(args.exe).resolve()
    if not exe_path.exists():
        print(f"Error: Test executable not found: {exe_path}", file=sys.stderr)
        return 1

    # Build command
    cmd = [str(exe_path)]

    if not args.all:
        # Load dirty tests
        if args.dirty_json:
            dirty_json = Path(args.dirty_json)
        else:
            # Auto-detect: .dirty/dirty_tests.json relative to exe
            # Exe is at: Cluiche/bin/exe/Debug/GoogleTests.exe
            # Tests are at: Cluiche/Tests/GoogleTests/
            # Go up 4 levels: Debug -> exe -> bin -> Cluiche
            cluiche_root = exe_path.parent.parent.parent.parent
            test_dir = cluiche_root / "Tests" / "GoogleTests"
            dirty_json = test_dir / ".dirty" / "dirty_tests.json"

        if dirty_json.exists():
            try:
                with open(dirty_json) as f:
                    data = json.load(f)
                    gtest_filter = data.get("gtest_filter", "*")

                    if gtest_filter and gtest_filter != "*":
                        cmd.append(f"--gtest_filter={gtest_filter}")
                        stats = data.get("stats", {})
                        dirty_count = stats.get("dirty_tests", 0)
                        total_count = stats.get("total_tests", 389)
                        print(f"[RunDirtyTests] Running dirty tests: {dirty_count} patterns")
                        print(f"[RunDirtyTests] Modules: {', '.join(data.get('changed_modules', []))}")
                        if data.get("dirty_tests", {}).get("by_failure"):
                            print(f"[RunDirtyTests] + {len(data['dirty_tests']['by_failure'])} failed tests")
                        if data.get("dirty_tests", {}).get("by_disabled"):
                            print(f"[RunDirtyTests] + {len(data['dirty_tests']['by_disabled'])} disabled tests")
                    else:
                        print("[RunDirtyTests] No dirty tests found, running all tests")
            except Exception as e:
                print(f"[RunDirtyTests] Warning: Could not read dirty_tests.json: {e}", file=sys.stderr)
                print("[RunDirtyTests] Running all tests")
        else:
            print(f"[RunDirtyTests] No dirty_tests.json found at {dirty_json}")
            print("[RunDirtyTests] Running all tests")
    else:
        print("[RunDirtyTests] Running all tests (--all specified)")

    print(f"[RunDirtyTests] Command: {' '.join(cmd)}")
    print()

    # Run tests
    if args.output:
        output_file = Path(args.output)
        with open(output_file, 'w') as f:
            result = subprocess.run(cmd, stdout=f, stderr=subprocess.STDOUT)
        print(f"\n[RunDirtyTests] Results saved to: {output_file}")

        # Also save to .dirty/last_run_results.txt for next tracking cycle
        if not args.all and dirty_json and dirty_json.exists():
            results_file = dirty_json.parent / "last_run_results.txt"
            try:
                with open(output_file) as src, open(results_file, 'w') as dst:
                    dst.write(src.read())
                print(f"[RunDirtyTests] Results copied to: {results_file}")
            except Exception as e:
                print(f"[RunDirtyTests] Warning: Could not save results for tracking: {e}", file=sys.stderr)
    else:
        result = subprocess.run(cmd)

        # If no output file specified but we have dirty tracking, still capture results
        if not args.all and dirty_json and dirty_json.exists():
            results_file = dirty_json.parent / "last_run_results.txt"
            print(f"\n[RunDirtyTests] Note: To track test failures, run with --output flag")
            print(f"[RunDirtyTests]   Example: --output {results_file}")

    return result.returncode


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
