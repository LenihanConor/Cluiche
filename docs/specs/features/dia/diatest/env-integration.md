# Feature Spec: env-integration

## Parent System
@docs/specs/systems/dia/diatest.md

## Status
`Done`

## Summary

Implement `dia test env-integration` — an agentic loop that validates the full Cluiche environment inside a fresh Docker container. The loop runs: `dia env docker` (provision) → `dia pipeline` (build) → `dia test cli` (unit tests). If any stage fails due to an environment or infrastructure issue, the AI attempts a fix and re-runs from the failing stage. The loop prompts the operator before each fix attempt beyond the first. It terminates when all stages pass (green) or when the operator declines a fix attempt. The goal is a green build on a clean environment — pre-existing C++ logic bugs are out of scope.

## Problem

Even with all DiaEnv features implemented and unit tested, the only real validation is "does this work on a clean machine?". `dia test env-integration` automates that validation: spin up a fresh container, run the full pipeline, and have the AI fix any environment issues that emerge — issues that only surface in a clean environment (missing PATH entries, wrong dep version, container config gap, etc.).

## Goals

- Single command that runs the full env → pipeline → test sequence inside Docker
- On failure: classify the failure as environment/infrastructure (fixable) or logic bug (not fixable)
- Prompt operator before each fix attempt after the first; show proposed fix before applying
- Loop until green or operator declines
- Report which stage failed, what fix was attempted, and whether it succeeded
- All state (loop iteration, fixes attempted, current stage) visible in the terminal

## Non-Goals

- Fixing pre-existing C++ compiler errors or logic bugs (SD-TEST-002)
- Fixing test failures caused by incorrect test assertions
- Fully autonomous operation without operator oversight (SD-TEST-003)
- Running outside of the Docker container — host machine state must not affect results
- Hosted CI — local-only for now

## What "Environment/Infrastructure" Means

Fixable failures (AI may attempt):
- Missing or misconfigured PATH inside container
- A dep not restored correctly (wrong version, corrupt archive)
- A submodule not initialised inside the container
- A container image missing a required tool
- A `deps.json` entry with wrong URL or SHA-256 hash
- A `winget.json` entry pointing to a wrong version
- A `Directory.Build.targets` misconfiguration

Not fixable (loop terminates, operator informed):
- C++ compilation errors in engine code
- Test assertion failures in `dia test cli`
- Missing `.vcxproj` files or broken project references
- Failures that persist after 3 fix attempts on the same stage

## CLI Interface

```bash
# Run the full env-integration loop
dia test env-integration

# Skip provisioning (assume container already built)
dia test env-integration --skip-env

# Set max fix attempts before prompting (default: 1 auto, then prompt)
dia test env-integration --max-auto-fixes 2

# Non-interactive mode — fail immediately on any error, no fix attempts
dia test env-integration --no-fix
```

## Loop Design

```
┌─────────────────────────────────────────────────────┐
│               dia test env-integration               │
│                                                      │
│  Stage 1: dia env docker (provision container)       │
│  Stage 2: dia pipeline (build Debug + Release)       │
│  Stage 3: dia test cli (pytest suite)                │
│                                                      │
│  On failure:                                         │
│    1. Classify: env/infra or logic bug?              │
│    2. If logic bug → report, exit 1                  │
│    3. If attempt 1 → auto-fix, re-run from stage     │
│    4. If attempt 2+ → show proposed fix, prompt:     │
│         "Apply this fix and retry? [y/N/abort]"      │
│    5. If operator says N or abort → exit 1           │
│    6. If operator says y → apply fix, re-run stage   │
│    7. If same stage fails 3 times → exit 4           │
└─────────────────────────────────────────────────────┘
```

## Fix Classification

The AI classifies failures by inspecting the error output:

| Pattern | Classification |
|---------|---------------|
| `'python' is not recognized` / PATH error | Environment — fix PATH |
| `deps.json: SHA-256 mismatch` | Environment — re-download dep |
| `submodule not initialised` | Environment — run submodule init |
| `error C####` / MSVC compiler error | Logic bug — not fixable |
| `FAILED tests/...` (pytest) | Logic bug — not fixable |
| `docker: image not found` | Environment — rebuild image |
| `msbuild: project not found` | Logic bug — not fixable |

## Progress Output

```
[dia test env-integration]  attempt 1/∞

  Stage 1: env provisioning
    [1/3] image      ... already built, skipping
    [2/3] deps       ... cef not restored ... downloading ... done
    [3/3] paths      ... verified
  Stage 1: PASS

  Stage 2: pipeline
    [1/3] proto-compile  ... done
    [2/3] compile-code   ... FAILED
          error: 'python' is not recognized as an internal command
          Classification: environment — PATH not configured for MSBuild context

  → Auto-fix attempt 1: add Python to MSBuild PATH via Directory.Build.targets
  Applying fix...

  Stage 2: re-running from compile-code...
    [2/3] compile-code   ... done
    [3/3] package        ... done
  Stage 2: PASS

  Stage 3: dia test cli
    pytest ... 47 passed in 3.2s
  Stage 3: PASS

All stages green. Environment validated.
```

