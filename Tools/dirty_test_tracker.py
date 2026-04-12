#!/usr/bin/env python3
"""
Dirty Test Tracker for GoogleTests

Tracks failing, disabled, and change-affected tests at top-level module granularity.
Runs as PreBuildEvent in MSBuild.
"""

import argparse
import json
import os
import re
import subprocess
import sys
from datetime import datetime
from pathlib import Path


# Hardcoded module dependencies
MODULE_DEPENDENCIES = {
    "Core": [],
    "Maths": [],
    "Graphics": ["Core", "Maths"],
    "Input": ["Core"]
}


def get_git_info(repo_root):
    """Get current git commit and branch."""
    try:
        commit = subprocess.check_output(
            ["git", "-C", str(repo_root), "rev-parse", "--short", "HEAD"],
            text=True,
            stderr=subprocess.DEVNULL
        ).strip()

        branch = subprocess.check_output(
            ["git", "-C", str(repo_root), "rev-parse", "--abbrev-ref", "HEAD"],
            text=True,
            stderr=subprocess.DEVNULL
        ).strip()

        return commit, branch
    except (subprocess.CalledProcessError, FileNotFoundError):
        return "unknown", "unknown"


def get_changed_files(repo_root, baseline_commit):
    """Get list of changed files since baseline (committed + uncommitted)."""
    try:
        # Get committed changes since baseline
        committed = subprocess.check_output(
            ["git", "-C", str(repo_root), "diff", "--name-only", baseline_commit, "HEAD"],
            text=True,
            stderr=subprocess.DEVNULL
        ).strip()

        # Get uncommitted changes in working directory (staged + unstaged)
        uncommitted = subprocess.check_output(
            ["git", "-C", str(repo_root), "diff", "--name-only", "HEAD"],
            text=True,
            stderr=subprocess.DEVNULL
        ).strip()

        # Combine both sets
        all_files = set()
        if committed:
            all_files.update(line.strip() for line in committed.splitlines() if line.strip())
        if uncommitted:
            all_files.update(line.strip() for line in uncommitted.splitlines() if line.strip())

        return list(all_files)
    except (subprocess.CalledProcessError, FileNotFoundError):
        # No baseline or git unavailable - mark all dirty
        return None


def map_file_to_module(file_path):
    """Map a source file to top-level module (Core, Maths, Graphics, Input)."""
    file_path = file_path.replace("\\", "/")

    if file_path.startswith("Dia/DiaCore/"):
        return "Core"
    elif file_path.startswith("Dia/DiaMaths/"):
        return "Maths"
    elif file_path.startswith("Dia/DiaGraphics/"):
        return "Graphics"
    elif file_path.startswith("Dia/DiaInput/"):
        return "Input"

    return None


def get_dirty_modules(changed_files):
    """Get set of dirty modules including dependencies.

    Returns:
        tuple: (dirty_modules_set, file_mappings, dependency_additions)
    """
    file_mappings = []  # List of {file, module, reason}
    dependency_additions = []  # List of {module, added_because_of, dependencies}

    if changed_files is None:
        # Git unavailable or baseline invalid - all modules dirty
        return set(MODULE_DEPENDENCIES.keys()), [], []

    directly_dirty = set()

    # Map changed files to modules
    for file in changed_files:
        module = map_file_to_module(file)
        if module:
            directly_dirty.add(module)
            file_mappings.append({
                "file": file,
                "mapped_to_module": module,
                "reason": "file_in_module"
            })
        else:
            file_mappings.append({
                "file": file,
                "mapped_to_module": None,
                "reason": "not_in_tracked_modules"
            })

    # Add dependent modules
    result = set(directly_dirty)
    for module in directly_dirty:
        # Find modules that depend on this module
        for dep_module, deps in MODULE_DEPENDENCIES.items():
            if module in deps and dep_module not in directly_dirty:
                result.add(dep_module)
                dependency_additions.append({
                    "module": dep_module,
                    "added_because_of": module,
                    "dependency_chain": f"{dep_module} depends on {module}"
                })

    return result, file_mappings, dependency_additions


