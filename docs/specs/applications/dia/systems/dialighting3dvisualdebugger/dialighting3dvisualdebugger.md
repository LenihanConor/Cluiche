# System Spec: DiaLighting3DVisualDebugger

## Parent Application
@docs/specs/applications/dia/dia.md

**Gameplay Domains:** debug

## Purpose

DiaLighting3DVisualDebugger is the `IDebugDomain` implementation that makes lighting state visible in `DiaDebugPanel`. It wraps the `IVisualDebugger` draw classes from the DiaLighting3D system and surfaces light positions, direction widgets, influence-range circles, and path arc previews as world-space overlays, plus a per-type light count in the panel.

**Migration:** See `@docs/specs/applications/dia/systems/diadebugdomain/domain-migration.md`. Feature specs for the draw layer are at `debug-widget-config.md` and `path-arc-preview.md` in this directory.

## Domain Identity

| Property | Value |
|----------|-------|
| Domain ID | `"Lighting3D"` |
| Display name | `"Lighting3D"` |
| Description | `"Light registry — position widgets, direction arrows, range circles, path arcs"` |
| Group | `"Rendering"` |
| Accent | `DebugGroupAccents::kRendering` (`#8b5cf6`) |
| `HasWorldDrawers()` | `true` |
| Drawers | PositionWidgets, DirectionArrows, LightRanges, PathArcs |

`LightRanges` is a new drawer (not in the original two-drawer set). It draws world-space influence circles/cones for point and spot lights.

## Panel Card Specification

Per `debugger-contract.md` AC-16 and `docs/research/visual_debugger_redesign/mockup.html`:

- **Group:** Rendering — accent `DebugGroupAccents::kRendering` (`#8b5cf6`) via `var(--accent)`
- **Spacing:** group header `padding: 5px 10px`, domain header `padding: 4px 8px`, domain body `padding: 6px 10px 8px 10px`, `margin-bottom: 3px` between cards, drawer rows `gap: 5px 10px`
- **Stat line:** `"Lights: N"` where N = total registered light count
- **Expanded body:**
  - Drawer toggles: PositionWidgets, DirectionArrows, LightRanges, PathArcs
  - Stats table: Directional · Point · Spot · Shadow-casting
  - Scale slider (affects widget gizmo size and range circle line weight)

## JSON State Schema

```json
{
  "drawers": [
    { "name": "PositionWidgets", "enabled": true  },
    { "name": "DirectionArrows", "enabled": true  },
    { "name": "LightRanges",     "enabled": false },
    { "name": "PathArcs",        "enabled": false }
  ],
  "stats": {
    "total":         6,
    "directional":   1,
    "point":         4,
    "spot":          1,
    "shadowCasting": 2
  }
}
```

## LightRangesDrawer Specification

`LightRangesDrawer` draws the world-space influence region for point and spot lights:

- **Point light** — a circle (XZ plane) of radius `light.range` at the light position, Rendering accent color at 50% opacity
- **Spot light** — two rays at ±half-angle from the spot direction, plus a closing arc at `light.range`
- **Directional light** — no shape (infinite range); skipped

`LightRangesDrawer` reads `LightRegistry3D` directly (same data source as `LightWidgetsDrawer`).

## Domain ACs

In addition to all 16 ACs in `@docs/specs/applications/dia/systems/diadebugdomain/debugger-contract.md`:

| AC | Criterion |
|----|-----------|
| D-1 | `GetJSONState()` emits `stats.total`, `stats.directional`, `stats.point`, `stats.spot` from `LightRegistry3D` |
| D-2 | `stats.shadowCasting` = count of lights with shadow casting enabled |
| D-3 | `LightRangesDrawer.Draw()` draws a circle at each point light position with radius = `light.range` |
| D-4 | `LightRangesDrawer.Draw()` draws a cone arc (2 rays + arc) for each spot light in its spot direction |
| D-5 | `LightRangesDrawer.Draw()` draws nothing for directional lights |
| D-6 | No `DrawImGui()` call anywhere in this module |
| D-7 | Panel card layout complies with AC-16 spacing contract (see Panel Card Specification above) |

## Contract Compliance

Satisfies all 16 ACs from `@docs/specs/applications/dia/systems/diadebugdomain/debugger-contract.md`.

## Status

**Status:** Draft
