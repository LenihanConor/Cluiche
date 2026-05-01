# Feature Spec: env-setup-cmd

## Parent System
@docs/specs/systems/dia/diaenv.md

## Status
`Done`

## Summary

Implement `dia env setup` — the top-level orchestration command that provisions a fresh developer machine end-to-end by running all DiaEnv sub-steps in the correct order: toolchain install, SDK dep restore, submodule init, and AI context wiring. Each sub-step can also be invoked individually via flags. This is the "single command after git clone" payoff that DiaEnv exists to deliver.

## Problem

Even after `deps-manifest`, `winget-manifest`, `submodule-migration`, and `claude-context` are implemented as individual commands, a new developer must know the correct order to run them. `dia env setup` collapses the entire provisioning ritual into one command.

## Goals

- `dia env setup` runs all sub-steps in the correct dependency order
- Each sub-step is individually selectable via flags (`--toolchain`, `--deps`, `--submodules`, `--claude`)
- Progress output: each step reports start, completion, and any warnings
- Idempotent: re-running on an already-provisioned machine skips completed steps (sentinel-based)
- `--force` re-runs all steps regardless of sentinel state
- Exits 0 only if all steps complete without hard failure
- Elevation check before toolchain step; clear error if not elevated

## Non-Goals

- `dia env docker` — container infrastructure is a separate command (docker-build-env feature)
- `dia env verify` — health checking is a separate command (env-verify-cmd feature)
- Unattended fully silent install — Docker Desktop requires manual mode switch; VS may require user interaction; progress is always printed

## CLI Interface

```bash
# Full bootstrap — run all sub-steps in order
dia env setup

# Individual sub-steps
dia env setup --toolchain      # winget import + VS workload + Poetry install
dia env setup --deps           # deps.json restore (all binary SDKs)
dia env setup --dep <name>     # restore a single named dep
dia env setup --submodules     # git submodule init + update --recursive
dia env setup --claude         # generate settings.local.json + symlink memory dir

# Force re-run even if sentinels present
dia env setup --force
dia env setup --deps --force
```

## Orchestration Order

When run without flags, `dia env setup` executes sub-steps in this order:

| # | Step | Underlying feature | Skippable via sentinel? |
|---|------|--------------------|------------------------|
| 1 | Toolchain install | `winget-manifest` | No — winget handles idempotency internally |
| 2 | SDK dep restore | `deps-manifest` | Yes — `.diaenv/deps/<id>.restored` per dep |
| 3 | Submodule init | `submodule-migration` | Yes — `git submodule status` check |
| 4 | AI context wiring | `claude-context` | Yes — checks if `settings.local.json` exists |

Step 1 requires elevation; if not elevated, steps 2–4 still run and step 1 is skipped with a WARN — the user is shown the command to re-run step 1 as administrator.

## Progress Output

```
[dia env setup]
  [1/4] Toolchain       ... installing via winget ... done (42s)
  [2/4] SDK deps        ... sfml already restored, skipping
                        ... ultralight downloading ... done (8s)
                        ... cef downloading ... done (31s)
  [3/4] Submodules      ... initialising 5 submodules ... done (3s)
  [4/4] AI context      ... settings.local.json written ... done
Setup complete. Run `dia env verify` to confirm environment health.
```

## Implementation

### Files introduced / modified

```
Dia/DiaCLI/
└── dia_cli/
    ├── cli/
    │   └── env/
    │       └── setup_cmd.py              # Extended — add orchestration logic + all flags
    └── commands/
        └── env/
            └── setup_orchestrator.py     # New — ordered step runner with progress output
```

### `setup_orchestrator.py` responsibilities

- Accept a `steps` list (all steps, or subset from flags)
- For each step: call the relevant command module (`toolchain_install_cmd`, `deps_restore_cmd`, submodule init, `claude_context_cmd`)
- Catch step-level failures: log error, set overall exit code to 1, continue remaining steps unless `--fail-fast` passed
- Collect and print a summary at the end: steps passed, steps warned, steps failed
- Emit `OnSetupCompleted(passCount, warnCount, failCount, durationMs)` event

### Elevation handling

```python
import ctypes
def is_admin():
    try:
        return ctypes.windll.shell32.IsUserAnAdmin()
    except:
        return False
```

If not admin: skip toolchain step, print:
```
  [1/4] Toolchain  SKIPPED — not running as administrator
        Re-run: dia env setup --toolchain  (as Administrator)
```

