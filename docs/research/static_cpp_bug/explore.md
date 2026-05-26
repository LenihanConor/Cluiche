# Research: Explore — Static C++ Bug Detection Tools

**Session date:** 2026-05-25
**Folder:** docs/research/static_cpp_bug/

## Problem Space Overview

Static analysis for C++ finds bugs at compile time (or just after) without executing the program. It catches classes of defect that are invisible to the runtime: null pointer dereferences on paths never exercised in tests, use-after-free, integer overflow, resource leaks, uninitialized reads, dead code, and API misuse. For a game engine like Dia — where multi-threaded ProcessingUnits share state and the component/phase lifecycle has strict ordering invariants — static analysis can surface races, dangling pointers, and bad phase-order assumptions before they become intermittent crashes.

The secondary goal here is closing the loop with Claude: once findings are produced as structured data (SARIF, XML, plain-text), they can be piped into a `dia` CLI command that invokes Claude to triage and propose fixes. This transforms static analysis from a passive report into an active debugging workflow.

The toolchain constraint is strict: free, offline, works without MSVC lock-in, compatible with an eventual CMake migration. This rules out Visual Studio's paid Enterprise analysis tier and cloud-backed tools (Codacy, SonarCloud hosted tier, Synopsys Coverity cloud). MSVC `/analyze` is available today but is not the target direction.

## Existing Approaches

- **Clang-Tidy** — Modular lint framework built on Clang's AST; check categories include `bugprone-*`, `clang-analyzer-*`, `cppcoreguidelines-*`, `modernize-*`, `performance-*`. Outputs SARIF or plain text. Requires a `compile_commands.json` (easy with CMake, achievable with MSBuild via `clang-cl` or a shim).
- **Clang Static Analyzer (CSA)** — Deeper path-sensitive analysis baked into Clang; finds null-deref, use-after-free, dead stores, memory leaks. Invoked via `scan-build` or `clang --analyze`. Same compilation database requirement.
- **Cppcheck** — Standalone, does not need a compilation database, works directly on source files. Finds undefined behavior, out-of-bounds, uninitialized vars, resource leaks. Lower false-positive rate than Clang-Tidy on unfamiliar codebases; weaker type-system awareness.
- **PVS-Studio (free tier for open source)** — Commercial tool with a free license for open-source/student projects. Very low false-positive rate, deep inter-procedural analysis. Outputs SARIF. Requires license registration; partially offline.
- **MSVC `/analyze`** — Built into the Visual Studio compiler; enabled per-project. Windows-only, MSVC-only. Reasonable null/leak detection. Not direction-compatible with CMake migration.
- **SonarLint (IDE plugin)** — Free IDE plugin (VS Code, Visual Studio, JetBrains); performs local analysis. Requires internet for rule updates but works offline once synced. Superficial depth compared to Clang-Tidy.
- **Infer (Meta)** — Interprocedural analysis (bi-abduction). Designed for Java/C/C++/ObjC. Strong on resource and memory leaks. Build-system agnostic (wraps the compiler). Less mature for MSVC/Windows; best on GCC/Clang toolchains.
- **CodeChecker** — Open-source server + web UI that aggregates Clang-Tidy and CSA results, de-duplicates, tracks suppression and history. Runs fully offline. Acts as a finding database with a REST API — ideal for Claude integration.
- **Custom Clang plugins** — Write AST matchers for Cluiche-specific invariants (e.g. "all ProcessingUnit subclasses must call Super::Initialize"). High value, high cost.

## Design Axes

| Axis | Options | Notes |
|------|---------|-------|
| Analysis depth | Surface lint vs. path-sensitive vs. interprocedural | Deeper = more findings, more false positives, slower |
| Compilation database | Required (Clang) vs. optional (Cppcheck) vs. compiler-native (MSVC /analyze) | CMake emits `compile_commands.json` natively; MSBuild needs a shim today |
| Output format | Plain text vs. SARIF vs. XML vs. CodeChecker DB | SARIF is the standard Claude/LLM-friendly format |
| Invocation | CLI one-shot vs. IDE plugin vs. CI gate vs. `dia run` integration | All options are open |
| Claude integration | Manual paste vs. structured file feed vs. `dia diagnose` command | Structured feed enables automated triage loop |
| False-positive budget | Zero tolerance (block build) vs. suppression workflow vs. advisory only | Game engines accumulate cast patterns that trigger false positives |
| Build system coupling | MSVC-only vs. compiler-agnostic vs. CMake-native | Must not deepen MSVC dependency given migration intent |

