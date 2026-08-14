# System Spec: DiaRigidBody2DVisualDebugger

## Parent Application
@docs/specs/applications/dia/dia.md

**Gameplay Domains:** debug, physics

## Purpose

DiaRigidBody2DVisualDebugger is the `IDebugDomain` implementation that exposes RigidBody2D simulation state in `DiaDebugPanel`. It is the **reference migration domain** — the first world-space domain migrated from the old `IVisualDebugger` stack to the `IDebugDomain` architecture (SD-010 in DiaDebugDomain).

The domain wraps the five focused `IVisualDebugger` draw classes defined in `rigidbody2d-visual-debugger-stack.md` (Shapes, AABB, Velocity, Contacts, Constraints) and adds the JSON state bridge for per-drawer toggling, live body-count stats, sleep-state visualization, and parameter sliders.

**Migration:** See `@docs/specs/applications/dia/systems/diadebugdomain/domain-migration.md`.

## Domain Identity

| Property | Value |
|----------|-------|
| Domain ID | `"RigidBody2D"` |
| Display name | `"RigidBody2D"` |
| Description | `"Rigid body simulation — shapes, contacts, constraints, velocities"` |
| Group | `"Physics"` |
| Accent | `DebugGroupAccents::kPhysics` (`#f59e0b`) |
| `HasWorldDrawers()` | `true` |
| Drawers | Shapes, AABB, Velocity, Contacts, Constraints |

## Panel Card Specification

Per `debugger-contract.md` AC-16 and `docs/research/visual_debugger_redesign/mockup.html`:

- **Group:** Physics — accent `DebugGroupAccents::kPhysics` (`#f59e0b`) via `var(--accent)`
- **Spacing:** group header `padding: 5px 10px`, domain header `padding: 4px 8px`, domain body `padding: 6px 10px 8px 10px`, `margin-bottom: 3px` between cards, drawer rows `gap: 5px 10px`
- **Stat line:** `"Bodies: A awake / S sleeping (T total)"`
- **Expanded body:**
  - Drawer toggles: Shapes, AABB, Velocity, Contacts, Constraints
  - Stats table: Bodies (Active · Awake · Sleeping · Static) · Contacts · Dropped
  - Parameter sliders: Velocity scale, Normal length

## JSON State Schema

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
    "active":   14,
    "awake":     3,
    "sleeping": 11,
    "static":    2,
    "contacts":  4,
    "dropped":   0
  },
  "params": {
    "velocityScale": 0.1,
    "normalLength":  0.3
  }
}
```

## Domain ACs

In addition to all 16 ACs in `@docs/specs/applications/dia/systems/diadebugdomain/debugger-contract.md`:

| AC | Criterion |
|----|-----------|
| D-1 | `GetJSONState()` emits `stats.active`, `stats.awake`, `stats.sleeping`, `stats.static` from `PhysicsWorld` |
| D-2 | `GetJSONState()` emits `stats.contacts` = active contact count; `stats.dropped` = contacts dropped this frame (highlighted in accent color when > 0) |
| D-3 | `GetJSONState()` emits `params.velocityScale` and `params.normalLength`; `OnCommand("setParam", {key, value})` updates these and propagates to `VelocityArrowsDrawer` and `ContactNormalsDrawer` |
| D-4 | `PhysicsShapesDrawer` renders sleeping bodies at 50% opacity (dimmed) relative to awake bodies; static bodies in a distinct palette color |
| D-5 | `VelocityArrowsDrawer` includes an angular velocity ring: a circle of radius proportional to `angularVelocity` drawn around each body's center of mass when angular speed exceeds a threshold (default 0.1 rad/s) |
| D-6 | No `DrawImGui()` call anywhere in this module |
| D-7 | Panel card layout complies with AC-16 spacing contract (see Panel Card Specification above) |

## Contract Compliance

Satisfies all 16 ACs from `@docs/specs/applications/dia/systems/diadebugdomain/debugger-contract.md`.

## Status

**Status:** Draft
