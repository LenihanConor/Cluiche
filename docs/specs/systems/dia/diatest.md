# System Spec: DiaTest

## Parent Application
@docs/specs/applications/dia.md

## Purpose

DiaTest is the test execution system for the Cluiche platform. It owns the `dia test` command surface — providing a consistent interface to run different categories of tests: DiaCLI Python unit tests, C++ GoogleTests, React/Vitest UI tests, and the agentic environment integration loop. DiaTest does not own test authoring conventions (those belong to each feature spec) but it does own the runner infrastructure, reporting, and the AI-assisted environment integration loop.

The core problem DiaTest solves: tests for the Cluiche platform are spread across multiple frameworks (pytest, GoogleTest, Vitest) with no unified entry point. `dia test` provides a single command that knows how to invoke each framework correctly and report results consistently.

## Responsibilities

- **CLI Unit Tests** — `dia test cli`: run pytest for all DiaCLI modules inside the Docker container
- **Environment Integration Loop** — `dia test env-integration`: orchestrate `dia env → dia pipeline → dia test` inside Docker; use AI to fix environment/infrastructure failures; loop until green or operator prompt
- **Unified Exit Codes** — propagate framework-specific exit codes through a consistent DiaTest exit code table
- **Result Reporting** — human-readable summary per category; JSON output mode for scripted use

## Non-Responsibilities

- **Test authoring** — each feature spec defines its own tests; DiaTest only runs them
- **C++ GoogleTest execution** — owned by `dia test googletest`; DiaTest defines the interface
- **React/Vitest execution** — owned by `dia test editor-ui` and `dia test game-ui`
- **Fixing pre-existing compiler bugs** — the integration loop fixes environment/infrastructure issues only; C++ logic bugs are out of scope
- **Hosted CI configuration** — DiaTest is local-only; CI pipeline is a future concern

## Public Interfaces

### CLI Commands

```bash
# Run DiaCLI Python unit tests (inside Docker)
dia test cli

# Run the agentic environment integration loop (inside Docker)
dia test env-integration

# Future
dia test googletest         # Run C++ GoogleTests
dia test editor-ui          # Run DiaApplicationEditor (CEF) React/Vitest tests
dia test game-ui            # Run CluicheTest (Ultralight) React/Vitest tests
dia test all                # Run all registered test categories
```

### Exit Codes

| Code | Meaning |
|------|---------|
| `0` | All tests passed |
| `1` | One or more tests failed |
| `2` | Test runner not found or misconfigured |
| `3` | Docker container not available |
| `4` | Integration loop exhausted max attempts without reaching green |

## Features

| Feature | Description | Key Capabilities | Spec | Effort | Status |
|---------|-------------|------------------|------|--------|--------|
| cli-unit-tests | `dia test cli` — run pytest for all DiaCLI modules inside Docker | pytest, mocking, coverage report, runs in container | [cli-unit-tests.md](../../features/dia/diatest/cli-unit-tests.md) | 3 days | Done |
| env-integration | `dia test env-integration` — agentic loop: env→pipeline→test, AI fixes env failures | Docker, loop with prompt, AI fix scope limited to env issues | [env-integration.md](../../features/dia/diatest/env-integration.md) | 5 days | Done |
| googletest | `dia test googletest` — run built GoogleTests.exe binary; --filter, --config, --docker | Binary location, gtest_filter passthrough, pre-run dep check, docker re-invoke | [googletest.md](../../features/dia/diatest/googletest.md) | 2 days | Done |
| editor-ui | `dia test editor-ui` — run Vitest suite for DiaApplicationEditor (CEF) UI; --filter, --watch, --docker | npm/vitest invocation, node_modules check, docker re-invoke | [ui.md](../../features/dia/diatest/ui.md) | 2 days | Done |
| game-ui | `dia test game-ui` — run Vitest suite for CluicheTest (Ultralight) UI; --filter, --watch, --docker | shared ui_runner.py, game-specific subpath + docker subcmd | [game-ui.md](../../features/dia/diatest/game-ui.md) | 0.5 days | Done |

**Total Effort Estimate:** ~12 days

