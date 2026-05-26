# Feature Spec: ci-gate

**Status:** Approved  
**Parent System:** @docs/specs/systems/dia/diabugdetection.md  
**Research:** docs/research/static_cpp_bug/summary.md

---

## Summary

Add a `static-analysis` stage to `dia pipeline` that runs Cppcheck, compares findings against a committed `baseline.sarif`, and exits non-zero if any new `error`-severity findings are introduced. `dia check --accept-baseline` promotes the current findings to baseline. Sanitizers are excluded from the gate — they are user-invoked only. Nothing runs on `dia run`.

## Problem

Without an enforcement point, static analysis findings accumulate silently — new bugs are introduced and never noticed. The CI gate makes Cppcheck findings a hard quality boundary: new `error`-severity findings fail the pipeline stage, preventing regressions from landing unnoticed.

## Acceptance Criteria

- [ ] `dia pipeline --stage static-analysis` runs Cppcheck and compares against `Cluiche/out/check/baseline.sarif`
- [ ] If any new `error`-severity findings exist (present in `findings.sarif` but not `baseline.sarif`), the stage exits non-zero and prints a summary of new findings
- [ ] `delta.sarif` is written to `Cluiche/out/check/delta.sarif` on every gate run (used by `dia diagnose` as its default input)
- [ ] `dia check --accept-baseline` copies `findings.sarif` to `baseline.sarif`; prints count of accepted findings
- [ ] `baseline.sarif` is committed to the repo at `Cluiche/out/check/baseline.sarif`; gitignore explicitly un-ignores it
- [ ] An empty `baseline.sarif` (zero findings) is committed at feature ship — forces the first `--accept-baseline` to be deliberate
- [ ] `warning`-severity findings never fail the gate — advisory only
- [ ] The gate runs Cppcheck only — sanitizer runs are never triggered automatically (user-invoked only)
- [ ] `dia pipeline` without `--stage static-analysis` is completely unaffected — gate is opt-in
- [ ] Gate completion time (Cppcheck on full source tree) is reported in pipeline stage output

## Tasks

| # | Task | Notes |
|---|------|-------|
| 1 | Add `static-analysis` stage definition to `pipeline.toml` schema | `[stages.static-analysis]` with `enabled`, `baseline`, `fail_on_new_severity` keys |
| 2 | Implement stage runner in DiaCLI pipeline | Calls `dia check --tool=cppcheck`; reads `findings.sarif`; diffs against `baseline.sarif` |
| 3 | Implement SARIF diff logic | Compare by `ruleId` + `physicalLocation` (uri + startLine); new = in findings but not baseline |
| 4 | Implement `delta.sarif` writer | Write only new findings; SARIF 2.1.0 format; consumed by `dia diagnose` |
| 5 | Implement exit-code logic | Exit 1 if any new finding has `level: error`; exit 0 otherwise |
| 6 | Implement `dia check --accept-baseline` | Copy `findings.sarif` → `baseline.sarif`; print summary |
| 7 | Commit empty `baseline.sarif` to repo | Minimal valid SARIF 2.1.0 with zero results |
| 8 | Update `.gitignore` to un-ignore `baseline.sarif` | `Cluiche/out/check/` is gitignored; add `!Cluiche/out/check/baseline.sarif` exception |
| 9 | Print new findings summary on gate failure | File, line, rule ID, message for each new error finding |

## Traceability

| Level | Spec |
|-------|------|
| Platform | @docs/specs/platform/Cluiche.md |
| Application | @docs/specs/applications/dia.md |
| System | @docs/specs/systems/dia/diabugdetection.md |

## Binding Decisions Compliance

| Decision | Plain language | Compliance |
|----------|---------------|------------|
| PD-001 StringCRC | Identifiers use StringCRC | Compliant — pipeline stage name registered via StringCRC in DiaPipeline |
| PD-004 No STL in public APIs | DiaCore containers only in public C++ APIs | Compliant — gate is Python CLI; no new C++ public API |
| PD-005 x64 Windows only | All builds target x64 Windows | Compliant — Python and Cppcheck run on Windows x64 |
| PD-006 VS project files are source of truth | MSBuild is the build system | Compliant — gate does not modify `.vcxproj` files; reads source tree only |
| PD-009 Generated output under `Cluiche/out/` | Non-binary output under `Cluiche/out/<AppName>/` | Compliant — `delta.sarif`, `findings.sarif` under `Cluiche/out/check/`; `baseline.sarif` is intentional committed state |
| BD-002 baseline.sarif committed to repo | CI gate's source of truth is in version control | Compliant — `baseline.sarif` committed; gitignore exception added |
| BD-004 No inline suppression comments | Suppressions in config files only | Compliant — gate uses same `.cppcheck-suppressions.xml` as `cppcheck-integration`; no inline comments |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Severity threshold | Should `warning` findings ever escalate to gate failures in future? | Yes, via `fail_on_new_severity = "warning"` in `pipeline.toml` — the threshold is configurable; default is `error` only |
| 2 | SARIF diff | How to handle findings that move to a different line number (e.g. after unrelated edits)? | Match by `ruleId` + `uri` + `startLine`; a finding that moves lines is treated as a new finding at the new line and resolved at the old line. Acceptable noise — baseline re-acceptance handles it |
| 3 | Empty baseline | Should the initial committed `baseline.sarif` be empty (zero findings) or seeded with current findings? | Empty — forces the team to run `dia check` and deliberately accept the initial finding set. Shipping with a pre-seeded baseline hides existing technical debt |
| 4 | Gate in `dia pipeline` default run | Is `static-analysis` in the default `dia pipeline` stage list? | No — opt-in only via `dia pipeline --stage static-analysis`. Adding it to the default would slow the standard build pipeline |
| 5 | Relation to `dia diagnose` | Should the gate auto-trigger `dia diagnose` on failure? | No — gate reports findings and exits; `dia diagnose` is a separate deliberate invocation. Auto-triggering would mix read-only gate semantics with source-modifying fix semantics |
