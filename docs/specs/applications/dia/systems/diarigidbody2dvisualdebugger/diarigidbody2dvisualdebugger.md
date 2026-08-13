# System Spec: DiaRigidBody2DVisualDebugger

## Parent Application
@docs/specs/applications/dia/dia.md

**Gameplay Domains:** debug, physics

## Purpose

DiaRigidBody2DVisualDebugger is the `IDebugDomain` implementation that exposes RigidBody2D simulation state in `DiaDebugPanel`. It is the **reference migration** domain — the first world-space domain migrated from the old `IVisualDebugger` stack to the `IDebugDomain` architecture (SD-010 in DiaDebugDomain).

The domain wraps the five focused `IVisualDebugger` draw classes defined in `rigidbody2d-visual-debugger-stack.md` (Shapes, AABB, Velocity, Contacts, Constraints) and adds the JSON state bridge for per-drawer toggling from the panel, plus live stats rows and parameter sliders.

**Migration:** Specified in the DiaDebugDomain domain migration feature at `@docs/specs/applications/dia/systems/diadebugdomain/domain-migration.md`.

## Domain Identity

| Property | Value |
|----------|-------|
| Domain ID | `"rigidbody2d"` |
| Display name | `"RigidBody2D"` |
| Description | `"Rigid body simulation — shapes, contacts, constraints, velocities"` |
| Group | `"Physics"` |
| Accent | `DebugGroupAccents::kPhysics` (`#f59e0b`) |
| `HasWorldDrawers()` | `true` |
| Drawers | Shapes, AABB, Velocity, Contacts, Constraints |

## Panel Card Specification

Per `debugger-contract.md` AC-16 and `docs/research/visual_debugger_redesign/mockup.html`:

- **Group:** Physics — accent `DebugGroupAccents::kPhysics` (`#f59e0b`) via `var(--accent)`
- **Domain stat line:** `"Bodies: N"` where N = active body count
- **Expanded body:** drawer toggles (Shapes, AABB, Velocity, Contacts, Constraints), stats row (Active, Awake, Sleeping, Contacts, Dropped), parameter sliders (Velocity scale, Normal length)
- Spacing complies with AC-16 (outer padding/margin not overridden by domain-specific content)

## JSON State Schema (minimum)

```json
{
  "drawers": [
    { "name": "Shapes",      "enabled": true  },
    { "name": "AABB",        "enabled": false },
    { "name": "Velocity",    "enabled": true  },
    { "name": "Contacts",    "enabled": false },
    { "name": "Constraints", "enabled": false }
  ],
  "stats": {
    "active": 14, "awake": 3, "sleeping": 11, "contacts": 2, "dropped": 0
  },
  "params": {
    "velocityScale": 0.1,
    "normalLength":  0.3
  }
}
```

## Contract Compliance

Satisfies all 16 ACs from `@docs/specs/applications/dia/systems/diadebugdomain/debugger-contract.md`.

## Status

**Status:** Draft
