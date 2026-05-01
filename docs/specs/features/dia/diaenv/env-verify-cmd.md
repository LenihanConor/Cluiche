# Feature Spec: env-verify-cmd

## Parent System
@docs/specs/systems/dia/diaenv.md

## Status
`Done`

## Summary

Implement `dia env verify` — a read-only, CI-safe health checker that inspects the current machine against all DiaEnv manifests and reports PASS / WARN / FAIL per check with actionable fix commands. Covers: toolchain tools (VS 2022 + workload, Python 3.11, Git, Node.js, Poetry, Docker Desktop), binary SDK deps (via `deps.json` sentinels), git submodule state, and Docker Desktop Windows Containers mode. Exits 0 on all-pass, 1 on any FAIL, 2 on warnings only.

## Problem

After `dia env setup` runs, there is no way to confirm the environment is correctly configured without manually checking each tool. On a machine that was provisioned manually (not via `dia env setup`), there is no health check at all. `dia env verify` provides a single command that answers "is this machine ready to build Cluiche?" without making any changes.

## Goals

- Single read-only command that checks all environment components
- PASS / WARN / FAIL per check with a coloured, human-readable checklist
- `--json` output mode for scripted/CI use
- Actionable fix command printed for every WARN and FAIL
- Per-category flags: `--toolchain`, `--deps`, `--submodules`, `--docker`
- Exit codes: 0 = all pass, 1 = any FAIL, 2 = warnings only, 3 = manifest not found
- `vswhere.exe` integration for VS 2022 workload detection

## Non-Goals

- Making any changes to the environment — strictly read-only (SD-ENV-003)
- Auto-fixing failures — verify reports, setup fixes
- Continuous monitoring (`dia env doctor`) — deferred per system spec
- Checking build output or runtime correctness — environment only

## CLI Interface

```bash
# Full environment health check
dia env verify

# Category-specific checks
dia env verify --toolchain     # VS 2022, Python, Git, Node.js, Poetry, Docker Desktop
dia env verify --deps          # Binary SDK sentinels vs deps.json
dia env verify --submodules    # git submodule status
dia env verify --docker        # Docker Desktop presence + Windows Containers mode

# Machine-readable output
dia env verify --json

# Quiet — print only FAILs and WARNs
dia env verify --quiet
```

## Check Catalogue

### Toolchain checks (`--toolchain`)

| Check | PASS | WARN | FAIL | Fix command |
|-------|------|------|------|-------------|
| VS 2022 installed | `vswhere.exe` finds VS 17.x | VS installed but old version | Not installed | `dia env setup --toolchain` |
| C++ Desktop workload | `vswhere.exe` reports workload present | — | Workload missing | `dia env setup --toolchain` |
| Python 3.11 | `python --version` == 3.11.x | 3.11.x installed but not on PATH | Not installed | `dia env setup --toolchain` |
| Git | `git --version` succeeds | — | Not installed | `dia env setup --toolchain` |
| Node.js >= 18 | `node --version` >= 18.x | 10 <= version < 18 | Not installed or < 10 | `dia env setup --toolchain` |
| Poetry | `poetry --version` succeeds | — | Not installed | `pip install poetry` |
| Docker Desktop | Process/service present | Installed, not running | Not installed | `dia env setup --toolchain` |
| Docker Windows Containers | `docker info` shows `OSType: windows` | Docker running in Linux mode | Docker not running | Right-click tray → Switch to Windows containers |

### Dep checks (`--deps`)

For each entry in `deps.json`:

| State | Report | Fix command |
|-------|--------|-------------|
| Sentinel present, version matches | PASS | — |
| Directory exists, no sentinel | WARN — manually placed, unverified | `dia env deps --dep <id> --force` |
| Sentinel absent, directory missing | FAIL | `dia env deps --dep <id>` |
| Sentinel present, version mismatch | WARN — stale sentinel | `dia env deps --dep <id> --force` |

### Submodule checks (`--submodules`)

For each submodule in `.gitmodules`:

