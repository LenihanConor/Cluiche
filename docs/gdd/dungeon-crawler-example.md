# GDD → TDD Capability Cross-Reference (Example)

> This is an example output showing the format for cross-referencing a Game Design Document
> against engine capabilities. One row per GDD requirement. Generated/maintained by hand;
> validated by `dia check gdd-sync`.
>
> **Status values:** `built` | `partial` | `not-started` | `out-of-scope`
> **Source of truth for status:** the referenced spec's `status:` frontmatter field.

---

## Game: Dungeon Crawler Prototype

**GDD location:** `docs/gdd/dungeon-crawler.md`
**Last synced:** 2026-07-28

---

## Movement

| # | GDD Requirement | Engine Capability | Spec | Status | Gap / Notes |
|---|---|---|---|---|---|
| M-01 | Player moves with WASD; velocity-based, not grid-snapping | 2D rigid body velocity integration | [DiaRigidBody2D](../specs/applications/dia/systems/diarigidbody2d/diarigidbody2d.md) | `built` | PointBody2D covers translational movement |
| M-02 | Player slides along walls (no sticking) | Impulse-based collision response + friction | [DiaRigidBody2D](../specs/applications/dia/systems/diarigidbody2d/diarigidbody2d.md) | `built` | Friction coefficient configurable per body |
| M-03 | Dash ability: short burst with cooldown | Movement modifier with cooldown timer | — | `not-started` | No engine concept of ability cooldowns — game code only |
| M-04 | Knockback from enemy hits | Impulse application to player body | [DiaRigidBody2D](../specs/applications/dia/systems/diarigidbody2d/diarigidbody2d.md) | `built` | `ApplyImpulse()` on PointBody2D |

---

## Collision

| # | GDD Requirement | Engine Capability | Spec | Status | Gap / Notes |
|---|---|---|---|---|---|
| C-01 | Player vs dungeon walls (AABB) | Broad + narrow phase collision detection | [DiaRigidBody2D](../specs/applications/dia/systems/diarigidbody2d/diarigidbody2d.md) | `built` | |
| C-02 | Player vs enemy (circle-circle) | Narrow-phase circle intersection | [DiaGeometry2D](../specs/applications/dia/systems/diageometry2d/diageometry2d.md) | `built` | |
| C-03 | Projectile destroys on first hit, passes through enemies with pierce | Collision layer + mask filtering | [DiaRigidBody2D](../specs/applications/dia/systems/diarigidbody2d/diarigidbody2d.md) | `built` | Bitmask pair per body; pierce requires game-code hit counter |
| C-04 | Trigger zones (rooms, pickups) — no physical response | Sensor / trigger body type | [DiaRigidBody2D](../specs/applications/dia/systems/diarigidbody2d/diarigidbody2d.md) | `not-started` | Kinematic bodies exist but sensor/trigger (overlap-only, no response) is **not** in spec — needs feature spec |

---

## AI

| # | GDD Requirement | Engine Capability | Spec | Status | Gap / Notes |
|---|---|---|---|---|---|
| A-01 | Enemy patrols between waypoints | Pathfinding + waypoint traversal | [DiaPathfinding](../specs/applications/dia/systems/diapathfinding/diapathfinding.md) | `partial` | Spec is Draft; pathfinding not yet shipped |
| A-02 | Enemy transitions: Patrol → Chase → Attack based on player proximity | Hierarchical state machine | [DiaStateMachine](../specs/applications/dia/systems/diastatemachine/diastatemachine.md) | `built` | |
| A-03 | Enemy scores actions by urgency (attack vs flee vs heal) | Utility AI scorer | [DiaUtilityAI](../specs/applications/dia/systems/diautilityai/diautilityai.md) | `not-started` | Spec exists (Draft); not implemented |
| A-04 | Enemy reads world state (health, distance, noise level) | Blackboard data store | [DiaBlackboard](../specs/applications/dia/systems/diablackboard/diablackboard.md) | `built` | Typed slot blackboard shipped |
| A-05 | Group enemies coordinate via shared signal ("player spotted") | Blackboard shared across entities | [DiaBlackboard](../specs/applications/dia/systems/diablackboard/diablackboard.md) | `partial` | Global blackboard exists; per-group scoping is **not** in spec — needs feature spec |

---

## Animation

