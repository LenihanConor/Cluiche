# Research: Choice — CMake Migration & Dia Architecture

**Date:** 2026-05-25
**Chosen candidate:** Option D — Domain-Oriented with Core infrastructure base (refined)

## Rationale

The user confirmed Option D as the target architecture after iterating through four options. The domain model is preferred over a pure horizontal layer stack because visual debuggers and editor plugins belong *with* their domain, not in a global tooling bucket. New domains (AI, Pathfinding) can be added as self-contained vertical slices without restructuring the existing layer model.

The Core infrastructure was refined through discussion into six named sub-layers with explicit placement decisions for contested modules.

## Final Architecture

### Core (horizontal, shared by all domains)

| Sub-layer | Modules |
|-----------|---------|
| Foundation | DiaCore, DiaMaths, DiaGeometry2D, DiaGeometry3D, DiaSerializer, DiaObservation |
| Platform | DiaWindow, DiaInput, DiaThreading, DiaMailbox |
| Application | DiaApplicationFlow, DiaStateMachine |
| Entity | DiaEntity |
| Assets | DiaAsset, DiaAssetCatalogue, DiaAssetRuntime |
| Tooling | DiaEditor, DiaAPI, DiaAutomation, DiaWebSocket, DiaDebugServer, DiaDebugProtocol, DiaVisualDebugger (base/console) |

### Domains (vertical slices)

Each domain owns a `core/` tier and a `tools/` tier. Domain tools may depend on Core/Tooling + their own domain core. Domain cores may not depend on other domains.

| Domain | Core modules | Tools modules |
|--------|-------------|---------------|
| Physics | DiaRigidBody2D, DiaSoftBody2D | DiaRigidBody2DVisualDebugger, DiaSoftBody2DVisualDebugger |
| Animation | DiaRig2D, DiaIK2D, DiaAnimation2D, DiaMesh3D, DiaRig3D, DiaAnimation3D, DiaSkinning3D | DiaRig2DVisualDebugger, DiaIK2DVisualDebugger, DiaAnimation2DVisualDebugger |
| Rendering | DiaGraphics, DiaGraphics3D, DiaUI, DiaScene3D | DiaBgfx, DiaBgfx3D, DiaSFML, DiaUICEF, DiaUIUltralight, DiaImGui + visual debuggers |
| AI | DiaAI, DiaPathfinding (future) | DiaAIVisualDebugger (future) |

### Hard rules
- Core sub-layers are strictly ordered bottom-up: Foundation → Platform → Application → Entity/Assets (parallel) → Tooling
- Domain cores may only depend on Core sub-layers, not on other domain cores
- Domain tools may depend on Core/Tooling + their own domain core
- No upward reach within Core

## What Was Ruled Out

| Option | Reason not chosen |
|--------|------------------|
| Original 7 horizontal layers | Visual debuggers in global Tooling — poor discoverability; doesn't scale to new domains |
| Option A — 5 wide-middle | No sim/rendering separation; physics could accidentally depend on rendering |
| Option B — 6 sim/anim split | AI/Pathfinding awkwardly placed in Presentation; visual debuggers still global |

## Key Placement Decisions

| Module | Decision | Reason |
|--------|----------|--------|
| DiaSerializer | Foundation | General-purpose; needed below Assets and by configs/manifests |
| DiaObservation | Foundation | Only depends on DiaCore; `DIA_LOG_*` must be available to all layers |
| DiaStateMachine | Application | Used by application flow and entities; not entity-specific |
| DiaEntity | Entity (own sub-layer) | Separated from Application — lifecycle wiring ≠ object model |
| Domain visual debuggers | Domain tools tier | Belong with their domain, not in global Tooling |

## Pre-Spec Commitments

- PD-006 must be updated: CMake becomes the source of truth (replacing VS project files)
- The migration sequence is C7 → C1 → C2 → C3: YAML layer formalisation → audit tool → Foundation CMake pilot → layered INTERFACE model
- `.vcxproj` files remain usable during migration; CMake is additive until C3 completes
- `dia run` continues calling MSBuild until C3 is proven; then switches to `cmake --build`

## Next Step

Run `/spec-system` for a new `DiaArchitecture` system covering:
- Feature C7: Add `layer:` field to all module docs
- Feature C1: `dia check --tool=arch` Python audit tool
- Feature C2: Foundation CMake pilot (DiaCore + DiaMaths + DiaGeometry2D + DiaGeometry3D + DiaSerializer + DiaObservation)
- Feature C3: Layered CMake INTERFACE aggregates (full enforcement)
