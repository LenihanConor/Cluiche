# System Spec: DiaBugDetection

**Status:** Approved  
**Parent Application:** @docs/specs/applications/dia.md  
**Research:** docs/research/static_cpp_bug/summary.md

---

## Purpose

DiaBugDetection is the quality-assurance system that finds real bugs in Cluiche/Dia C++ code and gets them fixed autonomously. It combines two complementary analysis layers — runtime sanitizers (zero false positives, exercised paths) and static flow analysis (unexercised paths) — and closes the loop with an agentic Claude triage cycle that applies fixes, verifies they resolve the finding, and stages the result for human review. The human's only required action is `git commit`.

## Responsibilities

- **`dia check`** — Run analysis tools against the Cluiche source tree; produce a SARIF findings file at `Cluiche/out/check/findings.sarif`
- **Sanitizer build configurations** — Own `Debug-Asan` and `Debug-Ubsan` build variants; wire them into `dia run googletest --config Asan|Ubsan`
- **Cppcheck integration** — Run Cppcheck static flow analysis; convert output to SARIF; manage `.cppcheck-suppressions.xml`
- **`dia diagnose`** — Agentic fix loop: read findings, invoke Claude with full context (finding + stack/AST path + surrounding source), apply fix, re-run `dia check`, verify clean; stage changes on success; revert on stuck
- **CI gate** — `dia pipeline` static-analysis stage: compare findings against a stored baseline; fail the pipeline on any new `error`-severity finding
- **Baseline management** — `dia check --accept-baseline` to promote current findings to baseline after deliberate acceptance

## Non-Responsibilities

- **Test execution** — owned by DiaTest (`dia test`)
- **Clang-Tidy** — backlogged, blocked on CMake migration
- **TSan / ThreadSanitizer** — backlogged, blocked on Linux/WSL2 CI target
- **Environment provisioning** — owned by DiaEnv
- **Fixing test failures** — DiaBugDetection fixes static/sanitizer findings only, not GoogleTest assertion failures
- **CI/CD pipeline configuration** — local-only; no remote CI integration in this system

## Public Interfaces

### CLI Commands

```bash
# Run all enabled analysis tools; output to Cluiche/out/check/findings.sarif
dia check

# Run only sanitizer build (compiles + runs GoogleTests under ASan or UBSan)
dia check --tool=sanitizer --config=Asan
dia check --tool=sanitizer --config=Ubsan

# Run only Cppcheck static analysis
dia check --tool=cppcheck

# Promote current findings to baseline (clears the CI gate)
dia check --accept-baseline

# Agentic fix loop — triage all findings
dia diagnose

# Triage a single finding by SARIF result ID
dia diagnose --finding=<id>

# Dry-run: show what Claude would do but apply nothing
dia diagnose --dry-run
```

### Pipeline Stage

```toml
# pipeline.toml addition
[stages.static-analysis]
enabled = true
baseline = "Cluiche/out/check/baseline.sarif"
fail_on_new_severity = "error"   # warning | error | none
```

```bash
# Run via pipeline
dia pipeline --stage static-analysis
dia pipeline --stage static-analysis --target googletest
```

### Output Files

| File | Description |
|------|-------------|
| `Cluiche/out/check/findings.sarif` | Latest full findings from all tools |
| `Cluiche/out/check/baseline.sarif` | Accepted baseline (committed to repo) |
| `Cluiche/out/check/diagnose.md` | Last `dia diagnose` session report |
| `Cluiche/out/check/delta.sarif` | New findings vs baseline (CI gate input) |

### `dia diagnose` Agentic Loop Contract

```
for each finding in findings.sarif:
    1. Gather context:
       - finding metadata (file, line, check, message)
       - N lines of surrounding source
       - stack trace (sanitizer) or AST path (Cppcheck)
       - related headers included at the finding site
    2. Invoke Claude with full context → proposed fix
    3. Apply fix to source file(s)
    4. Re-run dia check --tool=<originating tool> on the affected file(s)
    5. If finding gone → stage changed files (git add), mark finding resolved
    6. If finding persists after MAX_ATTEMPTS:
       - Revert all changes for this finding (git checkout -- <files>)
       - Mark finding "needs-human" in diagnose.md
       - Continue to next finding
    7. Write session summary to Cluiche/out/check/diagnose.md
```

**Human gate:** After `dia diagnose` completes, the working tree contains staged fixes (via `git add`) for all resolved findings. The human reviews with `git diff --staged` and runs `git commit`. Stuck findings are listed in `diagnose.md` with Claude's last attempted fix and the reason it was reverted.

**MAX_ATTEMPTS:** 3 per finding (configurable in `pipeline.toml`).

## Dependencies

| System | Role |
|--------|------|
| DiaAPI | Command registration for `check` and `diagnose` commands |
| DiaCLI | Command routing / `dia` entrypoint; Python subprocess management |
| DiaPipeline | Hosts the `static-analysis` pipeline stage |
| GoogleTests | The binary that runs under ASan/UBSan |
| Anthropic SDK (Python) | Claude invocation inside `dia diagnose` |

## Features

