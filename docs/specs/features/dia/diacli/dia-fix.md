# Feature Spec: dia-fix

## Parent System
@docs/specs/systems/dia/diacli.md

## Status
`Done` — [Plan](dia-fix.plan.md)

## Summary

Implement a `dia fix <target>` command that runs an automated aider test-fix loop against any valid `dia run <target>`. The loop builds and runs tests, feeds failures to a local LLM (via Ollama by default), applies code edits, and re-runs until all tests pass or aider gives up. Cloud models are supported via `--model` flag.

## Problem

Running the test-fix loop against GoogleTests requires manual iteration — run tests, read failures, edit code, repeat — which is slow and expensive when using cloud LLMs.

## Goals

- `dia fix <target>` invokes `aider --test-cmd "dia run <target>"` with sensible defaults
- Local Ollama model (`qwen2.5-coder:14b`) is the default — zero cloud cost per iteration
- Cloud models selectable via `--model` for tasks that need stronger reasoning
- `--filter` scopes the test run to a subset (passed through to `dia run`)
- Non-interactive — safe for scripting and CI
- `dia env verify` detects missing `aider` / `ollama` and reports clearly
- `dia env setup` installs or guides installation of `aider` and `ollama`

## Non-Goals

- Routing plan model-column tasks automatically (UC2 — future feature)
- Supporting targets that are not valid `dia run` targets
- Persisting fix history or sessions
- Running multiple targets in a single invocation

## Acceptance Criteria

1. `dia fix <target>` runs an aider test-fix loop using `ollama/qwen2.5-coder:14b` by default
2. `<target>` maps directly to `dia run <target>` — any valid `dia run` target is supported
3. `--filter=<value>` is passed through to `dia run <target> --filter=<value>` when provided
4. `--model=<model>` overrides the default (supports any aider-compatible model string — local or cloud)
5. `--config=<config>` is passed through to `dia run <target> --config=<config>` (default: `Debug`)
6. The command is non-interactive (`--yes` passed to aider) — safe for scripting
7. Exit code 0 = all tests pass; non-zero = failures remain or error occurred
8. `dia env verify` reports FAIL with a fix hint if `aider` is not installed
9. `dia env verify` reports FAIL with a fix hint if `ollama` is not installed
10. `dia env setup` installs `aider` via `pip install aider-chat` or prints clear instructions if it cannot
11. `dia env setup` prints clear instructions to install Ollama and pull `qwen2.5-coder:14b`

## Known Limitation

`dia fix` is only as good as `dia run`'s ability to surface failures. Silent failures (e.g. logged to a future observability platform but not propagated to exit code) will not be caught — aider will see exit 0 and declare victory. The fix for this lies in `dia run`, not `dia fix`.

## Files

### New

```
Dia/DiaCLI/dia_cli/cli/fix.py
Dia/DiaCLI/dia_cli/commands/fix/__init__.py
Dia/DiaCLI/dia_cli/commands/fix/group.py
Dia/DiaCLI/dia_cli/commands/fix/fix_handler.py
```

### Modified

```
Dia/DiaCLI/dia_cli/commands/env/verify_orchestrator.py   # add aider + ollama checks
Dia/DiaCLI/dia_cli/commands/env/setup_orchestrator.py    # add aider + ollama install/guidance
```

## Tasks

| # | Task | Description |
|---|------|-------------|
| 1 | `dia fix` command skeleton | `cli/fix.py`, `commands/fix/group.py` with Click wiring; no handler logic yet |
| 2 | `fix_handler.py` | Build aider invocation from args, subprocess call, exit code propagation |
| 3 | `dia env verify` — aider + ollama checks | Add two `CheckResult` entries to `verify_orchestrator.py` under a new `local-llm` category |
| 4 | `dia env setup` — aider + ollama guidance | Add install steps to `setup_orchestrator.py` |
| 5 | Tests | Unit tests for `fix_handler` argument construction; integration smoke test |

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaCLI | @docs/specs/systems/dia/diacli.md |

