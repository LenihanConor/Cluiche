# Gameplay Domains

Canonical vocabulary for the `gameplay_domains:` field on system spec frontmatter.
Used by `dia check gdd-sync` to generate the capability cross-reference view.

> **Do not use freeform values.** Add new domains here first, then use them in specs.

## Domain List

| Domain | Covers |
|---|---|
| `movement` | Player/entity locomotion, velocity, forces, dashing, jumping |
| `physics` | Rigid body simulation, constraints, joints, integration |
| `collision` | Detection, response, triggers, sensors, layers/masks |
| `animation` | Sprite/skeletal animation playback, blending, blend trees |
| `rendering` | Sprite/mesh drawing, materials, shaders, draw order |
| `lighting` | Dynamic lights, ambient, shadows (2D or 3D) |
| `camera` | Camera follow, shake, zoom, viewport behaviours |
| `ai` | Decision-making, utility AI, state machines, behaviour trees |
| `pathfinding` | Navigation meshes, A*, waypoints, steering |
| `input` | Keyboard, mouse, gamepad, input mapping |
| `audio` | Sound effects, music, spatial audio |
| `ui` | HUD, menus, widgets, screen layout |
| `scene` | Level loading, scene graph, room transitions |
| `persistence` | Save/load, serialisation, settings |
| `debug` | Runtime inspection, visualisation, overlays |

## Adding a New Domain

1. Add the row to the table above with a clear description.
2. Update the validation list in `dia check gdd-sync` (`_VALID_DOMAINS` set in `check.py`).
3. Commit both changes together.
