# Feature Spec: GridVisibility Test Stage

**Status:** Done
**Parent:** @docs/specs/applications/cluichetest/systems/teststages/teststages.md

## Problem Statement

`DiaGridVisibility` unit tests validate the shadowcasting algorithm and cell-state transitions in isolation; no stage verifies two independent sight-source groups running simultaneously, `Revealed` fog persistence, or `IVisibilityChangeObserver` callbacks at real frame rate with visual feedback.

## Template Answers (T1–T10)

| # | Question | Answer |
|---|----------|--------|
| T1 | Engine features exercised | `GridVisibilitySystem<SquarePathGrid>`: two independent `VisibilityGroupId` groups, `RegisterSightSource`, `Update()`, `CanSee`, `GetCellState`, `GetVisibleEntities`, `IVisibilityChangeObserver` entity-spotted callbacks, `VisibilityState` (Hidden/Revealed/Visible) transitions with Revealed-decay |
| T2 | Scene setup in DoStart | 24×18 `SquarePathGrid` (cellSize = 30 world units, origin centred on screen). Centre vertical divider wall with a 3-cell gap corridor. L-shaped occlusion walls in each half. **Red patrol** (Group 0, `RGBA(220,80,80)`): circular waypoint loop through the left half, sight radius 7 vis-cells. **Blue patrol** (Group 1, `RGBA(80,130,220)`): mirrored loop through the right half, same radius. **5 static enemy entities** (`RGBA(120,120,130)` default, pulse-to-group-colour on detection): 2 in left zone, 2 in right zone, 1 near the centre corridor. `IVisibilityChangeObserver` registered: sets per-enemy flash timer on `OnEntityBecameVisible`. |
| T3 | Checkpoints and success conditions | `gridvis.red_loop_complete` — Red patrol returns to start waypoint (first full loop). `gridvis.blue_loop_complete` — Blue patrol returns to start waypoint. `gridvis.both_loops_complete` — both complete (primary pass gate). |
| T4 | Metrics emitted | `cluichetest.gridvis.red_cells_visible` (Gauge, current Visible cell count for Group 0), `cluichetest.gridvis.blue_cells_visible` (Gauge, Group 1), `cluichetest.gridvis.enemies_spotted_by_red` (Gauge), `cluichetest.gridvis.enemies_spotted_by_blue` (Gauge) |
| T5 | Processing Unit | SimPU — `GridVisibilitySystem` is single-threaded; all Update/query calls on SimPU |
| T6 | Assets needed | None — grid layout (wall mask), patrol waypoints, and enemy positions are compile-time constants |
| T7 | Gap vs unit tests | Unit tests never exercise: (a) two groups on the same system running simultaneously at 30 Hz, (b) Revealed fog persistence across multiple patrol sweeps, (c) `IVisibilityChangeObserver::OnEntityBecameVisible` triggering a visual flash on a real entity handle, (d) `GetVisibleEntities` returning an up-to-date span after `Update()` at frame rate |
| T8 | Determinism constraints | Fixed-timestep SimPU (30 Hz). Patrol movement: constant speed (4 vis-cells/sec), linear interpolation between fixed waypoints. No randomness. Same frame count → same patrol position → same visibility state. |
| T9 | Expected frame budget | Each patrol loop ≈ 13–15 s (waypoint perimeter ~52 cells ÷ 4 cells/s). Both loops complete by ~15 s (≈ 450 frames). Budget: 1200 frames (40 s at 30 Hz). |
| T10 | Dependencies on other modules | `DiaGridVisibility`, `DiaPathfinding` (SquarePathGrid), `DiaEntity` (Domain, Entity), `DiaEntitySpatial` (EntitySpatialModule, SpatialComponent), `VisualDebuggerModule`, `AutomationModule`, `DiaObservation::Metric` |

## Acceptance Criteria

### Shared ACs (inherited from Infrastructure spec)