| Feature | Description | Status |
|---------|-------------|--------|
| `sanitizer-configs` | Debug-Asan and Debug-Ubsan build configurations; `dia check --tool=sanitizer` | [sanitizer-configs.md](../../features/dia/diabugdetection/sanitizer-configs.md) Approved |
| `cppcheck-integration` | Cppcheck install, `.cppcheck-suppressions.xml`, `dia check --tool=cppcheck`, SARIF output | [cppcheck-integration.md](../../features/dia/diabugdetection/cppcheck-integration.md) Approved |
| `dia-diagnose-loop` | Agentic Claude fix loop with context gather, apply, verify, stage/revert | [dia-diagnose-loop.md](../../features/dia/diabugdetection/dia-diagnose-loop.md) Approved |
| `ci-gate` | `dia pipeline` static-analysis stage; baseline comparison; fail on new error findings | [ci-gate.md](../../features/dia/diabugdetection/ci-gate.md) Approved |

## Inherited Binding Decisions

| Decision | How this system honours it |
|----------|---------------------------|
| PD-001 StringCRC for identifiers | `dia check` and `dia diagnose` command names registered via `StringCRC` constants in DiaAPI |
| PD-002 ProcessingUnit/Phase/Module architecture | `dia check` and `dia diagnose` are CLI commands, not runtime modules — no PU/Phase involvement |
| PD-004 No STL in public APIs | DiaBugDetection is a DiaCLI Python system; no C++ public API surfaces — PD-004 does not apply to Python CLI code |
| PD-005 x64 Windows only | All tools (Cppcheck, MSVC ASan, Clang-cl UBSan) target Windows x64; sanitizer configs set `/MACHINE:X64` |
| PD-006 VS project files are source of truth | Sanitizer build configs are new solution configurations in `Cluiche.sln` and `Directory.Build.props`; no per-project overrides |
| PD-007 C++20 required | Sanitizer configs inherit `/std:c++20` from `Directory.Build.props` (PD-008); no override |
| PD-008 Directory.Build.props owns build settings | `Debug-Asan` and `Debug-Ubsan` configurations added via `Directory.Build.props` conditionals; no per-project duplication |
| PD-009 Generated output under `Cluiche/out/` | All findings files, baseline, and diagnose report live under `Cluiche/out/check/`; fully gitignored except `baseline.sarif` |
| PD-010 `.diagame` is project root | DiaBugDetection has no `.diagame` dependency — it operates on source files, not game project files |

## AI Review Questions

| # | Question | Answer |
|---|----------|--------|
| 1 | Does `dia diagnose` need a project-level API key config for Claude, or does it inherit from the shell environment (`ANTHROPIC_API_KEY`)? | Shell environment (`ANTHROPIC_API_KEY`). No in-repo key storage. |
| 2 | Which model should `dia diagnose` use? | `claude-sonnet-4-6` by default (configurable in `pipeline.toml`); `claude-opus-4-7` for complex multi-file findings |
| 3 | What is MAX_ATTEMPTS? Is it per-finding or per-session? | Per-finding, default 3. Configurable. If a finding fails all attempts, revert and continue — never block the loop. |
| 4 | Should `baseline.sarif` be committed to the repo? | Yes — it is the CI gate's source of truth. Lives at `Cluiche/out/check/baseline.sarif`; the `out/` directory is gitignored by default but this file is explicitly un-ignored. |
| 5 | What happens if `dia diagnose` is run with a dirty working tree? | Abort with a clear error: "Working tree is dirty. Commit or stash changes before running dia diagnose." Prevents clobbering in-progress work. |
| 6 | Should `dia diagnose` fix all findings in one session, or only new findings (delta vs baseline)? | Only findings in `delta.sarif` (new vs baseline) by default. `--all` flag to diagnose all findings including pre-existing. |
| 7 | How does the context gather step handle findings in generated code or third-party headers? | Skip findings where the file path is under `External/` or matches a pattern in `.cppcheck-suppressions.xml`. Only fix findings in `Dia/` and `Cluiche/` source trees. |
| 8 | What is the suppression strategy for Cppcheck false positives in engine patterns (C-style casts, custom allocators)? | Central `.cppcheck-suppressions.xml` at repo root. No inline `// cppcheck-suppress` comments. Suppressions reviewed when `--accept-baseline` is run. |
| 9 | Does `dia check` need to build before running sanitizers, or does it assume a pre-built binary? | `dia check --tool=sanitizer` always builds first (calls `dia pipeline --stage compile-code --config Asan`). `dia check --tool=cppcheck` does not build. |
| 10 | How should `dia diagnose` handle a finding that requires changes across multiple files? | Claude receives all relevant files as context. Changes applied atomically — all files staged together or all reverted together if the finding persists. |

## Decisions

| ID | Decision | Rationale | Binding |
|----|----------|-----------|---------|
| BD-001 | `dia diagnose` uses shell `ANTHROPIC_API_KEY`; no in-repo key | Security — API keys must never be committed | Yes |
| BD-002 | `baseline.sarif` is committed to the repo | CI gate needs a stable reference point; baseline is intentional state, not noise | Yes |
| BD-003 | `dia diagnose` aborts on dirty working tree | Prevents clobbering in-progress work; Claude fixes must start from a clean state | Yes |
| BD-004 | Suppressions in central config files only; no inline suppression comments | Prevents `// NOLINT` / `// cppcheck-suppress` sprawl; suppressions are auditable | Yes |
| BD-005 | `dia diagnose` stages (git add) on success; never commits | Human retains full control over commit message and review; C1 gate from research | Yes |
| BD-006 | `dia diagnose` reverts on stuck (S2); leaves working tree clean | Human gets a clean state + a diagnose.md report; S2 gate from research | Yes |