## Dependencies

| Dependency | Type | Notes |
|------------|------|-------|
| `deps-manifest` feature | Hard | `deps_restore_cmd.py` must exist |
| `winget-manifest` feature | Hard | `toolchain_install_cmd.py` must exist |
| `submodule-migration` feature | Hard | `.gitmodules` must be correct before submodule init |
| `claude-context` feature | Hard | `claude_context_cmd.py` must exist |

## Acceptance Criteria

1. `dia env setup` with no flags runs all four steps in order and exits 0 on full success
2. `dia env setup --toolchain` runs only the toolchain step; other steps are skipped
3. `dia env setup --deps --submodules` runs only those two steps in the correct order
4. `dia env setup --dep sfml` restores only the named dep
5. Running `dia env setup` on an already-provisioned machine completes in under 5 seconds (all sentinel checks pass, no network I/O)
6. `dia env setup --force` re-runs all steps regardless of sentinel state
7. If not running as administrator, toolchain step is skipped with a WARN; remaining steps run; exit code is 2 (warnings only)
8. If a step fails (e.g. a dep download fails), the failure is logged, remaining steps continue, and exit code is 1
9. Progress output shows step number, name, status, and duration for each step
10. Final summary line reports overall pass/warn/fail counts and suggests `dia env verify`

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
| PD-005 | Platform | x64 Windows only | Compliant — `ctypes.windll` elevation check is Windows-only; consistent with PD-005 |
| PD-006 | Platform | VS project files are source of truth | Compliant — setup command does not touch any `.vcxproj` files |
| PD-007 | Platform | C++20 required | Compliant — tooling feature; no compiler configuration touched |
| PD-008 | Platform | `Directory.Build.props` owns OutDir/IntDir | Compliant — no build output paths modified |
| AD-001 | Dia App | Module system with YAML frontmatter | Compliant — Python tooling; no new C++ module introduced |
| SD-CLI-001 | DiaCLI | MDK CLI architecture | Compliant — `setup_cmd.py` follows the two-file plugin pattern |
| SD-CLI-002 | DiaCLI | Python-based implementation | Compliant — all implementation is Python |
| SD-CLI-003 | DiaCLI | Separate from C++ DiaAPI | Compliant — no C++ DiaAPI commands introduced |
| SD-CLI-006 | DiaCLI | Click framework for argument parsing | Compliant — `setup_cmd.py` uses Click groups and decorators |
| SD-CLI-008 | DiaCLI | Exit codes follow Unix conventions | Compliant — exits 0 on success, 1 on hard failure, 2 on warnings only |
| SD-ENV-001 | DiaEnv | `deps.json` is single source of truth for binary SDKs | Compliant — setup delegates dep restore to `deps_restore_cmd`; no parallel dep list |
| SD-ENV-002 | DiaEnv | SHA-256 verification required for all downloaded deps | Compliant — SHA-256 is enforced inside `deps_restore_cmd`; setup does not bypass it |
| SD-ENV-004 | DiaEnv | `winget.json` generated by `winget export` and committed | Compliant — setup delegates toolchain install to `toolchain_install_cmd` which reads `winget.json` |
| SD-ENV-010 | DiaEnv | Python 3.11 is the single Python version | Compliant — tooling feature; no Python version constraint introduced |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Step ordering | Should submodule init (step 3) run before dep restore (step 2)? Submodules are source deps; binary SDKs are independent. | No — order is correct. Binary SDK restore (step 2) is independent of submodules. Submodule init (step 3) depends only on `.gitmodules` being correct, which is a one-time migration, not a runtime dep on step 2. |
| 2 | Failure handling | Should a failed step abort the remaining steps, or continue? | Continue by default — a failed dep download should not prevent submodule init or claude-context wiring. Add `--fail-fast` flag to abort on first failure for CI-like use. |
| 3 | Elevation | Should `dia env setup` auto-elevate via UAC, or skip and warn? | Skip and warn — consistent with `winget-manifest` decision. Print the re-run command. |
| 4 | `--dep` flag | `--dep <name>` implies `--deps` — should it error if used with `--toolchain` or `--submodules`? | Yes — `--dep` is a sub-selector of `--deps`; combining with unrelated flags is a usage error. Exit 2 with a clear message. |
| 5 | Progress output | Should progress be suppressed with a `--quiet` flag for scripted use? | Yes — add `--quiet` flag that suppresses progress lines and prints only errors and the final summary line. |
