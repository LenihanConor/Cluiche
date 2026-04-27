# Feature Spec: cli-unit-tests

## Parent System
@docs/specs/systems/dia/diatest.md

## Status
`Done`

## Summary

Implement `dia test cli` — a command that runs the pytest suite for all DiaCLI modules inside the Docker container. Tests use `unittest.mock` to stub filesystem, network, and subprocess calls so they run fast and without real network I/O. Covers all DiaEnv command modules as the first priority (`deps_restore.py`, `toolchain_verify.py`, `submodule_verify.py`, `setup_orchestrator.py`, `verify_orchestrator.py`, `claude_context_cmd.py`, `deps_cmd.py`), plus any other DiaCLI utility modules. pytest, pytest-xdist, and pytest-cov are already in `pyproject.toml` dev dependencies.

## Problem

The DiaEnv features introduce significant Python logic (sentinel detection, SHA-256 verification, mirror fallback, exit code propagation, submodule health checks) with no automated tests. Bugs in this logic will only surface at runtime on a real machine. Unit tests catch logic errors early, cheaply, and without needing a full environment.

## Goals

- `dia test cli` runs the full pytest suite inside the Docker container and exits 0 on all-pass
- Test structure: `Dia/DiaCLI/tests/` directory mirroring the module structure
- All tests use mocks — no real network calls, no real filesystem writes outside tmp, no real subprocess invocations
- Coverage report printed to stdout; optionally written to file with `--coverage-out`
- `--filter` flag to run a subset of tests (passed through to pytest `-k`)
- `--parallel` flag to run with `pytest-xdist` (`-n auto`)

## Non-Goals

- C++ GoogleTest execution — separate feature (`dia test googletest`)
- React/Vitest UI tests — separate feature (`dia test ui`)
- End-to-end tests that require a real environment — those belong to `env-integration`
- 100% coverage requirement — coverage is reported but not enforced as a gate

## CLI Interface

```bash
# Run all DiaCLI unit tests inside Docker
dia test cli

# Filter to a specific module or test name
dia test cli --filter deps_restore

# Run in parallel
dia test cli --parallel

# Write coverage report to file
dia test cli --coverage-out Cluiche/out/DiaCLI/logs/test/coverage.xml

# Combined
dia test cli --parallel --filter diaenv
```

## Test Structure

```
Dia/DiaCLI/
└── tests/
    ├── conftest.py                        # Shared fixtures: tmp_dir, mock_deps_json, mock_sentinel_dir
    ├── utils/
    │   ├── test_deps_restore.py           # deps_restore.py unit tests
    │   ├── test_toolchain_verify.py       # toolchain_verify.py unit tests
    │   ├── test_submodule_verify.py       # submodule_verify.py unit tests
    │   └── test_software_installer.py     # software_installer.py unit tests
    └── commands/
        └── env/
            ├── test_deps_restore_cmd.py   # CLI surface tests (Click test runner)
            ├── test_setup_orchestrator.py # setup orchestration logic tests
            ├── test_verify_orchestrator.py# verify orchestration logic tests
            └── test_claude_context_cmd.py # claude context generation + symlink tests
```

## Key Test Cases Per Module

### `deps_restore.py`
- Sentinel present → skip download (no network call made)
- Sentinel absent → download attempted
- `--force` → download even if sentinel present
- SHA-256 match → sentinel written, exit 0
- SHA-256 mismatch → partial file deleted, exit 1, no sentinel written
- Primary URL 404 → fallback to first mirror
- All mirrors fail → exit 1 with all URLs listed
- `file://` mirror → `shutil.copy` called, not `requests`
- Partial `.tmp` file present at start → deleted before re-download
- Concurrent calls → atomic sentinel write (temp rename) prevents race

### `toolchain_verify.py`
- `vswhere.exe` not found → FAIL for VS + workload checks, no crash
- VS present, workload missing → FAIL with correct fix command
- Python 3.11 on PATH → PASS
- Python 3.11 not on PATH → FAIL
- Docker Desktop in Linux mode → WARN
- Docker Desktop not running → WARN

### `submodule_verify.py`
- All submodules initialised → all PASS
- One uninitialised → FAIL with fix command
- `.gitmodules` missing → exit 3

### `setup_orchestrator.py`
- All steps run in order 1→4
- `--toolchain` flag → only step 1 runs
- Not admin → step 1 skipped with WARN, steps 2–4 run, exit 2
- Step 2 fails → step 3 and 4 still run, exit 1
- `--fail-fast` → abort on first failure

### `verify_orchestrator.py`
- All checks pass → exit 0, summary shows all PASS
- One FAIL → exit 1
- Only WARNs → exit 2
- `--json` → output is valid JSON matching schema
- `--quiet` → only WARN/FAIL lines printed

### `claude_context_cmd.py`
- `settings.local.json` exists, no `--force` → skip, exit 0
- `settings.local.json` missing → copy from template, exit 0
- `--force` → overwrite
- Symlink target already correct → skip, exit 0
- Profile path is a plain directory → WARN, do not overwrite, exit 2
- Username computed dynamically from `%USERPROFILE%` (mocked)

## Implementation

### Files introduced

