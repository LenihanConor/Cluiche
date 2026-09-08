# Research: Ideate — Static C++ Bug Detection Tools

**Input:** docs/research/static_cpp_bug/explore.md

## Candidates

### Candidate 1: Cppcheck + `dia check` CLI command
**Home module/system:** DiaAPI (new `dia check` command / plugin)
**Size:** S
**Description:** Install Cppcheck (free, offline, no compilation database needed) and wire it into a new `dia check` command. The command runs Cppcheck against the Dia source tree, outputs findings to `Cluiche/out/check/cppcheck.xml`, and prints a summary. No build-system changes required — Cppcheck walks source files directly. A `.cppcheck` config file suppresses known false-positive patterns (C-style casts, system headers).
**Primary value:** Zero-friction entry point — works today with MSBuild, no compilation database, findings available in minutes.

---

### Candidate 2: Clang-Tidy via CMake mirror of DiaCore
**Home module/system:** New `CMakeLists.txt` for DiaCore + `.clang-tidy` config
**Size:** M
**Description:** Create a thin CMake mirror for DiaCore only (the most foundational module). CMake emits `compile_commands.json` natively; Clang-Tidy consumes it. Run `clang-tidy` on DiaCore sources with a curated check set (`bugprone-*`, `clang-analyzer-*`). The MSBuild `.vcxproj` remains authoritative for the actual build — the CMake file is analysis-only. This validates the Clang toolchain path without committing to a full migration, and produces SARIF output.
**Primary value:** Unlocks the full Clang AST analysis ecosystem for the most-depended-on module, with a low-risk CMake footprint.

---

### Candidate 3: Clang-Tidy full project via `compile_commands.json` shim
**Home module/system:** DiaAPI new `dia check` command + `compiledb` / `bear` shim
**Size:** M
**Description:** Use a tool like `compiledb` or a custom MSBuild logger to generate `compile_commands.json` from the existing `.vcxproj` files. Wire Clang-Tidy across all Dia modules into `dia check --tool=clang-tidy`. Curate a `.clang-tidy` config that enables `bugprone-*`, `clang-analyzer-*`, `cppcoreguidelines-*` (with STL/modernize checks disabled to respect PD-004). Outputs SARIF to `Cluiche/out/check/`.
**Primary value:** Full Clang-Tidy coverage of all modules without waiting for CMake migration; SARIF output ready for Claude integration.

---

### Candidate 4: `dia diagnose` — Claude-powered triage loop
**Home module/system:** DiaAPI new `dia diagnose` command
**Size:** M
**Description:** A `dia diagnose` command that reads a SARIF or XML findings file (produced by any of the above tools) and invokes Claude via the Anthropic SDK. Claude receives the finding (file, line, check name, message, surrounding code context) and responds with: root cause explanation, fix suggestion, and severity classification. Output is written to `Cluiche/out/check/diagnose.md`. The command can be run after any `dia check` invocation. Supports `--finding=<id>` to triage a single finding.
**Primary value:** Closes the static-analysis loop — findings become actionable fix proposals, not a passive report to ignore.

---

### Candidate 5: Cppcheck + Clang-Tidy combined pipeline with SARIF merge
**Home module/system:** DiaAPI `dia check` command (extended)
**Size:** M
**Description:** Run both Cppcheck and Clang-Tidy (once compilation database exists), merge their SARIF outputs into a single `findings.sarif`, deduplicate by location, and write a markdown summary. A VS Code SARIF Viewer extension renders findings inline. This is Candidates 1 + 3 composed into a single `dia check` command with a `--tool=all` flag. Suppression is managed centrally via `.clang-tidy` and a `cppcheck-suppressions.xml`.
**Primary value:** Best combined signal-to-noise by using Cppcheck's flow analysis alongside Clang-Tidy's type-aware checks; single output seam for Claude integration.

---

### Candidate 6: Custom Clang AST plugin — Dia lifecycle invariant checker
**Home module/system:** New `DiaCluicheChecks` Clang plugin (lives in `Tools/`)
**Size:** L
**Description:** A custom Clang plugin (loaded via `-fplugin`) that encodes Cluiche-specific invariants as AST matchers: "every class inheriting `IModule` must call `Super::Initialize()`", "no `new`/`delete` in game code", "all `StringCRC` constants must be `constexpr`". The plugin is built with LLVM's CMake infrastructure and loaded during a dedicated analysis build. Findings output as diagnostics (same format as Clang warnings).
**Primary value:** The only approach that catches engine-specific lifecycle bugs that generic tools are blind to — highest long-term value, highest build cost.

---

