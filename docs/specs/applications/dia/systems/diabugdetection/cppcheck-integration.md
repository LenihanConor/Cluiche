# Feature Spec: cppcheck-integration

**Status:** Approved  
**Parent System:** @docs/specs/applications/dia/systems/diabugdetection/diabugdetection.md  
**Research:** docs/research/static_cpp_bug/summary.md

---

## Summary

Install Cppcheck via `dia env setup` (winget), run it against the `Dia/` and `Cluiche/` source trees via `dia check --tool=cppcheck`, and produce a `findings.sarif` file at `Cluiche/out/check/`. Manage suppressions centrally in `.cppcheck-suppressions.xml` at the repo root. All invocations are user-triggered — nothing runs on the default build or test path.

## Problem

Static flow analysis is missing from the project. Bugs on unexercised code paths — uninitialised variables, out-of-bounds reads, resource leaks — are invisible to the compiler and sanitizers alike. Cppcheck catches these without requiring a compilation database or build-system changes.

## Acceptance Criteria

- [ ] `dia env setup` installs Cppcheck via `winget install Cppcheck.Cppcheck` if not present
- [ ] `dia env verify` reports the installed Cppcheck version; warns if missing
- [ ] `dia check --tool=cppcheck` runs Cppcheck against `Dia/` and `Cluiche/` source trees with `--enable=warning,performance,portability`
- [ ] System headers (`External/`, Windows SDK paths) are excluded from analysis
- [ ] Findings are written to `Cluiche/out/check/findings.sarif` in SARIF 2.1.0 format
- [ ] A `.cppcheck-suppressions.xml` exists at the repo root; suppresses known false-positive patterns (C-style casts, custom allocators)
- [ ] No inline `// cppcheck-suppress` comments added to source files (BD-004)
- [ ] `dia check` (no `--tool` flag) runs Cppcheck by default
- [ ] `dia check --tool=cppcheck` completes in under 3 minutes on the full source tree on a developer workstation
- [ ] `dia run googletest` and `dia run cluichetest` are completely unaffected

## Tasks

| # | Task | Notes |
|---|------|-------|
| 1 | Add Cppcheck to `dia env setup` winget install list | `winget install Cppcheck.Cppcheck`; skip if already present |
| 2 | Add Cppcheck version check to `dia env verify` | `cppcheck --version`; warn if not found |
| 3 | Create `Dia/DiaCLI/check.py` with `cppcheck` sub-command | Builds Cppcheck CLI args; runs subprocess; streams output |
| 4 | Implement SARIF 2.1.0 output conversion | Convert Cppcheck XML output (`--xml`) to SARIF; write to `Cluiche/out/check/findings.sarif` |
| 5 | Create `.cppcheck-suppressions.xml` at repo root | Suppress C-style cast warnings, custom allocator patterns; document each suppression with rationale |
| 6 | Wire `dia check` default to run Cppcheck | No `--tool` flag = Cppcheck; `--tool=cppcheck` explicit alias |
| 7 | Exclude `External/` and system headers | Pass `--suppress-xml` and `--suppress=*:External/*` |

## Traceability

| Level | Spec |
|-------|------|
| Platform | @docs/specs/platform/Cluiche.md |
| Application | @docs/specs/applications/dia/dia.md |
| System | @docs/specs/applications/dia/systems/diabugdetection/diabugdetection.md |

## Binding Decisions Compliance

| Decision | Plain language | Compliance |
|----------|---------------|------------|
| PD-001 StringCRC | Identifiers use StringCRC | Compliant — `dia check` command registered via StringCRC; Cppcheck is a Python subprocess call, no new C++ public API |
| PD-004 No STL in public APIs | DiaCore containers only in public C++ APIs | Compliant — no new C++ public API; Python CLI only |
| PD-005 x64 Windows only | All builds target x64 Windows | Compliant — Cppcheck Windows x64 binary installed via winget |
| PD-006 VS project files are source of truth | MSBuild is the build system | Compliant — Cppcheck does not require a compilation database; does not touch `.vcxproj` files |
| PD-009 Generated output under `Cluiche/out/` | Non-binary output lives under `Cluiche/out/<AppName>/` | Compliant — `findings.sarif` written to `Cluiche/out/check/` |
| BD-004 No inline suppression comments | Suppressions in config files only | Compliant — all suppressions in `.cppcheck-suppressions.xml`; no `// cppcheck-suppress` in source |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Check set | Which Cppcheck `--enable` categories to start with? | `warning,performance,portability` — excludes `style` (too noisy) and `unusedFunction` (false positives on engine registration patterns) |
| 2 | Scope | Should `Cluiche/Tests/` be included in the analysis scope? | Yes — test code has the same quality bar; bugs in test infrastructure cause false-passing tests |
| 3 | SARIF | Which SARIF fields are required for `dia diagnose` to work? | `ruleId`, `message.text`, `locations[0].physicalLocation` (uri + startLine), `relatedLocations` if available |
| 4 | Performance | If Cppcheck takes >3 min, what's the mitigation? | Pass `--max-ctu-depth=4` (default is 2 which is fast) and use `--jobs=$(nproc)` for parallelism |
| 5 | Suppressions | Who reviews `.cppcheck-suppressions.xml` to prevent suppression accumulation? | Suppressions reviewed manually when `dia check --accept-baseline` is run; each suppression must have a `<!-- rationale -->` comment |
