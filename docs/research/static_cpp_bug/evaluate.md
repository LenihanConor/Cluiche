# Research: Evaluate — Static C++ Bug Detection Tools

**Input:** docs/research/static_cpp_bug/ideate.md

## Scoring Criteria

- **Engine Value (0.25):** Improves Dia module reliability or surfaces real engine bugs
- **Game Value (0.20):** Improves CluicheTest stability or unblocks testbed work
- **Implementation Cost (0.25):** Inverse of effort — 5 = very cheap, 1 = very expensive
- **Risk (0.15):** Inverse of uncertainty — 5 = well-understood, 1 = highly uncertain
- **Cluiche Fit (0.15):** Aligns with PD decisions, module structure, CMake migration direction

## Scores

| Candidate | Engine (0.25) | Game (0.20) | Cost (0.25) | Risk (0.15) | Fit (0.15) | Total |
|-----------|---------------|-------------|-------------|-------------|------------|-------|
| C1  Cppcheck + `dia check` | 3 | 3 | 5 | 5 | 4 | **3.90** |
| C2  Clang-Tidy CMake mirror (DiaCore) | 4 | 2 | 3 | 4 | 5 | **3.65** |
| C3  Clang-Tidy full project shim | 4 | 3 | 2 | 3 | 4 | **3.20** |
| C4  `dia diagnose` Claude triage | 3 | 4 | 3 | 4 | 4 | **3.50** |
| C5  Cppcheck + Clang-Tidy combined | 5 | 4 | 2 | 3 | 4 | **3.60** |
| C6  Custom Clang AST plugin | 5 | 2 | 1 | 2 | 4 | **2.85** |
| C7  SonarLint IDE integration | 2 | 3 | 5 | 5 | 3 | **3.40** |
| C8  CodeChecker local server | 4 | 3 | 1 | 3 | 3 | **2.80** |
| C9  `dia check --ci` gate | 3 | 3 | 4 | 5 | 5 | **3.80** |
| C10 Sanitizer build configs (ASan/UBSan) | 5 | 4 | 4 | 5 | 4 | **4.45** |
| C11 WSL2 / TSan Linux target | 5 | 3 | 2 | 3 | 3 | **3.40** |

## Top 3 Candidates

### Rank 1: C10 — Sanitizer build configurations (ASan + UBSan) (score: 4.45)
**Why:** Highest combined score across all axes. ASan and UBSan are supported by MSVC and Clang-cl on Windows today, require no compilation database or build-system changes, and produce zero false positives — every finding is a real bug on a real executed path. For a game engine with custom containers (`DynamicArrayC`, `HashTable`) and a complex multi-phase lifecycle, sanitizer runs over the existing GoogleTest suite will surface real memory safety and undefined-behaviour bugs that static analysis can only guess at. Aligns with PD-007 (C++20) and does not deepen MSVC dependency.
**Watch out for:** ASan adds ~2× runtime overhead — keep sanitizer configs separate from the default Debug build so they don't slow the normal dev loop. Some MSVC ASan limitations exist around heap-allocated `operator new` overrides used in DiaCore pooling; may need per-module suppression.

### Rank 2: C1 — Cppcheck + `dia check` CLI command (score: 3.90)
**Why:** Lowest friction entry point for static analysis — no compilation database, no build-system changes, works today on the MSBuild tree. Cppcheck's flow-sensitive analysis catches uninitialised variables, out-of-bounds paths, and resource leaks that are invisible to the compiler. A `dia check` command establishes the integration seam that all future tools (C4, C5, C9) can extend. Highest cost score (5) because setup is genuinely an afternoon's work.
**Watch out for:** Cppcheck's type-system awareness is weaker than Clang-Tidy — it will miss overload-resolution bugs and template misuse. It's a strong first layer, not a complete solution. Configure `--suppress` carefully to avoid `// NOLINT` sprawl.

### Rank 3: C9 — `dia check --ci` gate (score: 3.80)
**Why:** Pure policy layer — zero new tooling required once C1 or C10 exists. Makes static analysis and sanitizer runs enforceable quality gates rather than advisory reports. By comparing against a baseline SARIF, it surfaces only *new* findings per run, avoiding the "ignore everything because there are 200 warnings" failure mode. Aligns strongly with PD-006 (build pipeline ownership) and is the natural next step after C1.
**Watch out for:** Requires a stable baseline to be useful — if C1 is introduced with 50 pre-existing findings, the gate must accept them all before it can enforce new ones. A one-time baseline acceptance step (`dia check --accept-baseline`) handles this, but it must be done deliberately.

## Recommendation

**C10 (Sanitizer configs)** is the right first move. It delivers the highest bug-finding signal with the lowest setup cost, works on Windows today with both MSVC and Clang-cl, and requires no compilation database or build system changes. Zero false positives mean every finding demands a fix — this directly serves the goal of getting Claude to triage and resolve real bugs. After C10 is wired in, **C1 (Cppcheck)** adds the complementary static layer that catches bugs on unexercised paths. Together they give broad coverage: C10 finds bugs that ran, C1 finds bugs on paths that haven't been tested yet. **C9 (CI gate)** then makes both permanent quality enforcements with one afternoon of policy wiring. This three-step stack (C10 → C1 → C9) honours PD-007 (C++20 toolchain), does not deepen MSVC lock-in, and leaves the door open for C4 (Claude triage) and C11 (TSan via WSL2) as natural follow-ons.
