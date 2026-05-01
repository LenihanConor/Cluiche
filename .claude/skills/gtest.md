---
name: gtest
description: Build, run, and analyze GoogleTest results with iteration support
tags: [test, build, cpp, googletest]
user_invocable: true
agent_invocable: false
---

# GoogleTest Build & Run Skill

Build the GoogleTests project, run tests, and parse results for quick iteration.

## Usage

```
/gtest [options]
```

## Options

- No args: Build and run all tests
- `--filter=<pattern>`: Run tests matching GoogleTest filter (e.g., `--filter=ThreadPool*`)
- `--rerun-failed`: Parse last results and rerun only failed tests
- `--build-only`: Build without running
- `--no-build`: Skip build and just run tests
- `--config=<Debug|Release>`: Build configuration (default: Debug)
- `--verbose`: Show full GoogleTest output
- `--list`: List available tests without running them

## What This Skill Does

1. **Build**: Compile GoogleTests.vcxproj using MSBuild
2. **Run**: Execute GoogleTests.exe with any filters
3. **Parse**: Extract and highlight test failures clearly
4. **Report**: Show summary with pass/fail counts and failed test names
5. **Save**: Store full results to `Cluiche/Tests/GoogleTests/last_test_results.txt` for `--rerun-failed`

## Examples

```bash
# Build and run all tests
/gtest

# Run specific test suite
/gtest --filter=ThreadPool*

# Run specific test case
/gtest --filter=ThreadPool.Constructor

# Rerun only what failed last time
/gtest --rerun-failed

# Build without running
/gtest --build-only

# Quick rerun without rebuilding
/gtest --no-build

# List available tests
/gtest --list

# Release build and run
/gtest --config=Release
```

## Instructions for Claude

When this skill is invoked:

1. **Parse the arguments** to determine build/run behavior
2. **Build the project** (unless `--no-build`):
   ```bash
   msbuild Cluiche/Tests/GoogleTests/GoogleTests.vcxproj /p:Configuration=<config> /p:Platform=x64 /nologo /v:minimal
   ```
3. **Determine test filter**:
   - If `--filter=X`: Use that filter directly with `--gtest_filter=X`
   - If `--rerun-failed`: Read `Cluiche/Tests/GoogleTests/last_test_results.txt`, parse failed test names, build filter
   - If `--list`: Use `--gtest_list_tests` instead of running
   - Otherwise: Run all tests (no filter)
4. **Run tests** (unless `--build-only`):
   ```bash
   Cluiche/bin/<config>/x64/GoogleTests.exe [--gtest_filter=<filter>]
   ```
5. **Parse GoogleTest output**:
   - Extract test results (PASSED/FAILED)
   - Find failure messages and stack traces
   - Count totals
6. **Display results**:
   - Show summary: X tests, Y passed, Z failed
   - If failures exist, list each failed test with its error message
   - Show a concise, actionable view (not raw output unless `--verbose`)
7. **Save results**: Write full output to `Cluiche/Tests/GoogleTests/last_test_results.txt`

### Output Format

For failures, show:
```
❌ FAILED: 3 of 145 tests

1. TestSuiteName.TestName
   Expected: value == 5
   Actual: value = 3
   At: path/to/file.cpp:123

2. AnotherSuite.AnotherTest
   ...
```

For success:
```
✅ PASSED: All 145 tests passed
```

### Error Handling

- **Build fails**: Show the actual MSBuild error (not just "build failed"), highlight the specific file/line if available
- **Test exe doesn't exist**: Show expected path `Cluiche/bin/exe/<config>/GoogleTests.exe`, suggest running with `--no-build` removed
- **No tests match filter**: Show "No tests matched filter '<pattern>'. Use --list to see available tests."
- **Zero tests found**: Likely means GoogleTests.exe wasn't built correctly - show build status
- **MSBuild not found**: User may need to run from Developer Command Prompt or ensure MSBuild is in PATH

### Smart Defaults

- Always use `/nologo /v:minimal` for MSBuild (less noise)
- Default to Debug configuration (matches most dev work)
- Save results automatically to `Cluiche/Tests/GoogleTests/last_test_results.txt` for easy `--rerun-failed`
- Parse output even if tests fail (non-zero exit is expected with GTest failures)
- When filtering tests, show how many tests were selected vs total available
- For build errors, check if DiaCore or other dependencies need rebuilding

### Tips for Better Results

- **When build fails frequently**: Dependencies (DiaCore, DiaMaths) may be out of date - suggest rebuilding solution
- **Filter syntax**: `*` matches anything, `?` matches one char, `:` separates multiple patterns
  - `ThreadPool*` matches all ThreadPool tests
  - `ThreadPool.Constructor:ThreadPool.Destructor` matches exactly those two
  - `*Async*:*Thread*` matches tests with "Async" OR "Thread" in name
- **Incremental workflow**: After fixing a test, use `--no-build` if you know dependencies haven't changed
- **Finding test names**: Use `--list` to see all available test names, helpful for building filters