This stage satisfies AC-S1 through AC-S9 as defined in the [Infrastructure spec](test-stage-infrastructure.md#a--shared-acs-every-test-stage-must-satisfy-these).

### Stage-Specific ACs

| # | Criterion | Verification |
|---|-----------|--------------|
| AC-C1 | Red patrol completes one full waypoint loop | `gridvis.red_loop_complete` checkpoint passes |
| AC-C2 | Blue patrol completes one full waypoint loop | `gridvis.blue_loop_complete` checkpoint passes |
| AC-C3 | Both checkpoints pass within 1200 frames | `gridvis.both_loops_complete` checkpoint; orchestrator timeout |
| AC-C4 | Grid cells render in three visually distinct states: near-black (Hidden), dim-tinted (Revealed), bright-tinted (Visible); Red group tints warm red, Blue group tints cool blue; overlap cells tint purple | Visual inspection during `dia run cluichetest` |
| AC-C5 | Wall cells cast visible shadow wedges — cells behind walls from the patrol's perspective remain Hidden or Revealed while exposed cells are Visible | Visual inspection |
| AC-C6 | Enemy entities pulse to group colour when `CanSee` returns true and return to grey when no longer visible | Visual inspection; `enemies_spotted_by_red/blue` metrics reflect correct count |
| AC-C7 | The two groups are independent: Red's fog does not reveal right-zone cells; Blue's fog does not reveal left-zone cells | Visual inspection |
| AC-C8 | Revealed state persists after the patrol moves away — cells do not snap back to Hidden | Visual inspection (dim-tinted fog persists behind the patrol trail) |
| AC-C9 | Repeated runs produce identical metric values at frame 300 | Determinism — AC-S7 |

## Design

### World Layout

```
24×18 SquarePathGrid, cellSize = 30 world units
World bounds: (−360, −270) → (360, 270)  [centred on origin]

Zones:
  Left  (Group 0 — Red):   cols  0–10
  Right (Group 1 — Blue):  cols 13–23
  Corridor:                cols 11–12, rows 7–10  (gap in centre divider)

Centre divider:   cols 11–12, rows 0–6 and rows 11–17    (impassable)
L-wall left-A:    cols 3–5, row 4  +  col 5, rows 4–7    (impassable)
L-wall left-B:    cols 2–4, row 13                        (impassable)
L-wall right-A:   cols 18–20, row 4  +  col 18, rows 4–7 (impassable)
L-wall right-B:   cols 19–21, row 13                      (impassable)

Red patrol waypoints (Group 0 — left zone):
  (1,1) → (9,1) → (9,16) → (1,16) → (1,1)   [4-cell clearance from edges]

Blue patrol waypoints (Group 1 — right zone):
  (14,1) → (22,1) → (22,16) → (14,16) → (14,1)

Enemy positions (static):
  E0 (left zone):   (5, 6)   — behind L-wall-A, first spotted mid-loop
  E1 (left zone):   (8, 14)  — open area, near patrol bottom leg
  E2 (corridor):    (11, 9)  — centre gap, either group can see it as patrol passes
  E3 (right zone):  (15, 3)  — open area, near patrol top leg
  E4 (right zone):  (19, 12) — behind L-wall-B, first spotted mid-loop
```

### Cell Colour Scheme

| State | Red group only | Blue group only | Both groups |
|-------|----------------|-----------------|-------------|
| Hidden | `RGBA(10,10,15,255)` | `RGBA(10,10,15,255)` | `RGBA(10,10,15,255)` |
| Revealed | `RGBA(55,22,22,255)` | `RGBA(22,22,55,255)` | `RGBA(40,22,40,255)` |
| Visible | `RGBA(160,60,50,215)` | `RGBA(50,90,160,215)` | `RGBA(130,55,140,215)` |
| Wall | `RGBA(25,25,30,255)` | — | — |

Per-cell render: compute worst-case state for each group independently, then select the combined colour from the table above. Single `RequestDrawRect` call per cell.

### Drawer

`GridVisibilityDrawer` — single `IVisualDebugger` implementor registered with `DebugLayerManager` at priority 20, layer `"GridVisibilityTest"`:

1. **Cell pass** — `RequestDrawRect(tl, br, outline, fill)` per cell using colour table
2. **Entity pass** — `RequestDraw(pos, radius, colour)` per entity; enemy radius pulses `8 + 4·sin(t·6f)` while flash timer > 0; patrol radius fixed at 10
3. **HUD** — `RequestDrawText` top-left: `"Red:  N vis | N spotted"`, top-right: `"Blue: N vis | N spotted"`

### Patrol Movement

Linear interpolation between world-space waypoints at 4 vis-cells/sec (120 world units/sec). `SpatialComponent` position updated each SimPU frame. Loop counter incremented when patrol crosses within 0.5 cells of waypoint[0] after having advanced past waypoint[1].

## Binding Decisions

| ID | Source | Decision | Compliance |
|----|--------|----------|------------|
| SD-TS-001 | TestStages | One manifest stage per test feature | Stage adds one `gridvisibility_test_stage` entry to the v3 manifest |
| SD-TS-002 | TestStages | Checkpoints registered in DoStart, auto-cleared on stop | All three checkpoints registered in `OnStart`; no manual cleanup |
| SD-TS-003 | TestStages | Test stages emit metrics | Four Gauge metrics registered in `OnStart` |
| SD-TS-004 | TestStages | Each test stage returns to Boot | Manifest `transitions` includes `"Boot"` |
| PD-001 | Platform | StringCRC for IDs | Stage name, module type ID, checkpoint names, metric names all `StringCRC` |
| PD-006 | Platform | VS project files source of truth | Module .h/.cpp added to CluicheTest.vcxproj + .vcxproj.filters |
| AD-001 (CT) | CluicheTest | Three PUs | Stage module lives on SimPU (GridVisibilitySystem single-threaded, matches SimPU) |

## Open Design Questions

| # | Question | Resolution |
|---|----------|------------|
| ODQ-1 | **EntitySpatial wiring**: `GridVisibilitySystem::Update()` requires `EntitySpatialModule&` and `Entity::Domain&`. Should the stage use a `ModuleRef<EntitySpatialModule>` to the shared PU module (which may contain other entities), or create a minimal local spatial index for the 7 entities it owns? | **Resolved: own a local `Dia::Entity::Domain` + `std::unique_ptr<Dia::EntitySpatial::EntitySpatialModule>`**. This is the established pattern in `EntitySpatialTestStageModule`. The test stage owns its entities entirely; using the shared module would couple stage lifetime to PU-module ordering unnecessarily. |
| ODQ-2 | **Sight radius vs grid coverage**: Radius of 7 vis-cells on a 24-wide grid covers ~14 cells diameter. For a patrol looping the perimeter of an 8-cell-wide zone this means most interior cells may flip Visible on every pass with little dark area visible at once. Should radius be reduced to 5 (leaves a dark interior pocket) or kept at 7 (keeps the lit area dramatic)? | **Resolved: 6 vis-cells**. Patrol at the left edge (col 1) reaches col 7; at the right edge (col 9) reaches col 3 — leaving a persistent dark pocket in the zone interior that fills with Revealed fog over the loop. At 7 the interior stays mostly lit; at 5 the zone feels murky. L-walls carve additional shadow wedges on top of the baseline radius. |
