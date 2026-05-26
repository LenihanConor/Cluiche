# Research: Ideate — CMake Migration & Dia Architecture

**Input:** docs/research/migrat_cmake/explore.md

## Candidates

### Candidate 1: Architecture Audit Tool (Python include-graph enforcer)
**Home module/system:** DiaCLI / `dia check --tool=arch`
**Size:** S
**Description:** A Python script (runnable as `dia check --tool=arch`) that reads every `dia.*.architecture.module.md` YAML file, collects `dependencies.forbidden`, then parses all `#include` directives across the corresponding source files and reports violations. No CMake needed. Runs on Windows today with zero build-system change. CI can gate on it.

This is the minimum viable architecture enforcement tool. It doesn't prevent future violations at compile time, but it makes the current state auditable and makes regressions visible in PRs. It also produces the violation report you need before any CMake migration — you can't migrate to layered CMake without first knowing which silent violations exist.

**Primary value:** Turns the documented-but-unenforced dependency rules into a CI gate with no build system change. Acts as the prerequisite audit for any deeper restructure.

---

### Candidate 2: Foundation Layer CMake Pilot (DiaCore + DiaMaths + DiaGeometry2D)
**Home module/system:** Dia/DiaCore, Dia/DiaMaths, Dia/DiaGeometry2D
**Size:** M
**Description:** Write `CMakeLists.txt` for the three leaf-level Foundation modules — the ones with zero Dia dependencies. These are the safest starting point: no forbidden-dep risk, no external SDK complexity, and they form the base everything else links against. A `CMakePresets.json` with two presets (VS generator + Ninja) is written at the repo root. The three modules build cleanly under both; GoogleTests tests for DiaCore still pass.

The `.vcxproj` files remain and continue to be the primary dev workflow. The CMake files are additive. This pilot proves the pattern, gets `compile_commands.json` working for the most important modules (DiaCore is the highest-value Clang-Tidy target), and identifies the real friction before committing to a full migration.

**Primary value:** Unlocks Clang-Tidy on DiaCore/DiaMaths/DiaGeometry2D immediately; proves the CMake pattern at low risk; leaves the VS workflow untouched.

---

### Candidate 3: Layered CMake INTERFACE aggregates (full architecture model)
**Home module/system:** Repo root CMakeLists.txt
**Size:** L
**Description:** Define the full 7-layer architecture as CMake `INTERFACE` library targets — `Dia::Foundation`, `Dia::PlatformPrimitives`, `Dia::EngineServices`, `Dia::Simulation`, `Dia::Rendering`, `Dia::Tooling`, `Dia::VisualDebuggers`. Each layer aggregates its member modules and declares `target_link_libraries` dependencies only on lower layers. Upstream modules that `link against Dia::Foundation` cannot accidentally reach `Dia::Tooling` headers — the compiler enforces it.

This is the high-value architecture deliverable. It requires completing the Foundation pilot first (C2), and fixing any violations the audit tool (C1) surfaces. It does not require migrating all 55 modules simultaneously — layers can be added top-down or bottom-up. The VS solution can still be generated from CMake. `dia run` is updated to call `cmake --build` instead of `msbuild`.

**Primary value:** Makes the 7-layer architecture structurally enforced at compile time, not just documented. Every new module addition is a diff against a `CMakeLists.txt` — reviewable, auditable, mergeable.

---

### Candidate 4: YAML → CMakeLists.txt generator
**Home module/system:** DiaCLI / `dia generate cmake`
**Size:** S
**Description:** A Python script that reads all `dia.*.architecture.module.md` files and the corresponding `.vcxproj` files, then writes draft `CMakeLists.txt` stubs. The generated stubs include: `add_library`, source file list (from `.vcxproj` `<ClCompile>` items), `target_include_directories`, and `target_link_libraries` (from YAML `dependencies.required`). The output is a starting point, not production-ready — but it eliminates the mechanical scaffolding work for 55 modules.

