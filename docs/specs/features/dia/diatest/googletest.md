# Feature Spec: googletest

## Parent System
@docs/specs/systems/dia/diatest.md

## Status
`Done`

## Summary

Implement `dia test googletest` — runs the built `GoogleTests.exe` binary and streams output to the terminal. Assumes the binary has already been built by `dia pipeline --stage compile-code --target googletest`. Fails with a clear message if the binary is not found. Supports `--filter` (maps to `--gtest_filter`), `--config` (Debug or Release), and `--docker` (re-invokes inside the container).

## Problem

Running the C++ GoogleTest suite currently requires knowing the exact path to `GoogleTests.exe` and the correct `--gtest_filter` syntax. `dia test googletest` provides a single, consistent entry point that fits into the `dia test` command surface and the `env-integration` loop.

## Goals

- Locate `GoogleTests.exe` at `Cluiche/bin/<config>/x64/GoogleTests.exe` per `Directory.Build.props` convention
- Run the binary and stream stdout/stderr to the terminal in real time
- Exit with the binary's exit code (0 all-pass, 1 any-fail)
- Exit 2 with a clear message if the binary is not found at the expected path
- `--filter <pattern>` maps to `--gtest_filter=<pattern>`
- `--config Debug|Release` selects which built binary to run (default: Debug)
- `--docker` re-invokes inside the Docker container via `docker run` (same pattern as `dia pipeline --docker`)
- `--verbose` passes `--gtest_verbose` to GoogleTest for detailed per-test output

## Non-Goals

- Building the binary — that is `dia pipeline --stage compile-code --target googletest`
- Parsing or reformatting GoogleTest XML output — raw terminal output only
- Parallel test execution across multiple binaries
- Test result persistence between runs

## CLI Interface

```bash
# Run all tests (Debug binary)
dia test googletest

# Run specific tests by filter
dia test googletest --filter "TestArray*"
dia test googletest --filter "TestComponent/TestSingleton"

# Select Release binary
dia test googletest --config Release

# Verbose output
dia test googletest --verbose

# Run inside Docker container
dia test googletest --docker

# Combined
dia test googletest --docker --config Release --filter "TestPhaseTransition*"
```

## Binary Location

`GoogleTests.exe` is expected at:
```
<repo_root>/Cluiche/bin/<config>/x64/GoogleTests.exe
```

For `--config Debug` (the default): `Cluiche/bin/Debug/x64/GoogleTests.exe`
For `--config Release`: `Cluiche/bin/Release/x64/GoogleTests.exe`

If the binary is not found: exit 2 with:
```
GoogleTests.exe not found at Cluiche/bin/Debug/x64/GoogleTests.exe
Run: dia pipeline --stage compile-code --target googletest --config Debug
```

## Pre-Run Dependency Check

Before invoking the binary, check that `python311.dll` exists in the output directory:

```python
out_dir = binary_path.parent
if not (out_dir / "python311.dll").exists():
    logger.error(
        f"Runtime dependencies not staged in {out_dir}\n"
        f"Run: dia pipeline --stage package --target googletest --config {config}"
    )
    return 2
```

## GoogleTest Invocation

```python
cmd = [str(binary_path)]
if filter_pattern:
    cmd.append(f"--gtest_filter={filter_pattern}")
if verbose:
    cmd.append("--gtest_verbose")

result = subprocess.run(cmd, cwd=str(binary_path.parent))
return result.returncode
```

The working directory is set to `binary_path.parent` so that the binary can find its staged runtime files (DLLs, manifests) in the same directory.

## `--docker` Behaviour

Follows the same pattern as `dia pipeline --docker`:

1. Check Docker image exists; exit 3 if not
2. Re-invoke `dia test googletest` inside the container with all flags forwarded (minus `--docker`)
3. Volume-mount the repo root at `C:/repo`
4. Stream container stdout/stderr to the host terminal in real time

```python
docker_cmd = [
    "docker", "run", "--rm",
    "--volume", f"{repo_root}:C:/repo",
    "--workdir", "C:/repo",
    image_name,
    "python", "-m", "dia_cli",
    "test", "googletest",
] + forwarded_args
```

## Implementation

### Files introduced

```
Dia/DiaCLI/
└── dia_cli/
    ├── cli/
    │   └── test/
    │       └── googletest_cmd.py        # Click command: dia test googletest
    └── commands/
        └── test/
            └── googletest_runner.py     # Binary location, invocation, docker re-invoke
```

### `googletest_runner.py` responsibilities

- `find_binary(repo_root, config)` — resolve path, return `Path | None`
- `run(repo_root, config, filter_pattern, verbose, docker, image_name)` — orchestrate location check + invocation or docker re-invoke

## Dependencies