## Implementation

### Files introduced

```
Dia/DiaCLI/
└── dia_cli/
    ├── cli/
    │   └── test/
    │       └── env_integration_cmd.py         # Click command: dia test env-integration
    └── commands/
        └── test/
            └── env_integration_runner.py      # Loop orchestration, failure classification, fix application
```

### `env_integration_runner.py` responsibilities

1. Run each stage via subprocess (`dia env docker`, `dia pipeline`, `dia test cli`) inside the container
2. On non-zero exit: capture stdout/stderr, classify failure
3. If environment failure and within auto-fix limit: generate fix, apply, re-run stage
4. If environment failure and beyond auto-fix limit: print proposed fix, prompt operator
5. If logic bug: print classification, exit 1 immediately
6. Track iteration count, stage, and fix history; print running summary
7. On all-green: print summary and exit 0

### Fix generation

Fix generation is a Claude API call (using the model available to DiaCLI at runtime) with:
- The failure output as context
- The list of fixable failure patterns
- The constraint: "fixes must be limited to environment/infrastructure changes"
- The proposed fix is shown to the operator before application

## Dependencies

| Dependency | Type | Notes |
|------------|------|-------|
| `docker-build-env` feature (DiaEnv) | Hard | Container must exist |
| `dia pipeline` system | Hard | Must be implemented before this feature is usable |
| `cli-unit-tests` feature (DiaTest) | Hard | `dia test cli` must work before the loop can validate it |
| Claude API | Runtime | Fix generation requires an API call; must be authenticated |

## Acceptance Criteria

1. `dia test env-integration` runs all three stages in order inside the Docker container
2. On a correctly provisioned environment, all stages pass and the command exits 0
3. On a simulated PATH failure (injected via `--inject-fault path`), the loop detects and fixes it automatically on attempt 1
4. After 1 auto-fix attempt, subsequent fix attempts display the proposed fix and prompt the operator
5. `[y]` applies the fix and re-runs the stage; `[N]` exits 1 with a summary; `[abort]` exits 1 immediately
6. A C++ compiler error (`error C####`) is classified as a logic bug; the loop exits 1 with "not fixable" message and does not attempt a fix
7. The same stage failing 3 times exits 4 with a summary of all attempted fixes
8. `--no-fix` runs the loop without any fix attempts; exits 1 on first failure
9. `--skip-env` skips Stage 1 (assumes container already provisioned)
10. All stage output is visible in the terminal in real time (not buffered)

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
| PD-006 | Platform | VS project files are source of truth | Compliant — AI fixes may not modify `.vcxproj` files |
| PD-007 | Platform | C++20 required | Compliant — no compiler configuration touched by the loop |
| PD-008 | Platform | `Directory.Build.props` owns OutDir/IntDir | Compliant — AI fixes may not override these properties |
| SD-CLI-001 | DiaCLI | MDK CLI architecture | Compliant — two-file plugin pattern |
| SD-CLI-002 | DiaCLI | Python-based implementation | Compliant |
| SD-CLI-006 | DiaCLI | Click framework | Compliant |
| SD-CLI-008 | DiaCLI | Exit codes follow Unix conventions | Compliant — exits 0/1/4 per DiaTest exit code table |
| SD-TEST-001 | DiaTest | All tests run inside Docker | Compliant — all three stages run inside the container |
| SD-TEST-002 | DiaTest | AI fix scope limited to env/infra | Compliant — failure classifier gates all fix attempts; C++ errors are not fixable |
| SD-TEST-003 | DiaTest | Loop prompts operator after first fix | Compliant — auto-fix applies only on attempt 1; all subsequent attempts require operator confirmation |
| SD-ENV-008 | DiaEnv | Docker scope is headless build + test only | Compliant — integration loop is headless; no GPU/GUI |
| SD-ENV-010 | DiaEnv | Python 3.11 | Compliant |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Claude API | How is the Claude API authenticated inside the container? | `ANTHROPIC_API_KEY` environment variable passed to `docker run` via `--env`; never baked into the image. |
| 2 | Fix scope | Can the AI fix modify `Directory.Build.targets` or `deps.json`? | Yes — these are environment/infrastructure files. The AI may propose changes to `Directory.Build.targets`, `deps.json`, `winget.json`, and DiaCLI Python modules. It may not modify `.vcxproj` files (PD-006). |
| 3 | Fault injection | Is `--inject-fault` needed for acceptance testing, or is manual simulation sufficient? | `--inject-fault` is a test-only flag useful for verifying the fix loop works without needing a real broken environment. Include it as a hidden Click option (`hidden=True`). |
| 4 | Loop state | If the operator closes the terminal mid-loop, is state lost? | Yes — no persistence between runs. The loop is stateless; a fresh `dia test env-integration` starts from Stage 1. This is acceptable for the current scope. |
| 5 | `dia pipeline` dependency | `dia pipeline` does not exist yet — does this spec need to wait? | The spec is Approved now; implementation must wait until `dia pipeline` is specced and implemented. The dependency is noted in the Dependencies table. |
