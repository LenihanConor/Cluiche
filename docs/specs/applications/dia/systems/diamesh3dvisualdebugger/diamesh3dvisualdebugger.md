# System Spec: DiaMesh3DVisualDebugger

## Parent Application
@docs/specs/applications/dia/dia.md

**Gameplay Domains:** debug

## Purpose

DiaMesh3DVisualDebugger is the `IDebugDomain` implementation that makes 3D mesh draw-call state visible in `DiaDebugPanel`. It wraps the `IVisualDebugger` draw classes from the DiaMesh3D system and surfaces bounds wireframes, origin markers, and load-state stats to the panel.

**Migration:** Specified in the DiaDebugDomain domain migration feature at `@docs/specs/applications/dia/systems/diadebugdomain/domain-migration.md`. Feature specs for the underlying draw layer are at `mesh3d-bounds-and-origins.md` and `mesh3d-stats.md` in this directory.

## Domain Identity

| Property | Value |
|----------|-------|
| Domain ID | `"mesh3d"` |
| Display name | `"Mesh3D"` |
| Description | `"3D mesh draw commands — bounds wireframe, origins, load-state stats"` |
| Group | `"Rendering"` |
| Accent | `DebugGroupAccents::kRendering` (`#8b5cf6`) |
| `HasWorldDrawers()` | `true` |
| Drawers | BoundsWireframe, Origins, LoadStateOverlay |

## Panel Card Specification

Per `debugger-contract.md` AC-16 and `docs/research/visual_debugger_redesign/mockup.html`:

- **Group:** Rendering — accent `DebugGroupAccents::kRendering` (`#8b5cf6`) via `var(--accent)`
- **Domain stat line:** `"Draws: N"` where N = draw command count this frame
- **Expanded body:** drawer toggles (BoundsWireframe, Origins, LoadStateOverlay), stats row (Loaded, Streaming, Failed counts)
- Spacing complies with AC-16 (outer padding/margin not overridden by domain-specific content)

## Contract Compliance

Satisfies all 16 ACs from `@docs/specs/applications/dia/systems/diadebugdomain/debugger-contract.md`.

## Status

**Status:** Draft
