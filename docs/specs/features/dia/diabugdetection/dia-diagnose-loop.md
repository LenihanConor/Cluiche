# Feature Spec: dia-diagnose-loop

**Status:** Approved  
**Parent System:** @docs/specs/systems/dia/diabugdetection.md  
**Research:** docs/research/static_cpp_bug/summary.md

---

## Summary

Implement `dia diagnose` — an agentic Claude fix loop that reads `delta.sarif` (new findings vs baseline), gathers rich context per finding (40 lines each side + stack trace), invokes Claude to propose and apply a fix, re-runs the relevant check to verify, stages the fix via `git add` on success, and reverts cleanly after MAX_ATTEMPTS (3) on failure. The human's only action is `git commit` after reviewing staged changes. Operates on both Cppcheck and sanitizer findings (both are SARIF). Aborts if the working tree is dirty.

## Problem

Static analysis and sanitizer findings are a passive report. Without an automated fix loop they accumulate and get ignored. `dia diagnose` closes this gap by making every finding actionable — Claude triages, fixes, and verifies; the developer reviews and commits.

## Acceptance Criteria

- [ ] `dia diagnose` aborts immediately if `git status` reports a dirty working tree, with a clear error message (BD-003)
- [ ] `dia diagnose` reads `Cluiche/out/check/delta.sarif` by default; `--all` flag reads `findings.sarif` instead
- [ ] `dia diagnose --finding=<ruleId:location>` triages a single finding only
- [ ] `dia diagnose --dry-run` prints what Claude would do but applies no file changes
- [ ] For each finding, context gathered includes: 40 lines above + below the finding location, full file path, rule ID and message, sanitizer stack trace or Cppcheck AST path if present, all `#include` directives in the affected file
- [ ] Claude is invoked with model `claude-sonnet-4-6` by default; configurable in `pipeline.toml`
- [ ] Claude's proposed fix is applied directly to the source file(s)
- [ ] After applying, `dia check --tool=<originating-tool>` is re-run on the affected file(s) only; if the finding is gone the fix is confirmed
- [ ] On confirmed fix: all changed files are staged (`git add`); finding marked resolved in session report
- [ ] On failure after MAX_ATTEMPTS (3): all changes for that finding are reverted (`git checkout -- <files>`); finding marked `needs-human` in session report; loop continues to next finding (BD-006)
- [ ] Multi-file fixes: all files for a finding are staged together or all reverted together — never partially staged
- [ ] `ANTHROPIC_API_KEY` read from shell environment; `dia diagnose` fails with a clear error if not set (BD-001)
- [ ] Session report written to `Cluiche/out/check/diagnose.md` on every run, even if all findings are `needs-human`
- [ ] Findings in `External/` paths are silently skipped — only `Dia/` and `Cluiche/` source trees are fixed
- [ ] `dia diagnose` does not invoke `git commit` under any circumstances (BD-005)

## Tasks

| # | Task | Notes |
|---|------|-------|
| 1 | Create `Dia/DiaCLI/diagnose.py` with CLI arg parsing | `--finding`, `--dry-run`, `--all`; validate `ANTHROPIC_API_KEY` present |
| 2 | Implement dirty-working-tree guard | `git status --porcelain`; abort if non-empty |
| 3 | Implement context gather | Read finding from SARIF; extract 40 lines each side; collect includes; attach stack trace if present |
| 4 | Implement Claude invocation | Anthropic Python SDK; structured prompt with finding + context; parse fix from response |
| 5 | Implement fix application | Write Claude's proposed changes to source file(s); handle multi-file atomically |
| 6 | Implement verify step | Re-run `dia check --tool=<tool> --file=<path>` on affected files; check if ruleId+location gone from output |
| 7 | Implement stage-on-success | `git add <changed files>` on confirmed fix |
| 8 | Implement revert-on-stuck | `git checkout -- <changed files>` after MAX_ATTEMPTS; mark `needs-human` |
| 9 | Write session report to `diagnose.md` | Resolved findings (file, rule, fix summary) + needs-human findings (file, rule, last attempted fix, reason reverted) |
| 10 | Add `dia diagnose` to DiaAPI command registry | Register via `StringCRC` |
| 11 | Add `max_attempts` and `model` to `pipeline.toml` | Under `[diagnose]` section; defaults: `max_attempts=3`, `model="claude-sonnet-4-6"` |

## Traceability

| Level | Spec |
|-------|------|
| Platform | @docs/specs/platform/Cluiche.md |
| Application | @docs/specs/applications/dia.md |
| System | @docs/specs/systems/dia/diabugdetection.md |

## Binding Decisions Compliance

| Decision | Plain language | Compliance |
|----------|---------------|------------|
| PD-001 StringCRC | Identifiers use StringCRC | Compliant — `dia diagnose` command registered via StringCRC |
| PD-004 No STL in public APIs | DiaCore containers only in public C++ APIs | Compliant — `diagnose.py` is Python; no new C++ public API |
| PD-005 x64 Windows only | All builds target x64 Windows | Compliant — Python CLI runs on Windows; `git` and Anthropic SDK are cross-platform |
| PD-009 Generated output under `Cluiche/out/` | Non-binary output under `Cluiche/out/<AppName>/` | Compliant — `diagnose.md`, `delta.sarif` under `Cluiche/out/check/` |
| BD-001 API key via shell env | `ANTHROPIC_API_KEY` from shell, not repo | Compliant — key read from `os.environ`; error if missing |
| BD-003 Abort on dirty working tree | `dia diagnose` aborts if dirty | Compliant — first action is `git status --porcelain` check |
| BD-004 No inline suppression comments | Suppressions in config files only | Compliant — Claude is instructed never to add `// cppcheck-suppress` or `// NOLINT` inline comments as fixes |
| BD-005 Stage only, never commit | Human retains commit control | Compliant — `git add` only; `git commit` never called |
| BD-006 Revert on stuck | Clean working tree on failure | Compliant — `git checkout -- <files>` after MAX_ATTEMPTS |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Claude prompt | Should the prompt include the full file or just the 40-line window? | 40-line window + all `#include` directives at the top of the file. Full file risks context overflow on large engine headers; includes give Claude enough type context |
| 2 | Multi-file fixes | How does Claude signal which files to change? | Claude responds with a structured diff block per file (unified diff format); `diagnose.py` parses and applies each diff independently |
| 3 | Verify step | Re-running `dia check` on a single file — does Cppcheck support `--file=<path>`? | Yes — `cppcheck <filepath>` analyses a single file; use same suppression file. For sanitizers, re-running the full GoogleTests binary is required |
| 4 | Model override | Should `--model` be a CLI flag on `dia diagnose`? | Yes — `dia diagnose --model=claude-opus-4-7` for complex findings; defaults to `pipeline.toml` value |
| 5 | Rate limiting | What if the Anthropic API rate-limits mid-session? | Catch `RateLimitError`; wait with exponential backoff (max 3 retries); if all retries exhausted, mark finding `needs-human` and continue |
| 6 | Instruction to Claude | Should Claude be told about Cluiche coding conventions (PD-004, naming, no STL)? | Yes — system prompt includes a brief conventions block: no STL in public headers, use DiaCore containers, PascalCase methods, `m` prefix members |
| 7 | Dry-run output | What does `--dry-run` print? | For each finding: the proposed diff Claude would apply, without writing any files; session report written with all entries marked `dry-run` |