```
Dia/DiaCLI/
└── tests/
    ├── conftest.py
    ├── utils/
    │   ├── test_deps_restore.py
    │   ├── test_toolchain_verify.py
    │   ├── test_submodule_verify.py
    │   └── test_software_installer.py
    └── commands/
        └── env/
            ├── test_deps_restore_cmd.py
            ├── test_setup_orchestrator.py
            ├── test_verify_orchestrator.py
            └── test_claude_context_cmd.py
```

```
Dia/DiaCLI/
└── dia_cli/
    ├── cli/
    │   └── test_cmd.py                    # Click command: dia test cli
    └── commands/
        └── test/
            └── cli_test_cmd.py            # Orchestration: invoke pytest inside container
```

### `cli_test_cmd.py` responsibilities

- Check Docker image exists; exit 3 if not
- Invoke `docker run <image> python -m pytest Dia/DiaCLI/tests/ <flags>` with volume-mounted repo
- Pass through `--filter`, `--parallel`, `--coverage-out` as pytest arguments
- Propagate pytest exit code as `dia test cli` exit code

## Dependencies

| Dependency | Type | Notes |
|------------|------|-------|
| `docker-build-env` feature | Hard | Container must exist to run tests inside it |
| `pytest` (^7.0.0) | Python dev dep | Already in `pyproject.toml` |
| `pytest-xdist` | Python dev dep | Already in `pyproject.toml` |
| `pytest-cov` | Python dev dep | Already in `pyproject.toml` |
| All DiaEnv command modules | Hard | Tests cannot be written until the modules exist |

## Acceptance Criteria

1. `dia test cli` runs the full pytest suite inside the Docker container and exits 0 when all tests pass
2. `dia test cli` exits 1 when any test fails; failing test names are printed
3. `dia test cli --filter deps_restore` runs only tests matching `deps_restore`
4. `dia test cli --parallel` runs pytest with `-n auto`
5. `dia test cli --coverage-out <path>` writes a coverage XML report to the specified path
6. All tests use mocks — no real network calls, no real subprocess calls, no writes outside `tmp`
7. `deps_restore.py` tests cover: sentinel skip, SHA-256 pass/fail, mirror fallback, `file://` dispatch, partial download cleanup
8. `toolchain_verify.py` tests cover: vswhere not found, VS workload missing, Docker Linux mode, Python not on PATH
9. `setup_orchestrator.py` tests cover: step ordering, `--fail-fast`, non-admin elevation skip
10. `dia test cli` exits 3 with a clear message if the Docker image does not exist

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaTest | @docs/specs/systems/dia/diatest.md |

## Binding Decisions Compliance

| ID | Source | Decision | Compliance |
|----|--------|----------|------------|
| PD-001 | Platform | Use StringCRC for all identifiers | Compliant — Python tooling only |
| PD-002 | Platform | ProcessingUnit/Phase/Module architecture | Compliant — test tooling; no runtime components |
| PD-003 | Platform | Component-based entities | Compliant — test tooling |
| PD-004 | Platform | No STL containers in public APIs | Compliant — Python only |
| PD-005 | Platform | x64 Windows only | Compliant — runs inside Windows x64 Docker container |
| PD-006 | Platform | VS project files are source of truth | Compliant — no `.vcxproj` files touched |
| PD-007 | Platform | C++20 required | Compliant — test tooling; no compiler configuration |
| PD-008 | Platform | `Directory.Build.props` owns OutDir/IntDir | Compliant — no build output paths modified |
| SD-CLI-001 | DiaCLI | MDK CLI architecture | Compliant — `test_cmd.py` follows two-file plugin pattern |
| SD-CLI-002 | DiaCLI | Python-based implementation | Compliant |
| SD-CLI-006 | DiaCLI | Click framework | Compliant |
| SD-CLI-008 | DiaCLI | Exit codes follow Unix conventions | Compliant — propagates pytest exit codes |
| SD-TEST-001 | DiaTest | All tests run inside Docker container | Compliant — `cli_test_cmd.py` invokes pytest via `docker run` |
| SD-TEST-004 | DiaTest | Unit tests use mocks; no real network/filesystem | Compliant — all key test cases specified with mocks |
| SD-ENV-010 | DiaEnv | Python 3.11 | Compliant — `pyproject.toml` updated to `^3.11` |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Test ordering | Should tests be written before or after the DiaEnv modules they test? | After — test-after pattern per tech.md. Tests are written alongside or immediately after each DiaEnv feature implementation. |
| 2 | conftest.py | What shared fixtures are needed across all test modules? | `tmp_path` (stdlib pytest), `mock_deps_json` (a valid deps.json fixture), `mock_sentinel_dir` (empty .diaenv/deps/), `mock_repo_root` (a temp directory acting as repo root). Defined in `tests/conftest.py`. |
| 3 | Click testing | How are Click commands tested? | Use Click's `CliRunner` — `from click.testing import CliRunner; result = runner.invoke(cli, ['env', 'deps'])`. Captures stdout/stderr and exit code without spawning a subprocess. |
| 4 | Docker dependency | Tests themselves must run inside Docker — but the test files are written on the host. Is there a chicken-and-egg problem? | No — tests are authored on the host and volume-mounted into the container at test time. The container just needs Python + pytest installed (already in the image via `pyproject.toml` dev deps). |