### Candidate 7: SonarLint local IDE integration (VS Code + Visual Studio)
**Home module/system:** Developer environment (no DiaAPI change)
**Size:** S
**Description:** Install SonarLint as a VS Code and/or Visual Studio extension. Configure it to run in "connected mode" against a local SonarQube instance, or in standalone mode (no server required). Findings appear inline in the editor as you type. No CLI integration, no `dia` command change. Works offline after initial rule sync. Complements but does not replace a CLI-based pipeline check.
**Primary value:** Immediate developer feedback loop in the editor with zero build-system changes; lowest friction for catching issues during authoring.

---

### Candidate 8: CodeChecker local server — findings database and suppression tracker
**Home module/system:** Infrastructure (`Tools/` + Docker or native install)
**Size:** L
**Description:** Run a CodeChecker server locally (native Python install or Docker). CodeChecker wraps Clang-Tidy and CSA, stores findings in a SQLite database, tracks which findings are new vs. pre-existing, manages suppressions, and exposes a REST API. `dia check` posts findings to CodeChecker; a `dia diagnose` command queries new findings via the REST API and feeds them to Claude. The web UI shows finding history across commits.
**Primary value:** Prevents finding debt accumulation — only new findings surface per run; suppression is centrally auditable rather than scattered `// NOLINT` comments.

---

### Candidate 9: `dia check --ci` gate — fail the pipeline on new high-severity findings
**Home module/system:** DiaAPI `dia pipeline` extension
**Size:** S (once a check tool exists)
**Description:** Extend `dia pipeline` with an optional `static-analysis` stage that runs `dia check`, compares findings against a baseline (the last clean run's SARIF file stored in `Cluiche/out/check/baseline.sarif`), and fails the pipeline if any new `error`-severity findings are introduced. The baseline is updated manually via `dia check --accept-baseline`. This is a policy layer, not a new tool — it wraps whichever tool is chosen in Candidates 1–5.
**Primary value:** Makes static analysis a quality gate rather than advisory noise; new bugs cannot silently accumulate.

---

### Candidate 10: Sanitizer build configurations (ASan + UBSan)
**Home module/system:** DiaAPI `dia run` command + new build configurations
**Size:** S
**Description:** Add `Debug-Asan` and `Debug-Ubsan` build configurations to the Visual Studio solution (or CMake when ready). Both are supported by MSVC (ASan since VS 2019) and Clang-cl on Windows. Wire them into `dia run googletest --config Asan` so the full GoogleTest suite runs under instrumentation. Findings print to stderr as structured diagnostics. A `dia check --sanitizer` flag captures output and converts it to a findings report alongside static analysis results.
**Primary value:** Zero false positives — every ASan/UBSan report is a real bug on a real executed path; complements static analysis which reasons without running.

---

### Candidate 11: WSL2 / Linux CI target for TSan + MSan
**Home module/system:** Infrastructure (`Tools/` + WSL2 environment)
**Size:** M
**Description:** TSan and MSan do not run on Windows. Add a WSL2 build target using CMake + Clang that compiles a subset of Dia (DiaCore, DiaInput, DiaApplicationFlow) and runs GoogleTests under TSan. This is the only reliable way to catch data races in the Main/Render/Sim ProcessingUnit threading model. The WSL2 target runs independently of the Windows build; `dia check --tsan` invokes it. Requires WSL2 installed (free, offline once set up) and a CMake build for the targeted modules.
**Primary value:** TSan is the only tool that reliably catches real data races — no static tool can substitute; directly addresses the multi-threaded phase/module architecture's highest-risk bug class.

---

## Coverage Map

The candidates span the full design-axis range from explore.md:

- **Depth**: S1 (Cppcheck, surface flow) → C2/C3/C5 (Clang-Tidy, type-aware) → C6 (custom plugin, engine-specific)
- **Compilation database**: C1/C7/C8 need none today → C2 introduces CMake for one module → C3/C5 via shim → C6 requires LLVM CMake
- **Output format**: C1 (XML/plain) → C2/C3/C5 (SARIF) → C8 (CodeChecker DB + REST)
- **Claude integration**: C4 is the dedicated loop-closer; C9 is the gate enforcer
- **Build system coupling**: C1/C7 touch nothing → C2 adds optional CMake mirror → C3 adds shim → C6 adds LLVM build
- **Size range**: S (C1, C7, C9, C10) → M (C2, C3, C4, C5, C11) → L (C6, C8)
- **Runtime vs. static**: C1–C9 are static (no execution needed) → C10/C11 are runtime (need a passing test suite to exercise paths)
- **Windows vs. cross-platform**: C1/C2/C3/C7/C9/C10 run on Windows today → C11 requires WSL2; C6/C8 require LLVM build