| State | Report | Fix command |
|-------|--------|-------------|
| Initialised, correct commit | PASS | — |
| Initialised, wrong commit | WARN | `git submodule update --recursive` |
| Not initialised | FAIL | `git submodule update --init --recursive` |
| Missing from `.gitmodules` | FAIL | See `submodule-migration` feature |

### Docker checks (`--docker`)

Subset of toolchain checks focused on Docker Desktop and Windows Containers mode. Reported separately because `docker-build-env` depends on it.

## Output Format

### Human-readable (default)

```
[dia env verify]

Toolchain
  VS 2022 (17.x)           PASS
  C++ Desktop workload     PASS
  Python 3.11              PASS
  Git                      PASS
  Node.js (20.x)           PASS
  Poetry                   PASS
  Docker Desktop           WARN  not running — start Docker Desktop
  Windows Containers mode  FAIL  Docker in Linux mode
                                 Fix: right-click tray → Switch to Windows containers

SDK Deps
  sfml                     PASS
  ultralight               PASS
  cef                      FAIL  not restored
                                 Fix: dia env deps --dep cef
  python311                PASS
  visjs                    PASS
  webix-5.2.1              PASS
  webix-2.4.7              PASS
  protobuf                 PASS

Submodules
  pybind11                 PASS
  asio                     PASS
  websocketpp              PASS
  googletest               PASS
  jsoncpp-master           WARN  not initialised
                                 Fix: git submodule update --init --recursive

Result: 1 FAIL, 2 WARN, 10 PASS  (exit 1)
```

### JSON output (`--json`)

```json
{
  "result": "fail",
  "pass": 10,
  "warn": 2,
  "fail": 1,
  "checks": [
    { "category": "toolchain", "name": "VS 2022", "status": "pass", "detail": "17.9.1" },
    { "category": "deps", "name": "cef", "status": "fail", "detail": "not restored", "fix": "dia env deps --dep cef" }
  ]
}
```

## Implementation

### Files introduced / modified

```
Dia/DiaCLI/
└── dia_cli/
    ├── cli/
    │   └── env/
    │       └── verify_cmd.py              # New — Click command: dia env verify
    ├── commands/
    │   └── env/
    │       └── verify_orchestrator.py     # New — runs all checkers, collects results
    └── utils/
        ├── toolchain_verify.py            # New — vswhere.exe + tool version checks
        ├── deps_verify.py                 # New — sentinel + deps.json cross-check
        └── submodule_verify.py            # New — git submodule status checks
```

### `verify_orchestrator.py` responsibilities

- Accept category filter (all / toolchain / deps / submodules / docker)
- Call each checker; collect `CheckResult(name, category, status, detail, fix)` objects
- Count PASS / WARN / FAIL; determine exit code
- Format output (human or JSON) and print
- Emit `OnEnvVerifyCompleted(passCount, warnCount, failCount)` event

## Dependencies

| Dependency | Type | Notes |
|------------|------|-------|
| `deps-manifest` feature | Hard | `deps.json` must exist for dep checks |
| `submodule-migration` feature | Hard | `.gitmodules` must be correct for submodule checks |
| `winget-manifest` feature | Hard | Toolchain check list derived from `winget.json` tool set |
| `vswhere.exe` | Runtime | Ships with VS 2022 installer; verify check degrades gracefully if absent |

## Acceptance Criteria

1. `dia env verify` runs all checks and prints a coloured human-readable checklist
2. Exit code is 0 when all checks PASS, 1 when any check FAILs, 2 when only WARNs present, 3 when `deps.json` or `.gitmodules` not found
3. Every WARN and FAIL line includes an actionable fix command
4. `dia env verify --json` outputs valid JSON matching the schema above
5. `dia env verify --toolchain` runs only toolchain checks; other categories skipped
6. `dia env verify` detects VS 2022 installed but C++ Desktop workload missing and reports FAIL
7. `dia env verify` detects Docker Desktop in Linux Containers mode and reports WARN
8. `dia env verify` detects an uninitialised submodule and reports FAIL with fix command
9. `dia env verify` is read-only — no files written, no downloads triggered, no installs run
10. `dia env verify --quiet` prints only WARN and FAIL lines plus the result summary

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaEnv | @docs/specs/systems/dia/diaenv.md |

