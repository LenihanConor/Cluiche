# System Spec: DiaLighting3DVisualDebugger

## Parent Application
@docs/specs/applications/dia/dia.md

**Gameplay Domains:** debug

## Purpose

DiaLighting3DVisualDebugger is the `IDebugDomain` implementation that makes lighting state visible in `DiaDebugPanel`. It wraps the `IVisualDebugger` draw classes from the DiaLighting3D system and surfaces light positions, direction widgets, and path arc previews to the panel.

**Migration:** Specified in the DiaDebugDomain domain migration feature at `@docs/specs/applications/dia/systems/diadebugdomain/domain-migration.md`. Feature specs for the underlying draw layer are at `debug-widget-config.md` and `path-arc-preview.md` in this directory.

## Domain Identity

| Property | Value |
|----------|-------|
| Domain ID | `"lighting3d"` |
| Display name | `"Lighting3D"` |
| Description | `"Light registry — position widgets, direction arrows, path arcs"` |
| Group | `"Rendering"` |
| Accent | `DebugGroupAccents::kRendering` (`#8b5cf6`) |
| `HasWorldDrawers()` | `true` |
| Drawers | PositionWidgets, DirectionArrows, PathArcs |

## Panel Card Specification

Per `debugger-contract.md` AC-16 and `docs/research/visual_debugger_redesign/mockup.html`:

- **Group:** Rendering — accent `DebugGroupAccents::kRendering` (`#8b5cf6`) via `var(--accent)`
- **Domain stat line:** `"Lights: N"` where N = registered light count
- **Expanded body:** drawer toggles (PositionWidgets, DirectionArrows, PathArcs), stats row (Directional, Point, Spot counts)
- Spacing complies with AC-16 (outer padding/margin not overridden by domain-specific content)

## Contract Compliance

Satisfies all 16 ACs from `@docs/specs/applications/dia/systems/diadebugdomain/debugger-contract.md`.

## Status

**Status:** Draft
