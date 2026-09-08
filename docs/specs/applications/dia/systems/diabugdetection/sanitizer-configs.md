# Feature Spec: sanitizer-configs

**Status:** Approved  
**Parent System:** @docs/specs/applications/dia/systems/diabugdetection/diabugdetection.md  
**Research:** docs/research/static_cpp_bug/summary.md

---

## Summary

Add `Debug-Asan` and `Debug-Ubsan` build configurations to the Cluiche solution so GoogleTests (and its Dia library dependencies) can be compiled and run under MSVC AddressSanitizer and Clang-cl UndefinedBehaviorSanitizer. Wire `dia run googletest --config Asan` and `--config Ubsan` into the DiaCLI run command. All runs are user-invoked — nothing is added to the default hot path.

## Problem

There is no way to run the engine under memory or undefined-behaviour instrumentation. Real bugs (use-after-free, stack corruption, signed overflow) that occur on exercised paths go undetected until they manifest as intermittent crashes.

## Acceptance Criteria

- [ ] `Debug-Asan` solution configuration exists in `Cluiche.sln` and `Directory.Build.props`; compiles GoogleTests + Dia library deps cleanly under MSVC `/fsanitize=address`
- [ ] `Debug-Ubsan` solution configuration exists; compiles GoogleTests + Dia library deps cleanly under Clang-cl with `-fsanitize=undefined`
- [ ] `dia run googletest --config Asan` builds the Asan configuration and runs `GoogleTests.exe`; sanitizer findings print to stderr
- [ ] `dia run googletest --config Ubsan` does the same for UBSan
- [ ] `dia check --tool=sanitizer --config=Asan` and `--config=Ubsan` run the binary and capture sanitizer stderr output into `Cluiche/out/check/sanitizer-asan.txt` and `sanitizer-ubsan.txt`
- [ ] Neither config is added to the default `dia run googletest` path — normal iteration is unaffected
- [ ] CluicheTest and CluicheEditor are excluded from sanitizer configs (GoogleTests + Dia libs only)
- [ ] `dia env verify` reports whether MSVC ASan runtime and LLVM/Clang-cl are available

## Tasks

| # | Task | Notes |
|---|------|-------|
| 1 | Add `Debug-Asan` configuration to `Directory.Build.props` | `/fsanitize=address` conditional on `$(Configuration)==Debug-Asan`; inherits all PD-008 settings |
| 2 | Add `Debug-Ubsan` configuration to `Directory.Build.props` | Clang-cl toolset; `-fsanitize=undefined`; conditional on `$(Configuration)==Debug-Ubsan` |
| 3 | Register both configs in `Cluiche.sln` for GoogleTests project + Dia lib deps only | Other projects map to `Debug\|x64` passthrough |
| 4 | Extend DiaCLI `run` command to accept `--config Asan\|Ubsan` | Passes config name through to MSBuild call |
| 5 | Add `dia check --tool=sanitizer` sub-command | Builds then runs; captures stderr to `out/check/sanitizer-<config>.txt` |
| 6 | Extend `dia env verify` with ASan runtime + LLVM presence checks | Warn if missing; do not block |

## Traceability

| Level | Spec |
|-------|------|
| Platform | @docs/specs/platform/Cluiche.md |
| Application | @docs/specs/applications/dia/dia.md |
| System | @docs/specs/applications/dia/systems/diabugdetection/diabugdetection.md |

## Binding Decisions Compliance

| Decision | Plain language | Compliance |
|----------|---------------|------------|
| PD-001 StringCRC | Identifiers use StringCRC, not raw strings | Compliant — no new runtime identifiers introduced; `dia check` command name registered via StringCRC in DiaAPI |
| PD-004 No STL in public APIs | DiaCore containers only in public C++ APIs | Compliant — this feature adds build configs and Python CLI only; no new C++ public API |
| PD-005 x64 Windows only | All builds target x64 Windows | Compliant — both configs set `/MACHINE:X64`; Clang-cl targets `x86_64-pc-windows-msvc` |
| PD-006 VS project files are source of truth | MSBuild is the build system | Compliant — configs added via `Directory.Build.props` and `Cluiche.sln`; no per-project overrides |
| PD-007 C++20 required | All projects compile under `/std:c++20` | Compliant — sanitizer configs inherit `LanguageStandard` from `Directory.Build.props` (PD-008) |
| PD-008 Directory.Build.props owns build settings | No per-project overrides for OutDir/IntDir/toolchain | Compliant — sanitizer flags added as `Directory.Build.props` conditionals; no `.vcxproj` overrides |
| PD-009 Generated output under `Cluiche/out/` | Non-binary output lives under `Cluiche/out/<AppName>/` | Compliant — sanitizer capture files go to `Cluiche/out/check/` |
| BD-001 API key via shell env | No in-repo key storage | N/A — no Claude invocation in this feature |
| BD-003 Abort on dirty working tree | `dia diagnose` aborts if dirty | N/A — `dia check` does not modify source files |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Sanitizer configs | Should `Debug-Asan` use MSVC ASan only, or also Clang-cl for parity? | MSVC ASan for `Debug-Asan`; Clang-cl for `Debug-Ubsan` (MSVC has no UBSan) |
| 2 | Scope | Which Dia library projects are included in the sanitizer configs? | All Dia static libs that GoogleTests links against; CluicheTest and CluicheEditor excluded |
| 3 | Output | Should sanitizer findings be converted to SARIF in this feature, or raw text only? | Raw text only in this feature; SARIF conversion is owned by `dia-diagnose-loop` which reads the raw output |
| 4 | Iteration impact | Does `dia run googletest` (no --config flag) change at all? | No — default config remains `Debug`; sanitizer configs are opt-in only |
| 5 | DiaEnv | Should `dia env setup` auto-install LLVM for Clang-cl UBSan? | No — `dia env verify` warns if missing; manual install via winget; UBSan is optional |
