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
- `--filter=<pattern>`: Run tests matching GoogleTest filter (e.g., `--filter=*Threading*`)
- `--rerun-failed`: Parse last results and rerun only failed tests
- `--dirty`: Run only tests for files that changed (uses `Tools/run_dirty_tests.py`)
- `--build-only`: Build without running
- `--no-build`: Skip build and just run tests
- `--config=<Debug|Release>`: Build configuration (default: Debug)
- `--verbose`: Show full GoogleTest output

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
/gtest --filter=InputSystem*

# Rerun only what failed last time
/gtest --rerun-failed

# Build without running
/gtest --build-only

# Quick rerun without rebuilding
/gtest --no-build

# Run tests for changed files only
/gtest --dirty
```

## Instructions for Claude

When this skill is invoked:

1. **Parse the arguments** to determine build/run behavior
2. **Build the project** (unless `--no-build`):
   ```bash
   msbuild Cluiche/Tests/GoogleTests/GoogleTests.vcxproj /p:Configuration=<config> /p:Platform=Win32 /nologo /v:minimal
   ```
3. **Determine test filter**:
   - If `--filter=X`: Use that filter
   - If `--rerun-failed`: Read `Cluiche/Tests/GoogleTests/last_test_results.txt`, parse failed test names, build filter
   - If `--dirty`: Run `python Tools/run_dirty_tests.py` to get filter
   - Otherwise: Run all tests
4. **Run tests** (unless `--build-only`):
   ```bash
   Cluiche/bin/exe/<config>/GoogleTests.exe [--gtest_filter=<filter>]
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

- If build fails: Show MSBuild error, don't run tests
- If test exe doesn't exist: Show path issue, suggest building
- If dirty test tracker fails: Fall back to running all tests
- If no tests match filter: Report it clearly

### Smart Defaults

- Always use `/nologo /v:minimal` for MSBuild (less noise)
- Default to Debug configuration (matches most dev work)
- Save results automatically for easy `--rerun-failed`
- Parse output even if tests fail (non-zero exit is expected)