def parse_test_results(results_file):
    """Parse GoogleTest results for failures, passes, and test suites run.

    Returns:
        tuple: (failed_tests, passed_tests, test_suites_run, suites_with_failures)
    """
    if not results_file.exists():
        return [], [], set(), set()

    failed = []
    passed = []
    test_suites_run = set()
    suites_with_failures = set()

    try:
        with open(results_file, encoding='utf-8', errors='ignore') as f:
            for line in f:
                if "[  FAILED  ]" in line:
                    # Match: [  FAILED  ] SuiteName.TestName
                    match = re.search(r'\[\s+FAILED\s+\]\s+(\w+)\.(\w+)', line)
                    if match:
                        suite_name = match.group(1)
                        test_name = match.group(2)
                        failed.append(f"{suite_name}.{test_name}")
                        test_suites_run.add(suite_name)
                        suites_with_failures.add(suite_name)
                elif "[       OK ]" in line:
                    # Match: [       OK ] SuiteName.TestName
                    match = re.search(r'\[\s+OK\s+\]\s+(\w+)\.(\w+)', line)
                    if match:
                        suite_name = match.group(1)
                        test_name = match.group(2)
                        passed.append(f"{suite_name}.{test_name}")
                        test_suites_run.add(suite_name)
    except Exception as e:
        print(f"[DirtyTestTracker] Warning: Could not parse test results: {e}", file=sys.stderr)

    return failed, passed, test_suites_run, suites_with_failures


def find_disabled_tests(test_dir):
    """Find DISABLED_ tests in source files."""
    disabled = []

    try:
        for test_file in test_dir.rglob("*.cpp"):
            if test_file.name == "Main.cpp":
                continue

            try:
                with open(test_file, encoding='utf-8', errors='ignore') as f:
                    content = f.read()
                    # Match: TEST(Suite, DISABLED_Name) or TEST_F(Suite, DISABLED_Name)
                    matches = re.findall(r'TEST(?:_F)?\s*\(\s*(\w+)\s*,\s*(DISABLED_\w+)\s*\)', content)
                    disabled.extend([f"{suite}.{test}" for suite, test in matches])
            except Exception:
                # Skip files that can't be read
                continue
    except Exception as e:
        print(f"[DirtyTestTracker] Warning: Could not scan for disabled tests: {e}", file=sys.stderr)

    return disabled


def get_test_suites_in_dir(test_dir, module_name):
    """Get all test suites in a module directory (e.g., Core, Maths, Graphics, Input)."""
    suites = set()
    module_dir = test_dir / module_name

    if not module_dir.exists():
        return suites

    try:
        for test_file in module_dir.rglob("*.cpp"):
            if test_file.name == "Main.cpp":
                continue

            try:
                with open(test_file, encoding='utf-8', errors='ignore') as f:
                    content = f.read()
                    # Match: TEST(Suite, Name) or TEST_F(Suite, Name)
                    matches = re.findall(r'TEST(?:_F)?\s*\(\s*(\w+)\s*,\s*\w+\s*\)', content)
                    suites.update(matches)
            except Exception:
                # Skip files that can't be read
                continue
    except Exception as e:
        print(f"[DirtyTestTracker] Warning: Could not scan test suites in {module_name}: {e}", file=sys.stderr)

    return suites


