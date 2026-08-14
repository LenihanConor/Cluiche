# System Spec: DiaMesh3DVisualDebugger

## Parent Application
@docs/specs/applications/dia/dia.md

**Gameplay Domains:** debug

## Purpose

DiaMesh3DVisualDebugger is the `IDebugDomain` implementation that makes 3D mesh draw-call state visible in `DiaDebugPanel`. It wraps the `IVisualDebugger` draw classes from the DiaMesh3D system and surfaces bounds wireframes, origin markers, and live load-state stats to the panel.

**Critical gap:** `MeshStatsDrawer.Draw()` (SimPU) caches 11 stat fields each frame (draws, dropped, loaded, skinned, per-layer, asset states). `GetJSONState()` in `Mesh3DDebugDomain` does not currently read these fields — none of the stats reach the panel. This spec mandates wiring `GetJSONState()` to emit all cached fields. `DrawImGui()` was removed; cached stats are the intended replacement data source.

**Migration:** See `@docs/specs/applications/dia/systems/diadebugdomain/domain-migration.md`. Feature specs for the draw layer are at `mesh3d-bounds-and-origins.md` and `mesh3d-stats.md` in this directory.

## Domain Identity

| Property | Value |
|----------|-------|
| Domain ID | `"Mesh3D"` |
| Display name | `"Mesh3D"` |
| Description | `"3D mesh draw commands — bounds, origins, draw count, load state"` |
| Group | `"Rendering"` |
| Accent | `DebugGroupAccents::kRendering` (`#8b5cf6`) |
| `HasWorldDrawers()` | `true` |
| Drawers | BoundsWireframe, Origins, LoadStateOverlay |

## Panel Card Specification

Per `debugger-contract.md` AC-16 and `docs/research/visual_debugger_redesign/mockup.html`:

- **Group:** Rendering — accent `DebugGroupAccents::kRendering` (`#8b5cf6`) via `var(--accent)`
- **Spacing:** group header `padding: 5px 10px`, domain header `padding: 4px 8px`, domain body `padding: 6px 10px 8px 10px`, `margin-bottom: 3px` between cards, drawer rows `gap: 5px 10px`
- **Stat line:** `"Draws: N (D dropped)"` — `mCachedDroppedCount` highlighted in accent color when > 0
- **Expanded body:**
  - Drawer toggles: BoundsWireframe, Origins, LoadStateOverlay
  - Stats table: Draws · Dropped · Loaded · Skinned · Static · Ready · Pending · Failed · NotFound
  - Per-layer breakdown table: one row per non-empty layer (layer ID + draw count)
  - Scale slider

## JSON State Schema

`GetJSONState()` must emit all cached fields from `MeshStatsDrawer`. This is the normative C++↔panel contract:

```json
{
  "drawers": [
    { "name": "BoundsWireframe",  "enabled": false },
    { "name": "Origins",          "enabled": false },
    { "name": "LoadStateOverlay", "enabled": false }
  ],
  "stats": {
    "draws":     14,
    "dropped":    0,
    "loaded":    22,
    "skinned":    6,
    "static":     8,
    "ready":     22,
    "pending":    3,
    "failed":     0,
    "notFound":   1
  },
  "layers": [
    { "layer": 0, "draws": 8 },
    { "layer": 1, "draws": 4 },
    { "layer": 2, "draws": 2 }
  ]
}
```

**Data source:** `Mesh3DDebugDomain::GetJSONState()` must call into `MeshStatsDrawer` to read the cached fields. `MeshStatsDrawer` needs public accessors (or a `GetCachedStats(out)` method) so `Mesh3DDebugDomain` can read them without breaking encapsulation. `MeshStatsDrawer.Draw()` is already called each SimPU frame and caches this data; `GetJSONState()` just needs to read it.

## Domain ACs

In addition to all 16 ACs in `@docs/specs/applications/dia/systems/diadebugdomain/debugger-contract.md`:

| AC | Criterion |
|----|-----------|
| D-1 | `GetJSONState()` emits `stats.draws` = `mCachedDrawCount`; zero when no mesh draws this frame |
| D-2 | `GetJSONState()` emits `stats.dropped` = `mCachedDroppedCount`; panel highlights this value in accent color when > 0 |
| D-3 | `GetJSONState()` emits `stats.loaded`, `stats.skinned`, `stats.static` from the corresponding cached fields |
| D-4 | `GetJSONState()` emits `stats.ready`, `stats.pending`, `stats.failed`, `stats.notFound` from the asset-state cached fields |
| D-5 | `GetJSONState()` emits `layers[]` with one entry per `mCachedLayers[i]` where `count > 0`; at most 32 entries |
| D-6 | All stat fields populated from `MeshStatsDrawer`'s cached values — no additional `FrameData3D` access in `GetJSONState()` |
| D-7 | No `DrawImGui()` call anywhere in this module; all stats reach the panel exclusively via `GetJSONState()` |
| D-8 | Panel card layout complies with AC-16 spacing contract (see Panel Card Specification above) |

## Contract Compliance

Satisfies all 16 ACs from `@docs/specs/applications/dia/systems/diadebugdomain/debugger-contract.md`.

## Status

**Status:** Draft
