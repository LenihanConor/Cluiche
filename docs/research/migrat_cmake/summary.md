# Research Summary — CMake Migration & Dia Architecture

**Session folder:** docs/research/migrat_cmake/
**Date:** 2026-05-25

## One-Line Answer

Adopt a domain-oriented architecture with a structured Core base, enforced at build time via CMake `target_link_libraries` — replacing the current flat 55-module MSBuild graph with an auditable, layered structure.

## Journey

1. **Explored:** The flat MSBuild layout has no build-time enforcement of the dependency rules already documented in YAML; CMake's `target_link_libraries` is the lever that turns those rules into compile errors. Secondary payoffs (Clang-Tidy, TSan) follow as consequences.
2. **Ideated:** 7 candidates generated — ranging from a Python audit tool (no build change) through Foundation CMake pilot, layered INTERFACE model, full cutover, and a YAML generator. A natural sequencing emerged: C7 → C1 → C4 → C2 → C3.
3. **Evaluated:** YAML layer formalisation (C7, 3.85) and Foundation CMake pilot (C2, 3.80) scored highest. Full cutover (C5) and dual-track (C6) scored lowest due to cost and ongoing maintenance burden.
4. **Chose:** Option D — domain-oriented architecture with Core infrastructure base, refined through iterative discussion to a 6-sub-layer Core + 4 named domains.

## Chosen Work Item

**Name:** DiaArchitecture system — domain-oriented module structure + CMake enforcement
**Home module:** New `DiaArchitecture` system spec (meta — governs all modules)
**Suggested spec type:** System (with 4 child feature specs: C7, C1, C2, C3)
**Estimated size:** M overall (C7 + C1 = S each; C2 = S/M; C3 = L)

## Target Architecture

### Core (6 sub-layers, strict bottom-up ordering)

```
Foundation:   DiaCore, DiaMaths, DiaGeometry2D, DiaGeometry3D,
              DiaSerializer, DiaObservation
Platform:     DiaWindow, DiaInput, DiaThreading, DiaMailbox
Application:  DiaApplicationFlow, DiaStateMachine
Entity:       diaentitytemplate
Assets:       DiaAsset, DiaAssetCatalogue, DiaAssetRuntime
Tooling:      DiaEditor, DiaAPI, DiaAutomation, DiaWebSocket,
              DiaDebugServer, DiaDebugProtocol,
              DiaVisualDebugger (base/console)
```

### Domains (vertical slices — core + tools per domain)

```
Physics/    DiaRigidBody2D, DiaSoftBody2D
            + DiaRigidBody2DVisualDebugger, DiaSoftBody2DVisualDebugger

Animation/  DiaRig2D, DiaIK2D, DiaAnimation2D,
            DiaMesh3D, DiaRig3D, DiaAnimation3D, DiaSkinning3D
            + DiaRig2DVisualDebugger, DiaIK2DVisualDebugger,
              DiaAnimation2DVisualDebugger

Rendering/  DiaGraphics, DiaGraphics3D, DiaUI, DiaScene3D
            + DiaBgfx, DiaBgfx3D, DiaSFML, DiaUICEF,
              DiaUIUltralight, DiaImGui

AI/         DiaAI, DiaPathfinding  (future)
            + DiaAIVisualDebugger  (future)
```

### Hard rules
- Core sub-layers: strict bottom-up only — no upward reach
- Domain cores: depend on Core only — never on other domain cores
- Domain tools: may depend on Core/Tooling + own domain core

## Key Insights from Exploration

- The flat MSBuild layout has no compile-time enforcement — YAML `dependencies.forbidden` is documentary only today; violations are invisible until code review
- CMake's primary value here is **architecture enforcement**, not build speed or cross-platform support
- DiaObservation belongs in Foundation (not Tooling) because `DIA_LOG_*` macros must be available to every layer including Foundation itself
- DiaSerializer belongs in Foundation — it is used by configs, manifests, and assets, all of which live above it
- Domain ownership of visual debuggers improves discoverability and scales cleanly to new domains (AI adds its own debugger without touching global Tooling)
- An audit tool (C1) must run before C2 to surface silent forbidden-dep violations in the current codebase — migrating to layered CMake without fixing violations first will fail to build
- PD-006 ("VS project files are source of truth") must be updated as part of C3

## Implementation Sequence

| Step | Candidate | Size | Deliverable |
|------|-----------|------|-------------|
| 1 | C7 — YAML layer formalisation | S | `layer:` field on all module docs; architecture is documented |
| 2 | C1 — Architecture audit tool | S | `dia check --tool=arch`; current violations known |
| 3 | C2 — Foundation CMake pilot | S/M | CMakeLists.txt for Foundation sub-layer; `compile_commands.json` working |
| 4 | C3 — Layered INTERFACE model | L | Full CMake enforcement; PD-006 updated; `dia run` switches to cmake |

## Discarded Candidates

| Candidate | Why discarded |
|-----------|--------------|
| Option A (5 wide-middle layers) | No sim/rendering separation; physics could reach rendering |
| Option B (6 sim/anim split) | AI awkwardly placed; visual debuggers still in global Tooling |
| C5 (full cutover / Open Folder) | Maximum disruption, no incremental value; C3 delivers enforcement without retiring .sln |
| C6 (dual-track analysis overlay) | Same Clang-Tidy benefit as C2 with ongoing dual-maintenance cost |

## References

- docs/research/migrat_cmake/explore.md
- docs/research/migrat_cmake/ideate.md
- docs/research/migrat_cmake/evaluate.md
- docs/research/migrat_cmake/choose.md
