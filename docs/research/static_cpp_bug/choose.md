# Research: Choice — Static C++ Bug Detection Tools

**Date:** 2026-05-25
**Chosen candidate:** Bundle A — C10 + C1 + C4 + C9

## Rationale

Bundle A was chosen as the first deliverable. It works entirely on Windows today with no CMake dependency, delivers the Claude triage loop (C4) as a first-class feature, and makes analysis permanent via a CI gate (C9). C10 (sanitizers) anchors the bundle because it has zero false positives — every finding is a real bug. C1 (Cppcheck) covers the complementary surface: bugs on paths the test suite never exercises.

## What Was Ruled Out

| Candidate | Reason not chosen |
|-----------|------------------|
| C2 Clang-Tidy CMake mirror | Deferred — wait for CMake migration; trivial then, painful now |
| C3 Clang-Tidy full project shim | Same; shim is fragile and adds friction |
| C5 Cppcheck + Clang-Tidy combined | Superseded by Bundle A + deferred Clang-Tidy |
| C6 Custom Clang AST plugin | L-size, phase 3 after the stack is stable |
| C7 SonarLint | Free to add as IDE nicety but adds nothing to the pipeline |
| C8 CodeChecker server | Overkill for current team size; SARIF + VS Code viewer is sufficient |
| C11 WSL2 / TSan | Added to backlog blocked on Linux/CMake migration |
| C2/C3 Clang-Tidy | Added to backlog blocked on CMake migration |

## Pre-Spec Commitments

- `dia check` command is the single entry point for all analysis tools
- `dia diagnose` is the Claude triage command; reads SARIF/XML from `Cluiche/out/check/`
- Sanitizer configs are build-configuration variants (`Debug-Asan`, `Debug-Ubsan`), not separate projects
- CI gate compares against a stored baseline; new findings = pipeline failure
- No `// NOLINT` sprawl — suppressions live in config files only
- TSan and Clang-Tidy are explicitly backlogged, blocked on Linux/CMake migration

## Next Step

Run `/spec-system` for a new `DiaBugDetection` system (or fold into DiaAPI as a new plugin group).
Suggested parent: DiaAPI (new `check` and `diagnose` command plugins).
