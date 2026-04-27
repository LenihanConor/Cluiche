# Feature Spec: ui

## Parent System
@docs/specs/systems/dia/diatest.md

## Status
`Done`

## Summary

Implement `dia test ui` — runs the Vitest suite for `Dia/DiaApplicationEditor/UI/` using the Node.js available in the current execution context (host or Docker container). Vitest and all testing dependencies are already configured in `package.json`. Supports `--filter` (maps to Vitest's `-t` flag), `--watch` mode, and `--docker` (re-invokes inside the container). Runs wherever the command is invoked from — no environment assumption baked in.

## Problem

Vitest is already set up and 10 test files exist in `Dia/DiaApplicationEditor/UI/src/`, but there is no `dia test` entry point that runs them consistently. Running them requires knowing the project directory, having Node.js/npm available, and knowing the `vitest run` invocation. `dia test ui` provides the single consistent entry point that fits into the `dia test` command surface.

## Goals

- Run `npm run test` (which calls `vitest run`) in `Dia/DiaApplicationEditor/UI/`
- Stream Vitest stdout/stderr to the terminal in real time
- Exit with Vitest's exit code (0 all-pass, 1 any-fail)
- Exit 2 with a clear message if `node_modules/` is not found (i.e. `npm install` hasn't been run)
- `--filter <pattern>` maps to Vitest's `-t <pattern>` flag for test name matching
- `--watch` runs `vitest` in watch mode instead of `vitest run`
- `--docker` re-invokes inside the Docker container via `docker run` (same pattern as `dia pipeline --docker`)

## Non-Goals

- Installing Node.js or npm — owned by DiaEnv (`winget-manifest`)
- Running `npm install` — the developer is expected to have done this; exit 2 prompts if missing
- Coverage reporting — Vitest's built-in coverage can be enabled separately; out of scope for this feature
- Running tests for other UI projects (if any are added in the future)

## CLI Interface

```bash
# Run all UI tests (host Node.js)
dia test ui

# Filter to tests matching a pattern
dia test ui --filter "ManifestStore"
dia test ui --filter "renders the flow view"

# Watch mode (re-runs on file change)
dia test ui --watch

# Run inside Docker container
dia test ui --docker

# Combined
dia test ui --docker --filter "FlowView"
```

## Vitest Invocation

```python
ui_dir = repo_root / "Dia/DiaApplicationEditor/UI"

# Check node_modules exists
if not (ui_dir / "node_modules").exists():
    logger.error(
        "node_modules not found at Dia/DiaApplicationEditor/UI/node_modules\n"
        "Run: cd Dia/DiaApplicationEditor/UI && npm install"
    )
    return 2

cmd = ["npm", "run", "test" if not watch else "test:watch"]
if filter_pattern:
    # npm run test -- -t <pattern>  (-- passes args through to vitest)
    cmd += ["--", "-t", filter_pattern]

result = subprocess.run(cmd, cwd=str(ui_dir))
return result.returncode
```

`npm run test` calls `vitest run` per the existing `package.json` scripts.
`npm run test:watch` calls `vitest` (watch mode).

## `--docker` Behaviour

Follows the same pattern as `dia pipeline --docker` and `dia test googletest --docker`:

1. Check Docker image exists; exit 3 if not
2. Re-invoke `dia test ui` inside the container with all flags forwarded (minus `--docker`)
3. Volume-mount the repo root at `C:/repo`
4. Stream container stdout/stderr to the host terminal in real time

```python
docker_cmd = [
    "docker", "run", "--rm",
    "--volume", f"{repo_root}:C:/repo",
    "--workdir", "C:/repo",
    image_name,
    "python", "-m", "dia_cli",
    "test", "ui",
] + forwarded_args
```

Node.js and npm must be available inside the Docker image (dependency on `docker-build-env`). When running inside the container, `npm install --prefer-offline` is always run before Vitest to ensure container-native binaries are used rather than host-compiled ones. `--prefer-offline` is fast when packages are cached in the npm cache layer baked into the Docker image.

## Implementation

### Files introduced

```
Dia/DiaCLI/
└── dia_cli/
    ├── cli/
    │   └── test/
    │       └── ui_cmd.py              # Click command: dia test ui
    └── commands/
        └── test/
            └── ui_runner.py          # node_modules check, vitest invocation, docker re-invoke
```

### `ui_runner.py` responsibilities

- `check_node_modules(ui_dir)` — return `True` if `node_modules/` exists
- `run(repo_root, filter_pattern, watch, docker, image_name)` — orchestrate check + invocation or docker re-invoke

## Dependencies

| Dependency | Type | Notes |
|------------|------|-------|
| `docker-build-env` feature (DiaEnv) | Hard (`--docker` only) | Container must have Node.js + npm installed |
| `winget-manifest` feature (DiaEnv) | Soft (host) | Node.js LTS installed on host via winget |
| `npm` / `node` | System tool | Must be on PATH in the execution context |
| `subprocess` (stdlib) | Python | |

## Acceptance Criteria

1. `dia test ui` runs `vitest run` in `Dia/DiaApplicationEditor/UI/` and exits with Vitest's exit code
2. All Vitest output streams to the terminal in real time
3. `--filter "ManifestStore"` passes `-t ManifestStore` to Vitest; only matching tests run
4. `--watch` runs Vitest in watch mode (does not exit on completion)
5. If `node_modules/` is not found, exits 2 with the path and an `npm install` fix command
6. `--docker` re-invokes inside the container; all flags forwarded; container output streams to host
7. If Docker image is not found when using `--docker`, exits 3 with a clear message
8. The existing 10 test files in `Dia/DiaApplicationEditor/UI/src/` all run without error

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
| PD-005 | Platform | x64 Windows only | Compliant — no platform-specific path logic; npm/node are cross-platform but Cluiche is Windows-only |
| PD-006 | Platform | VS project files are source of truth | Compliant — no `.vcxproj` files touched |
| PD-007 | Platform | C++20 required | Compliant — test tooling; does not affect compiler configuration |
| PD-008 | Platform | `Directory.Build.props` owns OutDir/IntDir | Compliant — UI tests produce no C++ build output |
| AD-001 | Dia App | Module system with YAML frontmatter | Compliant — Python tooling only |
| SD-CLI-001 | DiaCLI | MDK CLI architecture | Compliant — two-file plugin pattern |
| SD-CLI-002 | DiaCLI | Python-based implementation | Compliant |
| SD-CLI-006 | DiaCLI | Click framework | Compliant |
| SD-CLI-008 | DiaCLI | Exit codes follow Unix conventions | Compliant — 0 all-pass, 1 any-fail (propagated from Vitest), 2 node_modules missing, 3 Docker not found |
| SD-TEST-001 | DiaTest | All tests run inside Docker container | Compliant — `--docker` flag enables container execution; host execution is also supported for local dev |
| SD-TEST-004 | DiaTest | Unit tests use mocks; no real network/filesystem | Compliant — `ui_runner.py` unit tests mock subprocess and filesystem; the Vitest suite itself uses jsdom and @testing-library/react |
| SD-ENV-008 | DiaEnv | Docker scope is headless build + test only | Compliant — Vitest runs headless via jsdom; no browser required |
| SD-ENV-010 | DiaEnv | Python 3.11 | Compliant |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Node.js in Docker | Does the `docker-build-env` Dockerfile include Node.js? | Node.js LTS is listed in `winget-manifest` as a required tool. The `docker-build-env` spec must install it in the container image. This is a dependency: `dia test ui --docker` will fail if the container image was built without Node.js. The `docker-build-env` spec should be updated to ensure Node.js is included. |
| 2 | `npm run test` vs `npx vitest` | Should the invocation use `npm run test` or call `npx vitest run` directly? | `npm run test` — it uses the script already defined in `package.json` and is resilient to future script changes (e.g. if the team adds `--reporter=verbose` to the npm script). Calling `npx vitest` directly would bypass the script. |
| 3 | `--filter` passthrough | Does `npm run test -- -t <pattern>` correctly pass `-t` through to Vitest? | Yes — `npm run <script> -- <args>` passes `<args>` to the underlying command. `vitest run -t <pattern>` filters by test name. |
| 4 | `node_modules` in Docker | When volume-mounting the repo, `node_modules/` from the host is included. Does this cause issues inside the container? | Always run `npm install --prefer-offline` inside the container before Vitest when `--docker` is active. Host `node_modules` may contain Windows-native binaries (e.g. esbuild) that won't run in the container. `--prefer-offline` uses the npm cache so it is fast when packages are already present. The `ui_runner.py` docker path runs `npm install --prefer-offline` as a pre-step before invoking `vitest run`. |