**Recommended Implementation Order:**
1. `cli-unit-tests` (3d) — foundational; validates DiaEnv Python logic before running the integration loop
2. `env-integration` (5d) — depends on `docker-build-env`, `dia pipeline`, and `cli-unit-tests` being stable

## Platform Primitives Used

**Python Packages:**
- **pytest** (^7.0.0) — already in DiaCLI dev dependencies
- **pytest-xdist** — parallel test execution
- **pytest-cov** — coverage reporting
- **unittest.mock** (stdlib) — mocking filesystem, network, subprocess calls
- **click** — CLI surface (existing DiaCLI dependency)
- **OutputContext** (`dia_cli/utils/dia_output.py`) — structured terminal output and NDJSON event log; all test run events emitted via this layer (see `cli-output` feature)

**System Tools:**
- **Docker Desktop** (Windows Containers mode) — all tests run inside the container

## Dependencies on Other Systems

| System | Type | Notes |
|--------|------|-------|
| DiaEnv | Hard | `docker-build-env` feature provides the container; `deps-manifest` logic is what `cli-unit-tests` tests |
| `dia pipeline` | Hard (env-integration only) | Integration loop invokes the build pipeline inside the container |
| DiaCLI | Hard | All commands are DiaCLI plugins |
| DiaCLI `cli-output` | Hard | `OutputContext` is the sole terminal + NDJSON output layer for all test run events |

## Decisions

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| SD-TEST-001 | All tests run inside the Docker container | Guarantees clean, reproducible environment; host machine state cannot affect results | All features | Accepted | Yes |
| SD-TEST-002 | AI fix scope in integration loop is limited to environment/infrastructure issues | Pre-existing C++ bugs are out of scope; goal is a green build on a clean environment, not fixing logic bugs | env-integration | Accepted | Yes |
| SD-TEST-003 | Integration loop prompts operator before each fix attempt beyond the first | Prevents runaway AI changes; operator remains in control | env-integration | Accepted | Yes |
| SD-TEST-004 | `dia test cli` uses pytest with mocks; no real network or filesystem calls in unit tests | Tests must be fast and runnable without network access | cli-unit-tests | Accepted | Yes |

## Inherited Binding Decisions

| ID | Source | Decision | Implication for this system |
|----|--------|----------|----------------------------|
| PD-005 | Platform | x64 Windows only | Docker container is Windows x64; no Linux container support |
| PD-006 | Platform | VS project files are source of truth | DiaTest does not modify `.vcxproj` files; build is owned by `dia pipeline` |
| PD-009 | Platform | All generated non-binary output under `Cluiche/out/<AppName>/` | Test NDJSON event log written to `Cluiche/out/DiaCLI/logs/test/last-run.ndjson`; coverage reports to `Cluiche/out/DiaCLI/logs/test/coverage.xml`. Compliant. |
| SD-CLI-001 | DiaCLI | MDK CLI architecture | All `dia test` commands follow the two-file plugin pattern |
| SD-CLI-002 | DiaCLI | Python-based implementation | All DiaTest commands are Python |
| SD-CLI-006 | DiaCLI | Click framework | All commands use Click |
| SD-CLI-008 | DiaCLI | Exit codes follow Unix conventions | DiaTest exit codes follow the table above |
| SD-ENV-008 | DiaEnv | Docker scope is headless build + test only | DiaTest runs headless inside the container; no GPU/GUI |
| SD-ENV-010 | DiaEnv | Python 3.11 | pytest and all DiaCLI code runs on Python 3.11 |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Container | Should `dia test cli` build a fresh Docker image before running, or assume the image already exists? | Assume image exists (built by `dia env docker image`); exit 3 with clear message if image not found. Running the test suite should not trigger a slow image build. |
| 2 | Coverage | Should pytest coverage reports be written to a file (e.g. `coverage.xml`) or printed only? | Printed to stdout by default; optionally written to `Cluiche/out/DiaCLI/logs/test/coverage.xml` with `--coverage-out` flag (per PD-009). |
| 3 | Future features | Should `dia test googletest` and `dia test ui` be specced now or deferred? | Specced — both features are now in the features table. `googletest` depends on `dia pipeline compile-code` building the binary first. `ui` depends on Node.js being available in the execution context. |

## Status

`Done` - All 4 features implemented and proven: 235 tests pass, pipeline runs green end-to-end.
