# Research Summary — Static C++ Bug Detection Tools

**Session folder:** docs/research/static_cpp_bug/
**Date:** 2026-05-25

## One-Line Answer

Wire ASan/UBSan sanitizer build configs, Cppcheck, a `dia diagnose` Claude triage command, and a CI gate into a single `DiaBugDetection` system under DiaAPI — delivering zero-false-positive runtime checks and static path coverage today, with Clang-Tidy and TSan queued for when CMake/Linux land.

## Journey

1. **Explored:** Mapped 3 distinct layers (runtime sanitizers, static analysis, action/policy), identified the Windows TSan gap, and surfaced the `compile_commands.json` dependency that blocks Clang-Tidy today.
2. **Ideated:** 11 candidates generated across all layers and sizes (S–L); sanitizers and static tools are orthogonal, not competing.
3. **Evaluated:** C10 (sanitizers) scored highest (4.45) — zero false positives, works on Windows today, no build-system changes.
4. **Chose:** Bundle A (C10 + C1 + C4 + C9); TSan and Clang-Tidy explicitly backlogged blocked on Linux/CMake migration.

## Chosen Work Item

**Name:** DiaBugDetection system — Bundle A
**Home module:** DiaAPI (new `check` and `diagnose` command plugins)
**Suggested spec type:** System
**Estimated size:** M (3 weeks)

### Features in scope
| Feature | What it delivers |
|---------|-----------------|
| C10 Sanitizer build configs | `Debug-Asan` and `Debug-Ubsan` configs; `dia run googletest --config Asan` |
| C1 Cppcheck + `dia check` | Static flow analysis; findings to `Cluiche/out/check/cppcheck.xml` + SARIF |
| C4 `dia diagnose` | Claude reads SARIF, outputs root-cause + fix proposals to `out/check/diagnose.md` |
| C9 CI gate | `dia pipeline` static-analysis stage; baseline comparison; fail on new error-severity findings |

## Key Insights from Exploration

- **Sanitizers and static analysis are orthogonal** — sanitizers catch bugs on exercised paths (zero false positives); static analysis catches bugs on paths tests never hit. Both are needed.
- **TSan is blocked on Windows** — the only reliable race detector for the Main/Render/Sim threading model requires Linux/WSL2; this is the strongest argument for a Linux CI target.
- **Clang-Tidy is blocked on `compile_commands.json`** — trivial once CMake is in place, painful with MSBuild shims. Don't force it; wait for the migration.
- **C4 (`dia diagnose`) is the differentiator** — without it, static analysis produces a report that gets ignored. The Claude triage loop is what makes findings actionable.
- **False-positive budget matters** — enabling too many Cppcheck checks at once buries real bugs in noise. Start with `--enable=warning,performance,portability` and expand from there.
- **Suppression policy upfront** — use `.cppcheck-suppressions.xml` and SARIF baseline, never `// NOLINT` inline; inline suppressions spread and rot.

## Discarded Candidates

| Candidate | Why discarded |
|-----------|--------------|
| C2/C3/C5 Clang-Tidy | Blocked on CMake migration; backlogged |
| C6 Custom Clang plugin | L-size, phase 3; too expensive before the basic stack is proven |
| C7 SonarLint | IDE nicety only; nothing added to pipeline or Claude integration |
| C8 CodeChecker server | Overkill for current team size; SARIF file + VS Code viewer is sufficient |
| C11 TSan/WSL2 | Blocked on Linux target; backlogged |

## References

- docs/research/static_cpp_bug/explore.md
- docs/research/static_cpp_bug/ideate.md
- docs/research/static_cpp_bug/evaluate.md
- docs/research/static_cpp_bug/choose.md