The generator doesn't need to be perfect. If it gets 80% right and flags the remaining 20% with TODOs, it still saves days of manual work during the full migration.

**Primary value:** Cuts the mechanical cost of a full migration from weeks to days by bootstrapping CMake files from the existing YAML module metadata.

---

### Candidate 5: Full CMake migration with VS Open Folder
**Home module/system:** Entire repo
**Size:** XL
**Description:** Replace all 55 `.vcxproj` files with `CMakeLists.txt`. The `.sln` is retired. Developers use VS 2022 "Open Folder" mode, which has native CMake support (CMake Tools extension handles configure/build/debug). Ninja is the default generator for fast builds and `compile_commands.json`. A VS generator preset is available for developers who need the traditional solution view.

This is the full-value, full-cost option. `Directory.Build.props` is deleted; its settings live in `CMakePresets.json`. PD-006 is updated. `dia run` switches to `cmake --build`. All external deps are either `add_subdirectory`'d (googletest, protobuf) or wrapped in `find_package` modules (SFML, CEF, Ultralight). bgfx/bx/bimg remain built separately in `.diaenv/` as today.

**Primary value:** Complete solution — architecture enforcement, Clang-Tidy, TSan-ready, Open Folder IDE, Ninja speed. Maximum long-term gain; maximum short-term disruption.

---

### Candidate 6: Dual-track — MSBuild primary + CMake for analysis only
**Home module/system:** Repo root (parallel CMake tree)
**Size:** M
**Description:** A parallel `cmake/` directory at the repo root containing `CMakeLists.txt` files that mirror the `.vcxproj` structure. MSBuild remains the primary build and daily development path — nothing changes for `dia run`. The CMake tree exists only to generate `compile_commands.json` via Ninja, used by `dia check --tool=clang-tidy` and clangd. A CI job keeps the two trees in sync by checking that the CMake Ninja build passes on every PR.

The maintenance burden of keeping two trees in sync is the main cost. Adding a `.cpp` file means updating both `.vcxproj` and `CMakeLists.txt`. This is manageable short-term but compounds over time.

**Primary value:** Zero disruption to the VS developer workflow; unlocks Clang-Tidy analysis immediately at the cost of ongoing dual-maintenance.

---

### Candidate 7: Formal layer definition in YAML only (no CMake)
**Home module/system:** Module registry / documentation
**Size:** S
**Description:** Add a `layer` field to every `dia.*.architecture.module.md` (values: `foundation`, `platform_primitives`, `engine_services`, `simulation`, `rendering_integration`, `tooling`, `editor_plugins`). Extend the Python architecture audit tool (C1) to enforce layer ordering — a lower-layer module cannot depend on a higher-layer module. The build system is untouched; enforcement is at the CI level via the audit script.

This is the cheapest way to formalise the architecture and create an auditable record of which layer each module belongs to. It doesn't give compile-time enforcement, but it documents the intended architecture clearly and blocks regressions in CI. It also pre-populates the metadata that the CMake generator (C4) would need to produce the layered INTERFACE targets (C3).

**Primary value:** Cheap formalisation of the layer model; produces the metadata that all CMake-based candidates need; creates an auditable CI gate today without touching the build system.

---

## Coverage Map

The candidates span the full scope and cost range:
- **S / no build change:** C1 (audit tool), C4 (generator), C7 (YAML layer formalisation)
- **M / additive CMake:** C2 (foundation pilot), C6 (dual-track analysis overlay)
- **L / structural CMake:** C3 (layered INTERFACE model — full architecture enforcement)
- **XL / full cutover:** C5 (VS Open Folder, retire all `.vcxproj`)

The design axes are covered: C7 addresses architecture model without CMake; C1 covers enforcement without build change; C2+C3 cover incremental CMake adoption; C5 covers full cutover; C6 covers tooling-only CMake; C4 is the accelerator for any CMake candidate.

A natural sequencing emerges: **C7 → C1 → C4 → C2 → C3 → (optionally C5)**. Each step builds on the previous and all are independently shippable.