## Known Tradeoffs

- **Clang-Tidy** has a high false-positive rate on new codebases if all checks are enabled; a curated `.clang-tidy` config file (suppressing `modernize-*` until ready) is essential.
- **Cppcheck** is fast and needs no compilation database but misses type-system bugs that require knowing which overload was selected.
- **Combining Clang-Tidy + Cppcheck** covers complementary failure modes: Cppcheck for flow bugs, Clang-Tidy for API misuse and guidelines.
- **CodeChecker** adds operational overhead (local server) but pays back by preventing finding churn — it tracks which findings are new vs. pre-existing.
- **SARIF output** from any tool can be read by VS Code's "SARIF Viewer" extension and piped to Claude; this is the cleanest integration seam.
- **CMake migration**: all Clang-based tools consume `compile_commands.json` natively. Adding CMake support even as an optional parallel build would unlock the full Clang ecosystem today, without waiting for full MSBuild removal.

## Known Pitfalls (C++ / game engine context)

- Enabling too many checks at once → hundreds of low-value warnings drown real bugs; start with `bugprone-*` and `clang-analyzer-*` only.
- Suppression sprawl: `// NOLINT` comments spread through the codebase defeat the purpose; use `.clang-tidy` allow-lists instead.
- C-style casts (common in engine code) trigger `cppcoreguidelines-pro-type-cstyle-cast` constantly — configure or disable.
- Clang-Tidy on MSVC headers (`<windows.h>`) produces enormous noise; must exclude system include paths.
- Thread-safety checks (`-Wthread-safety`) require `GUARDED_BY` annotations — valuable long-term but costly to retrofit.
- Processing unit lifecycle patterns (Initialize → Update → Shutdown) are invisible to generic tools; only a custom Clang plugin would catch violations.

## Cluiche-Specific Opportunities

### Relevant Existing Modules

| Module | Relevance |
|--------|-----------|
| DiaCore | Containers (`DynamicArrayC`, `HashTable`) have bounds-checked access — Cppcheck can verify out-of-bounds paths |
| DiaApplicationFlow | Phase/Module lifecycle order invariants — custom checks would be highest value |
| DiaInput | Event queue has multi-threaded access — thread-safety lint would catch unsynchronized reads |
| DiaBgfx | New module, C API bridging — null-pointer and resource-leak checks are high value here |
| DiaAPI | Plugin loading / CLI commands — API misuse checks apply |
| All modules | `StringCRC` constants (`kUniqueId`) should never be constructed from runtime strings in hot paths — a custom check could enforce this |

### Platform Decision Constraints

| Decision | Implication for this topic |
|----------|---------------------------|
| PD-004 No STL in public APIs | Clang-Tidy `modernize-use-ranges`, `readability-container-*` checks may incorrectly suggest STL — must suppress |
| PD-007 C++20 required | Clang-Tidy supports C++20; `compile_commands.json` must pass `-std=c++20` |
| PD-006 VS project files are source of truth (today) | Need a `compile_commands.json` shim (e.g. `compiledb`, `bear`, or `clang-cl` with CMake mirror) to use Clang tools now |
| PD-008 Directory.Build.props owns build settings | Any CMake mirror must replicate the same include paths and defines — not override them |
| PD-005 x64 Windows only | All tools must run on Windows x64; Clang/LLVM Windows binaries are available; Cppcheck has Windows builds |

## Open Questions for Ideation

- Should we invest in a `compile_commands.json` shim now, or wait for CMake migration? (Blocks full Clang-Tidy value)
- Is a CodeChecker local server worth the overhead for a solo/small team, or is a simpler SARIF file + VS Code viewer sufficient?
- What is the right integration point with `dia`? A new `dia check` command? A stage in `dia pipeline`?
- Should Claude triage automatically on every `dia check` run, or only when explicitly asked?
- Which check categories give the highest signal-to-noise for game engine C++ specifically? (`bugprone-*` + `clang-analyzer-*` seem safest to start)
- Is a lightweight CMake mirror of one module (e.g. DiaCore) a viable first step to validate the Clang toolchain without committing to full migration?
- How should suppressions be managed? Per-file `// NOLINT`, `.clang-tidy` config, or a central suppression DB?