| # | GDD Requirement | Engine Capability | Spec | Status | Gap / Notes |
|---|---|---|---|---|---|
| AN-01 | Character sprite sheet animation (idle, run, attack) | Frame-based 2D animation | [DiaAnimation2D](../specs/applications/dia/systems/diaanimation2d/diaanimation2d.md) | `built` | |
| AN-02 | Blend between run and idle based on velocity | Animation blending / blend tree | [DiaAnimation2D](../specs/applications/dia/systems/diaanimation2d/diaanimation2d.md) | `partial` | Spec unclear on blend tree support — needs feature spec |
| AN-03 | Hit-flash shader effect on damage | Material parameter override per entity | [DiaBgfx3D](../specs/applications/dia/systems/diabgfx3d/diabgfx3d.md) | `not-started` | Per-entity material override not confirmed in spec |

---

## Rendering

| # | GDD Requirement | Engine Capability | Spec | Status | Gap / Notes |
|---|---|---|---|---|---|
| R-01 | Sprite rendering with layered draw order | 2D sprite renderer with sort order | [DiaGraphics](../specs/applications/dia/systems/diagraphics/diagraphics.md) | `built` | |
| R-02 | Dynamic lighting: torchlight pools around player | 2D dynamic point lights | [DiaLighting2D](../specs/applications/dia/systems/dialighting2d/dialighting2d.md) | `not-started` | Spec exists; not implemented |
| R-03 | Fog of war: unexplored rooms are blacked out | Visibility / masking layer | — | `not-started` | No engine concept — game code or custom shader needed; no spec exists |
| R-04 | Screen shake on heavy hit | Camera trauma/shake behaviour | [DiaCamera2D](../specs/applications/dia/systems/diacamera2d/diacamera2d.md) | `partial` | Camera behaviours in spec; shake specifically not confirmed — needs feature spec |

---

## Scene & Persistence

| # | GDD Requirement | Engine Capability | Spec | Status | Gap / Notes |
|---|---|---|---|---|---|
| SP-01 | Load dungeon room from file, swap on door trigger | Scene loading from .diascene asset | [DiaScene2D](../specs/applications/dia/systems/diascene2d/diascene2d.md) | `not-started` | Pre-spec only; Scene2DModule not yet specced |
| SP-02 | Save/load player state (health, inventory, position) | Serialisation to/from JSON | [DiaSerializer](../specs/applications/dia/systems/diaserializer/diaserializer.md) | `built` | |
| SP-03 | Procedurally generated dungeon layout at runtime | Procedural generation system | — | `out-of-scope` | No engine concept planned — game code entirely; no spec needed |

---

## UI

| # | GDD Requirement | Engine Capability | Spec | Status | Gap / Notes |
|---|---|---|---|---|---|
| U-01 | HUD: health bar, stamina bar, minimap | In-world UI rendering | [DiaUIUltralight](../specs/applications/dia/systems/diauiultralight/diauiultralight.md) | `partial` | DiaUI exists; minimap has no spec |
| U-02 | Pause menu with resume/quit | Menu UI with input routing | [DiaUIUltralight](../specs/applications/dia/systems/diauiultralight/diauiultralight.md) | `partial` | Input routing to UI vs game not confirmed |
| U-03 | Inventory screen (drag-drop grid) | Drag-and-drop UI widget | — | `not-started` | No engine concept — game code + DiaUI base; no spec exists |

---

## Gap Summary

| Status | Count |
|---|---|
| `built` | 11 |
| `partial` | 7 |
| `not-started` | 9 |
| `out-of-scope` | 1 |

### Feature specs needed (partial gaps that need drilling down)
- `DiaRigidBody2D` — sensor/trigger body type (C-04)
- `DiaBlackboard` — per-group scoping (A-05)
- `DiaAnimation2D` — blend tree support (AN-02)
- `DiaCamera2D` — screen shake behaviour (R-04)
- `DiaUI` — minimap widget (U-01), input routing (U-02)

### Engine work needed (not-started with no spec)
- Fog of war (R-03) — game code or custom shader; no spec path
- Inventory drag-drop (U-03) — game code on top of DiaUI
- Dash cooldown (M-03) — game code only

### Backlog candidates (not-started with spec path)
- Sensor/trigger body (C-04) → feature spec under DiaRigidBody2D
- DiaLighting2D implementation (R-02) → already specced, needs scheduling
- DiaScene2D / Scene2DModule (SP-01) → spec work needed first
- DiaUtilityAI implementation (A-03) → specced Draft, needs scheduling
- DiaPathfinding implementation (A-01) → specced Draft, needs scheduling