| Dependency | Type | Notes |
|------------|------|-------|
| `pipeline-config` feature (DiaPipeline) | Soft | `OutDir` convention — path hardcoded to `Cluiche/bin/<config>/x64/` |
| `compile-code` feature (DiaPipeline) | Runtime | Binary must be built before this command is run |
| `docker-build-env` feature (DiaEnv) | Hard (--docker only) | Container must exist for `--docker` |
| `subprocess` (stdlib) | Python | |

## Acceptance Criteria

1. `dia test googletest` runs `Cluiche/bin/Debug/x64/GoogleTests.exe` and exits with the binary's exit code
2. All test output streams to the terminal in real time
3. `--filter "TestArray*"` passes `--gtest_filter=TestArray*` to the binary; only matching tests run
4. `--config Release` runs `Cluiche/bin/Release/x64/GoogleTests.exe`
5. If the binary does not exist, exits 2 with the path and a `dia pipeline compile-code` fix command
6. If `python311.dll` is not found alongside the binary, exits 2 with a `dia pipeline package` fix command
7. `--docker` re-invokes inside the container; all flags are forwarded; container output streams to host
8. If Docker image is not found when using `--docker`, exits 3 with a clear message
9. `--verbose` passes `--gtest_verbose` to the binary

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaTest | @docs/specs/systems/dia/diatest.md |

## Binding Decisions Compliance

| ID | Source | Decision | Compliance |
|----|--------|----------|------------|
| PD-001 | Platform | Use StringCRC for all identifiers | Compliant — Python tooling only; no C++ identifiers introduced |
| PD-002 | Platform | ProcessingUnit/Phase/Module architecture | Compliant — test tooling; no runtime components |
| PD-003 | Platform | Component-based entities | Compliant — test tooling |
| PD-004 | Platform | No STL containers in public APIs | Compliant — Python only |
| PD-005 | Platform | x64 Windows only | Compliant — binary path hardcodes `x64`; no cross-platform support |
| PD-006 | Platform | VS project files are source of truth | Compliant — no `.vcxproj` files touched |
| PD-007 | Platform | C++20 required | Compliant — test tooling; does not affect compiler configuration |
| PD-008 | Platform | `Directory.Build.props` owns OutDir/IntDir | Compliant — binary path mirrors `Directory.Build.props` `OutDir` convention; not overridden |
| AD-001 | Dia App | Module system with YAML frontmatter | Compliant — Python tooling only |
| SD-CLI-001 | DiaCLI | MDK CLI architecture | Compliant — two-file plugin pattern |
| SD-CLI-002 | DiaCLI | Python-based implementation | Compliant |
| SD-CLI-006 | DiaCLI | Click framework | Compliant |
| SD-CLI-008 | DiaCLI | Exit codes follow Unix conventions | Compliant — 0 all-pass, 1 any-fail (propagated from GoogleTest), 2 binary not found, 3 Docker not found |
| SD-TEST-001 | DiaTest | All tests run inside Docker container | Compliant — `--docker` flag enables container execution; host execution is also supported for local dev |
| SD-TEST-004 | DiaTest | Unit tests use mocks; no real network/filesystem | Compliant — googletest_runner.py unit tests mock subprocess and filesystem |
| SD-ENV-008 | DiaEnv | Docker scope is headless build + test only | Compliant — GoogleTests is headless; no GPU/GUI required |
| SD-ENV-010 | DiaEnv | Python 3.11 | Compliant |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Binary path | The existing binary is at `Cluiche/Tests/GoogleTests/bin/exe/Debug/` — does `dia pipeline compile-code` change this to `Cluiche/bin/Debug/x64/`? | Standard path only — `dia test googletest` looks exclusively at `Cluiche/bin/<config>/x64/GoogleTests.exe`. The legacy `bin/exe/` path is not checked. This requires `dia pipeline compile-code` to have run first. Using the legacy path would allow skipping the pipeline and create two sources of truth for the binary location. |
| 2 | Runtime deps | `GoogleTests.exe` requires `python311.dll` and `.diaapp` manifests in the same directory. Does `dia test googletest` ensure these are present? | Pre-run check — before invoking the binary, `dia test googletest` checks that `python311.dll` exists in the output directory. If missing, exits 2 with "Runtime dependencies not staged. Run: dia pipeline --stage package --target googletest --config <config>". This is more actionable than the cryptic Windows DLL load failure that would otherwise appear. |
| 3 | `--docker` and SD-TEST-001 | SD-TEST-001 says all tests run inside Docker — but the spec allows host execution. Is this a conflict? | No conflict — SD-TEST-001 is the target state for the `env-integration` loop. `dia test googletest` on the host is a developer convenience. The `--docker` flag is the path used by `env-integration`. Both are valid. |