## Binding Decisions Compliance

| ID | Source | Decision | Compliance |
|----|--------|----------|------------|
| PD-001 | Platform | Use StringCRC for all identifiers | Compliant — Python tooling only; no C++ identifiers involved |
| PD-002 | Platform | ProcessingUnit/Phase/Module architecture | Compliant — tooling only; no runtime components |
| PD-003 | Platform | Component-based entities | Compliant — tooling only |
| PD-004 | Platform | No STL containers in public APIs | Compliant — Python only |
| PD-005 | Platform | x64 Windows only | Compliant — no platform-specific output logic; aider and ollama are Windows-compatible |
| PD-006 | Platform | VS project files are source of truth | Compliant — no `.vcxproj` files touched |
| PD-007 | Platform | C++20 required | Compliant — tooling; no compiler configuration |
| PD-008 | Platform | `Directory.Build.props` owns OutDir/IntDir | Compliant — no build output paths touched |
| PD-009 | Platform | All generated non-binary output under `Cluiche/out/<AppName>/` | Compliant — no generated output; aider writes edits directly to source files |
| AD-001 | Dia App | Module system with YAML frontmatter | Compliant — Python tooling only |
| AD-002 | Dia App | No STL containers in public APIs | Compliant — Python only |
| AD-003 | Dia App | Namespace convention `Dia::<Module>::` | Compliant — Python tooling; no C++ namespaces |
| AD-004 | Dia App | ProcessingUnit/Phase/Module for app structure | Compliant — tooling only |
| AD-005 | Dia App | Component-based entities | Compliant — tooling only |
| SD-CLI-001 | DiaCLI | MDK CLI architecture as foundation | Compliant — follows existing plugin pattern: `cli/fix.py` + `commands/fix/` |
| SD-CLI-002 | DiaCLI | Python-based implementation | Compliant |
| SD-CLI-003 | DiaCLI | Separate from C++ DiaAPI system | Compliant — pure Python orchestration; no DiaAPI calls |
| SD-CLI-006 | DiaCLI | Click framework for argument parsing | Compliant — uses `@click.command` and `@click.option` |
| SD-CLI-008 | DiaCLI | Exit codes follow Unix conventions | Compliant — exit 0 = success, non-zero = failure; propagated from aider |

## AI Review Questions

| # | Section | Question | Suggested Default | Answer |
|---|---------|----------|-------------------|--------|
| 1 | fix_handler | Should `--yes` (auto-confirm all edits) always be forced, or should it be opt-in? | Always forced — non-interactive is an AC | Always forced. |
| 2 | fix_handler | Should aider's output be streamed to the terminal or captured and re-emitted via `OutputContext`? | Stream directly — aider has its own rich output; capturing it adds complexity for no benefit | Stream directly. |
| 3 | env verify | Should the ollama check also verify that `qwen2.5-coder:14b` is pulled, or just that ollama is installed? | Check both — a missing model is a silent runtime failure | Check both. |
| 4 | env setup | Can `aider-chat` be installed automatically via `pip install aider-chat`, or should the command just print instructions? | Auto-install via pip — same pattern as other tool installs in DiaCLI | Auto-install via pip. |
| 5 | env setup | Should `dia env setup` pull `qwen2.5-coder:14b` automatically via `ollama pull`, or just print instructions? | Print instructions — pulling 8GB automatically without user consent is too aggressive | Print instructions only. |
| 6 | fix_handler | What files should be passed to aider as context? The whole `Dia/` tree, or scoped to the target? | Rely on aider's repo-map (pass no explicit files) — repo-map selects relevant context automatically | Rely on aider's repo-map; pass no explicit files. |
| 7 | Blockers | aider, ollama, and qwen2.5-coder:14b are not yet installed — is this spec blocked from implementation? | Yes — mark as blocked until prerequisites are installed | Not a spec blocker — install steps are Task 0 in the implementation plan. |
