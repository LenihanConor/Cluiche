# System Spec: DiaEntityVisualDebugger

## Parent Application
@docs/specs/applications/dia/dia.md

**Gameplay Domains:** debug

## Purpose

DiaEntityVisualDebugger is the `IDebugDomain` implementation that makes entity graph state visible in `DiaDebugPanel`. It provides six world-space and panel-hybrid drawers covering entity labels, hierarchy lines, component filter highlighting, entity picking, entity stats, and the selection inspector.

The live implementation is `EntityDebugDomain` in `Dia/DiaEntityVisualDebugger/`. This spec formalises the panel contract that was missing.

**Migration:** See `@docs/specs/applications/dia/systems/diadebugdomain/domain-migration.md`.

## Domain Identity

| Property | Value |
|----------|-------|
| Domain ID | `"Entity"` |
| Display name | `"Entity"` |
| Description | `"Entity graph — labels, hierarchy, picking, component inspector"` |
| Group | `"Entity"` |
| Accent | `DebugGroupAccents::kEntity` (`#eab308`) |
| `HasWorldDrawers()` | `true` |
| Drawers | EntityLabels, HierarchyLines, ComponentFilter, Picking, Stats, SelectionInspector |

## Panel Card Specification

Per `debugger-contract.md` AC-16 and `docs/research/visual_debugger_redesign/mockup.html`:

- **Group:** Entity — accent `DebugGroupAccents::kEntity` (`#eab308`) via `var(--accent)`
- **Spacing:** group header `padding: 5px 10px`, domain header `padding: 4px 8px`, domain body `padding: 6px 10px 8px 10px`, `margin-bottom: 3px` between cards, drawer rows `gap: 5px 10px`
- **Stat line:** `"Entities: A / T"` where A = alive, T = total allocated
- **Expanded body:**
  - 6 drawer toggles: EntityLabels, HierarchyLines, ComponentFilter, Picking, Stats, SelectionInspector
  - Stats row: Alive · Total · Max hierarchy depth
  - Selection section (shown only when SelectionInspector enabled and a pick is active): entity handle + component list

## JSON State Schema

```json
{
  "drawers": [
    { "name": "EntityLabels",       "enabled": true  },
    { "name": "HierarchyLines",     "enabled": false },
    { "name": "ComponentFilter",    "enabled": false },
    { "name": "Picking",            "enabled": false },
    { "name": "Stats",              "enabled": false },
    { "name": "SelectionInspector", "enabled": false }
  ],
  "stats": {
    "alive":            42,
    "total":            64,
    "maxHierarchyDepth": 5
  },
  "selection": {
    "entityHandle":   17,
    "componentCount":  4,
    "components": ["Position2D", "RigidBody2D", "RuleSet", "HTNPlannerComponent"]
  }
}
```

`"selection"` key is present only when a non-null entity is picked and SelectionInspector drawer is enabled; absent otherwise.

## Domain ACs

In addition to all 16 ACs in `@docs/specs/applications/dia/systems/diadebugdomain/debugger-contract.md`:

| AC | Criterion |
|----|-----------|
| D-1 | `"stats.alive"` = `Domain::GetAliveCount()`; `"stats.total"` = `Domain::GetSlotCount()` |
| D-2 | `"stats.maxHierarchyDepth"` = deepest parent chain in the current frame's entity hierarchy; 0 when no hierarchy |
| D-3 | `"selection"` present only when a non-null entity is picked and SelectionInspector drawer is enabled |
| D-4 | `"selection.components[]"` = display names of all components attached to the picked entity |
| D-5 | `ComponentFilterHighlightDrawer` draws entities that have a specific component type in accent color; filter type set via `OnCommand("setFilter", {componentType: "RigidBody2D"})` |
| D-6 | No `DrawImGui()` call anywhere in this module |
| D-7 | Panel card layout complies with AC-16 spacing contract (see Panel Card Specification above) |

## Status

**Status:** Approved
