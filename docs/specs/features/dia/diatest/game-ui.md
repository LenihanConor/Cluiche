# Feature Spec: game-ui

## Parent System
@docs/specs/systems/dia/diatest.md

## Status
`Done`

## Summary

Implement `dia test game-ui` — runs the Vitest suite for `Cluiche/CluicheTest/UI/` (the React game UI project targeting DiaUIUltralight) using the Node.js available in the current execution context (host or Docker container). Supports `--filter`, `--watch`, and `--docker` flags, identical to `dia test editor-ui`. The underlying runner (`ui_runner.py`) is shared; the only difference is the `ui_subpath` (`Cluiche/CluicheTest/UI`) and `docker_subcmd` (`game-ui`).

## Problem

`Cluiche/CluicheTest/UI/` has a Vitest suite covering the GameBridge contract and React component behaviour for the game-facing UI. Without a `dia test game-ui` entry point, the tests can only be run by manually `cd`-ing into the directory and invoking npm. This is inconsistent with how editor UI tests are run (`dia test editor-ui`) and means the game UI tests are easy to skip.

The game UI uses DiaUIUltralight (not CEF), so it is a distinct test target from the editor UI.

## Goals

- Run `npm run test` (which calls `vitest run`) in `Cluiche/CluicheTest/UI/`
- Stream Vitest stdout/stderr to the terminal in real time
- Exit with Vitest's exit code (0 all-pass, 1 any-fail)
- Exit 2 with a clear message if `node_modules/` is not found
- `--filter <pattern>` maps to Vitest's `-t <pattern>` flag
- `--watch` runs `vitest` in watch mode
- `--docker` re-invokes inside the Docker container

## Non-Goals

- C++↔Ultralight integration tests — those require a running Ultralight instance embedded in a C++ binary; that is a separate future feature
- Installing Node.js — owned by DiaEnv
- Running `npm install` — developer responsibility; exit 2 prompts if missing

## CLI Interface

```bash
# Run all game UI tests (host Node.js)
dia test game-ui

# Filter to tests matching a pattern
dia test game-ui --filter "GameBridge"
dia test game-ui --filter "ExamplePanel"

# Watch mode
dia test game-ui --watch

# Run inside Docker container
dia test game-ui --docker

# Combined
dia test game-ui --docker --filter "GameBridge"
```

## Vitest Invocation

Delegates entirely to `ui_runner.run()` with:
- `ui_subpath = "Cluiche/CluicheTest/UI"`
- `docker_subcmd = "game-ui"`

See `editor-ui.md` for the shared runner mechanics.

## Implementation

### Files changed

```
Dia/DiaCLI/
├── dia_cli/
│   ├── cli/
│   │   └── test.py                    # Added game-ui Click command
│   └── commands/
│       └── test/
│           └── ui_runner.py          # Extended run() with ui_subpath + docker_subcmd params
└── tests/
    └── test_dia_test_game_ui.py       # 13 tests: discovery, AC1–AC7, cwd, docker subcmd
```

## Dependencies

| Dependency | Type | Notes |
|------------|------|-------|
| `ui_runner.py` | Internal | Shared with `editor-ui`; accepts `ui_subpath` + `docker_subcmd` |
| `docker-build-env` feature (DiaEnv) | Hard (`--docker` only) | Container must have Node.js + npm installed |
| `winget-manifest` feature (DiaEnv) | Soft (host) | Node.js LTS on host |
| `npm` / `node` | System tool | VS-bundled Node.js used on Windows |
| `game-ui-framework-convention` (DiaUIUltralight) | Soft | This command runs the tests introduced by that feature |

## Acceptance Criteria

1. `dia test game-ui` runs `vitest run` in `Cluiche/CluicheTest/UI/` and exits with Vitest's exit code
2. All Vitest output streams to the terminal in real time
3. `--filter "GameBridge"` passes `-t GameBridge` to Vitest; only matching tests run
4. `--watch` runs Vitest in watch mode (does not exit on completion)
5. If `node_modules/` is not found, exits 2 with the path and an `npm install` fix command
6. `--docker` re-invokes inside the container with `test game-ui`; all flags forwarded
7. If Docker image is not found when using `--docker`, exits 3 with a clear message
8. The existing test files in `Cluiche/CluicheTest/UI/src/` all run without error

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
| PD-005 | Platform | x64 Windows only | Compliant — Windows-only paths; npm/node are cross-platform but Cluiche is Windows-only |
| PD-006 | Platform | VS project files are source of truth | Compliant — no `.vcxproj` files touched |
| PD-007 | Platform | C++20 required | Compliant — test tooling; does not affect compiler configuration |
| PD-008 | Platform | `Directory.Build.props` owns OutDir/IntDir | Compliant — UI tests produce no C++ build output |
| AD-001 | Dia App | Module system with YAML frontmatter | Compliant — Python tooling only |
| SD-CLI-001 | DiaCLI | MDK CLI architecture | Compliant — two-file plugin pattern |
| SD-CLI-002 | DiaCLI | Python-based implementation | Compliant |
| SD-CLI-006 | DiaCLI | Click framework | Compliant |
| SD-CLI-008 | DiaCLI | Exit codes follow Unix conventions | Compliant — 0 all-pass, 1 any-fail, 2 node_modules missing, 3 Docker not found |
| SD-TEST-001 | DiaTest | All tests run inside Docker container | Compliant — `--docker` flag enables container execution; host execution is also supported for local dev |
| SD-TEST-004 | DiaTest | Unit tests use mocks; no real network/filesystem | Compliant — `ui_runner.py` unit tests mock subprocess and filesystem; Vitest suite uses jsdom and @testing-library/react |
| SD-ENV-008 | DiaEnv | Docker scope is headless build + test only | Compliant — Vitest runs headless via jsdom; no Ultralight renderer required |
| SD-ENV-010 | DiaEnv | Python 3.11 | Compliant |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Scope | Why are game UI tests (jsdom/Vitest) separate from C++↔Ultralight integration tests? | The Vitest suite tests JS component behaviour and bridge contract in isolation (GameBridge mocked). Real C++↔Ultralight integration requires a running Ultralight instance embedded in a C++ executable — that is a separate, future feature. The Vitest tests are valuable now and do not require a C++ build. |
| 2 | Shared runner | Does sharing `ui_runner.py` between editor-ui and game-ui create coupling? | No — the runner is parameterised by `ui_subpath` and `docker_subcmd`. Each command passes its own values. No logic branches on which project is being tested. |
| 3 | `test:watch` script | Does `Cluiche/CluicheTest/UI/package.json` define `test:watch`? | Yes — `"test:watch": "vitest"` is present, consistent with the editor UI `package.json`. |