## Binding Decisions Compliance

| ID | Source | Decision | Compliance |
|----|--------|----------|------------|
| PD-001 | Platform | Use StringCRC for all identifiers | Compliant — Python tooling only; no C++ identifiers introduced |
| PD-002 | Platform | ProcessingUnit/Phase/Module architecture | Compliant — development tooling; no runtime components |
| PD-003 | Platform | Component-based entities | Compliant — development tooling; no entity system used |
| PD-004 | Platform | No STL containers in public APIs | Compliant — Python only; no C++ public API surface |
| PD-005 | Platform | x64 Windows only | Compliant — `vswhere.exe` and `ctypes.windll` are Windows-only; consistent with PD-005 |
| PD-006 | Platform | VS project files are source of truth | Compliant — verify does not touch any `.vcxproj` files |
| PD-007 | Platform | C++20 required | Compliant — tooling feature; no compiler configuration touched |
| PD-008 | Platform | `Directory.Build.props` owns OutDir/IntDir | Compliant — no build output paths modified |
| AD-001 | Dia App | Module system with YAML frontmatter | Compliant — Python tooling; no new C++ module introduced |
| SD-CLI-001 | DiaCLI | MDK CLI architecture | Compliant — `verify_cmd.py` follows the two-file plugin pattern |
| SD-CLI-002 | DiaCLI | Python-based implementation | Compliant — all implementation is Python |
| SD-CLI-003 | DiaCLI | Separate from C++ DiaAPI | Compliant — no C++ DiaAPI commands introduced |
| SD-CLI-006 | DiaCLI | Click framework for argument parsing | Compliant — `verify_cmd.py` uses Click decorators |
| SD-CLI-008 | DiaCLI | Exit codes follow Unix conventions | Compliant — exits 0/1/2/3 per DiaEnv exit code table |
| SD-ENV-001 | DiaEnv | `deps.json` is single source of truth | Compliant — dep checks read `deps.json`; no parallel source of truth |
| SD-ENV-002 | DiaEnv | SHA-256 verification required | Compliant — verify reads sentinels (which record SHA-256); does not re-verify hashes on every run (read-only) |
| SD-ENV-003 | DiaEnv | `dia env verify` is read-only and CI-safe | Compliant — this is the implementing feature; strictly no writes, no downloads, no installs |
| SD-ENV-007 | DiaEnv | Source-only deps use git submodules | Compliant — submodule checks use `git submodule status`; no file writes |
| SD-ENV-010 | DiaEnv | Python 3.11 is the single Python version | Compliant — toolchain check validates Python 3.11 specifically |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | vswhere.exe | What happens if `vswhere.exe` is not found (e.g. VS not yet installed)? | Report FAIL for both VS 2022 and C++ workload checks with the message "vswhere.exe not found — VS 2022 not installed". Do not crash. |
| 2 | Docker check | `docker info` requires Docker Desktop to be running — what if Docker is installed but not started? | Report WARN "Docker Desktop installed but not running" with fix "Start Docker Desktop". Do not attempt to start it. |
| 3 | Colour output | Should colour output be disabled automatically when stdout is not a TTY (e.g. piped to a file)? | Yes — detect TTY via `sys.stdout.isatty()`; disable colour codes when not a TTY. `--json` always disables colour. |
| 4 | deps.json absent | Should `dia env verify` fail hard (exit 3) or soft (FAIL check) if `deps.json` is missing? | Exit 3 with a clear error: "deps.json not found — run `dia env setup --deps` to initialise". Consistent with DiaEnv exit code table. |
| 5 | Partial flags | Can `--toolchain` and `--deps` be combined? | Yes — additive; runs only the named categories. `dia env verify --toolchain --deps` skips submodule and docker checks. |