def main(argv):
    parser = argparse.ArgumentParser(description='Track dirty tests for GoogleTests')
    parser.add_argument('--project-dir', required=True, help='GoogleTests project directory')
    parser.add_argument('--config', default='Debug', help='Build configuration')
    parser.add_argument('--force-all', action='store_true', help='Mark all tests as dirty')
    args = parser.parse_args(argv)

    # Normalize the project directory path (handles trailing backslash, extra dots, etc.)
    project_dir = Path(args.project_dir).resolve()
    repo_root = project_dir.parent.parent.parent  # Go up to Cluiche root

    # Create .dirty tracking directory
    dirty_dir = project_dir / ".dirty"
    dirty_dir.mkdir(exist_ok=True)

    baseline_file = dirty_dir / "baseline.txt"
    dirty_json_file = dirty_dir / "dirty_tests.json"
    results_file = dirty_dir / "last_run_results.txt"

    # Get git info
    current_commit, current_branch = get_git_info(repo_root)

    # Load baseline
    if baseline_file.exists() and not args.force_all:
        baseline_commit = baseline_file.read_text().strip()
    else:
        baseline_commit = current_commit
        baseline_file.write_text(baseline_commit)

    # Detect changes
    changed_files = get_changed_files(repo_root, baseline_commit)
    if args.force_all:
        changed_files = None

    dirty_modules, file_mappings, dependency_additions = get_dirty_modules(changed_files)

    # Build test patterns - get actual test suite names from each dirty module
    test_patterns = []
    module_to_suites = {}
    for module in sorted(dirty_modules):
        suites = get_test_suites_in_dir(project_dir, module)
        module_to_suites[module] = sorted(suites)
        # Add wildcard pattern for each suite (e.g., "RGBA.*")
        test_patterns.extend([f"{suite}.*" for suite in sorted(suites)])

    # Parse test results (failures, passes, and which suites ran)
    failed_tests, passed_tests, test_suites_run, suites_with_failures = parse_test_results(results_file)

    # Load previous failed tests
    previous_failed_file = dirty_dir / "previous_failed.json"
    if previous_failed_file.exists():
        try:
            with open(previous_failed_file) as f:
                previous_failed = json.load(f)
        except Exception:
            previous_failed = []
    else:
        previous_failed = []

    # Combine: keep previous failures that haven't been tested yet + new failures
    # Remove tests that were previously failed but now passed
    all_failed = set(previous_failed) | set(failed_tests)
    cleaned_failed_tests = set(previous_failed) & set(passed_tests)
    all_failed -= set(passed_tests)  # Remove any that passed
    failed_tests = sorted(all_failed)

    # Save failed tests for next run
    with open(previous_failed_file, 'w') as f:
        json.dump(failed_tests, f)

    # Clean test patterns for suites that were run and had no failures
    cleaned_patterns = []
    remaining_patterns = []
    for pattern in test_patterns:
        # Extract suite name from pattern (e.g., "Matrix22.*" -> "Matrix22")
        suite_name = pattern.rstrip(".*")
        if suite_name in test_suites_run and suite_name not in suites_with_failures:
            # This suite was run and had no failures - clean it!
            cleaned_patterns.append(pattern)
        else:
            # Keep it dirty (not run yet or had failures)
            remaining_patterns.append(pattern)

    test_patterns = remaining_patterns

    # Report cleaning
    total_cleaned = len(cleaned_failed_tests) + len(cleaned_patterns)
    if cleaned_failed_tests:
        print(f"[DirtyTestTracker] Cleaned {len(cleaned_failed_tests)} failed tests that now pass: {', '.join(sorted(cleaned_failed_tests))}")
    if cleaned_patterns:
        print(f"[DirtyTestTracker] Cleaned {len(cleaned_patterns)} test suites that passed: {', '.join(sorted(cleaned_patterns))}")

    # Find disabled tests
    disabled_tests = find_disabled_tests(project_dir)

    # Build gtest_filter
    all_filters = test_patterns + failed_tests + disabled_tests
    gtest_filter = ":".join(all_filters) if all_filters else "*"

    # Sample of changed files (for debugging)
    changed_files_sample = (changed_files[:5] if changed_files else [])

    # Filter file mappings to only show code changes (not docs, etc.)
    code_file_mappings = [m for m in file_mappings if m["mapped_to_module"] is not None]
    ignored_files_count = len(file_mappings) - len(code_file_mappings)

    # Generate simple run command
    exe_path_debug = "../../bin/exe/Debug/GoogleTests.exe"
    run_command = f'{exe_path_debug} --gtest_filter="{gtest_filter}"' if gtest_filter and gtest_filter != "*" else exe_path_debug

    # Generate JSON
    output = {
        "_help": {
            "description": "Dirty test tracking for GoogleTests - generated at build time",
            "usage": "Just run GoogleTests.exe (dirty filter is automatic). Override with --gtest_filter=* for all tests.",
            "reset_baseline": "git rev-parse --short HEAD > .dirty/baseline.txt",
            "sections": {
                "mapping": "Shows how CODE files were mapped to modules and dependencies applied",
                "changed_modules": "List of all dirty modules (directly changed + dependencies)",
                "module_test_suites": "Test suites found in each dirty module",
                "dirty_tests": "Categorized list of dirty tests (by_change, by_failure, by_disabled)",
                "gtest_filter": "Ready-to-use --gtest_filter string for GoogleTest",
                "stats": "Summary statistics"
            }
        },

        "version": "1.0",
        "timestamp": datetime.utcnow().isoformat() + "Z",
        "git_commit": current_commit,
        "git_branch": current_branch,
        "baseline_commit": baseline_commit,
        "tracking_mode": "top-level-modules",

        "summary": f"{len(test_patterns) + len(failed_tests) + len(disabled_tests)} dirty tests ({len(test_patterns)} from changes, {len(failed_tests)} failed, {len(disabled_tests)} disabled) - AUTOMATIC on next run",
        "run_command": run_command,
        "_run_command_help": "This is applied automatically when you run GoogleTests.exe (no flags needed). Override with --gtest_filter=* for all tests.",

        "mapping": {
            "_description": "Detailed mapping showing how CODE files -> modules -> dependencies (non-code files like docs are ignored)",
            "code_files_changed": code_file_mappings,
            "_code_files_changed_help": "Only files in Dia/* that mapped to a module (Core/Maths/Graphics/Input)",
            "ignored_files_count": ignored_files_count,
            "_ignored_files_help": f"{ignored_files_count} files changed but not in tracked modules (docs, tools, configs, etc.)",
            "directly_dirty_modules": sorted([m for m in dirty_modules
                                             if not any(d["module"] == m for d in dependency_additions)]),
            "_directly_dirty_help": "Modules that changed directly (not added by dependencies)",
            "dependency_additions": dependency_additions,
            "_dependency_additions_help": "Modules added because they depend on directly dirty modules",
            "module_dependencies": MODULE_DEPENDENCIES,
            "_module_dependencies_help": "Hardcoded dependency map: Graphics depends on [Core, Maths], Input depends on [Core]"
        },

        "changed_modules": sorted(dirty_modules),
        "_changed_modules_help": "All dirty modules (directly changed + added by dependencies)",

        "module_test_suites": module_to_suites,
        "_module_test_suites_help": "Test suites discovered in each dirty module directory",

        "dirty_test_dirs": sorted(dirty_modules),
        "_dirty_test_dirs_help": "Deprecated: same as changed_modules",

        "dirty_tests": {
            "_description": "Categorized list of all dirty tests",
            "by_change": test_patterns,
            "_by_change_help": "Test patterns for changed modules (cleaned when all tests in suite pass)",
            "by_failure": sorted(failed_tests),
            "_by_failure_help": "Tests that failed in previous runs (cleaned when they pass)",
            "by_disabled": sorted(disabled_tests),
            "_by_disabled_help": "Tests marked with DISABLED_ prefix in source files",
            "cleaned_count": total_cleaned,
            "_cleaned_help": f"{total_cleaned} tests/suites removed: {len(cleaned_failed_tests)} failed tests + {len(cleaned_patterns)} test suites that passed"
        },

        "gtest_filter": gtest_filter,
        "_gtest_filter_help": "Pass this to GoogleTests.exe with --gtest_filter=<value>",

        "stats": {
            "_description": "Summary statistics",
            "total_tests": 389,
            "dirty_tests": len(test_patterns) + len(failed_tests) + len(disabled_tests),
            "changed_modules": len(dirty_modules),
            "directly_dirty": len([m for m in dirty_modules
                                  if not any(d["module"] == m for d in dependency_additions)]),
            "added_by_dependency": len(dependency_additions),
            "failed_tests": len(failed_tests),
            "disabled_tests": len(disabled_tests),
            "changed_files": len(changed_files) if changed_files else 0
        }
    }

    with open(dirty_json_file, 'w') as f:
        json.dump(output, f, indent=2)

    print(f"[DirtyTestTracker] Tracked {len(dirty_modules)} dirty modules: {', '.join(sorted(dirty_modules))}")
    print(f"[DirtyTestTracker] {len(failed_tests)} failed tests, {len(disabled_tests)} disabled tests")
    print(f"[DirtyTestTracker] Output: {dirty_json_file}")

    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
