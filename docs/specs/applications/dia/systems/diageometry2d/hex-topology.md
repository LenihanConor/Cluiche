# Feature Spec: Hex Topology

**Parent:** @docs/specs/applications/dia/systems/diageometry2d/diageometry2d.md
**Status:** `Done`
**Plan:** @docs/specs/applications/dia/systems/diageometry2d/hex-topology.plan.md
**Origin:** 2026-11-23 architecture cleanup. `dia check arch` reports `dia.celllayers -> dia.pathfinding` (3 includes of `HexTopology.h`): a visual package reaching into a gameplay package for pure geometry.
**Moves from:** @docs/specs/applications/dia/systems/diapathfinding/hex-rectangular-grid.md (the feature that introduced `HexTopology`)

## Summary

Move `HexTopology` and `CellCoord` out of `DiaPathfinding` into `DiaGeometry2D` (`foundation/maths`). They are pure 2D hex-grid math with no pathfinding in them, and visual, gameplay and Draoi code all use them. Placing them below every consumer removes the cross-domain include and gives Draoi a direct dependency on geometry instead of on pathfinding.

No behaviour changes. This is a relocation plus a namespace change.

## Problem

- `HexTopology` (index and coordinate conversion, six-direction neighbours, hex distance via cube coordinates, cell centre and point-to-cell mapping, direction vectors) depends only on `Vector2D` and `CellCoord`.
- It lives in `DiaPathfinding` (gameplay) because pathfinding was its first user.
- `DiaCellLayers` (visual) uses it for picking and the selection tools (`CellFromPoint`, `ToIndex`, `ToCoord`, `CellCenter`, `NeighbourAt`), so visual depends on gameplay. The layer rules forbid that.
- 47 files in Dia and Draoi (61 with the apps) reference it: pathfinding, `DiaFlowField`, `DiaGridVisibility`, `DiaCellLayers`, and the Draoi hex-world packages.
- Alternative ruled out 2026-11-23: `DiaGraphics/CellField` cannot replace it. It is a GPU vertex builder (`ICellGridGeometryBuilder`, `HexFlatTopOddQBuilder`), with no neighbours, picking or point-to-cell mapping.

## Acceptance Criteria

1. `CellCoord` and `HexTopology` live in `DiaGeometry2D` under `Grid/`, in namespace `Dia::Geometry2D`, with the same members and the same behaviour.
2. `DiaPathfinding` no longer owns either type. Pathfinding code includes them from `DiaGeometry2D`.
3. `dia check arch` no longer reports `dia.celllayers -> dia.pathfinding`, and reports no new violation.
4. `dia check deps` passes. `dia.geometry2d` already depends on `dia.maths`; `dia.celllayers`, `dia.flowfield`, `dia.gridvisibility`, `dia.pathfinding` and the affected Draoi modules list `dia.geometry2d`.
5. All existing `HexTopology` tests pass unchanged apart from the namespace and include, and live beside the code in the geometry test folder.
6. The Draoi hex-world and terrain golden files are byte-identical before and after.
7. No compatibility alias is left behind when the feature is Done.

## API Design

No new API. Names and signatures are unchanged; only the home changes.

```cpp
// DiaGeometry2D/Grid/CellCoord.h      (was DiaPathfinding/CellCoord.h)
// DiaGeometry2D/Grid/HexTopology.h    (was DiaPathfinding/HexTopology.h)
namespace Dia::Geometry2D {
    struct CellCoord { int x; int y; bool operator==(const CellCoord&) const = default; };
    class HexTopology { /* unchanged */ };
    constexpr uint32_t kNoCell, kHexDirections;
}
```

`HexNeighbours.h` stays in `DiaPathfinding`: it is a neighbour policy over the grid search, not geometry. It changes only its include and namespace use.

## Binding Decisions

| Decision | Source | How it constrains this feature |
|---|---|---|
| PD-011: layers point down only | `docs/specs/platform/Cluiche.md` | The reason for the move: geometry sits below visual and gameplay. |
| PD-014: Dia has no gameplay vocabulary | `docs/specs/platform/Cluiche.md` | `HexTopology` is neutral geometry, so it qualifies. |
| Escalation rule | `CLAUDE.md` | Adds new header and implementation files to `DiaGeometry2D`. **User sign-off for the placement was given 2026-11-23.** |
| Determinism | Draoi hex-world goldens | Code is moved, not edited; golden files must stay byte-identical (AC 6). |

## Tasks

Detailed rows are in the plan. In outline:

1. Move the files into `DiaGeometry2D/Grid/`, with forwarding aliases left in `DiaPathfinding` so everything still compiles.
2. Migrate every user (pathfinding, flowfield, grid visibility, cell layers, Draoi, apps, tests) to the new include and namespace.
3. Remove the aliases and the old files, move the tests, update module docs and the registry.
4. Verify and close out.

## Open Design Questions

1. **Namespace.** `Dia::Geometry2D` matches the rest of the package. The scaffold suggested `Dia::Hex::Topology`; I would not add a new namespace for two types. Confirm before task 1.
2. **Two-step move vs one sweep.** The plan leaves temporary aliases in `DiaPathfinding` for one task so the 47-file migration can be built and tested in pieces. If you prefer a single mechanical sweep with no aliases, it saves a task but makes any build break harder to localise.
3. **Docs that name the old location.** About 10 CoW specs say `Dia::Pathfinding::HexTopology`. They are history, not contracts. The plan updates only `hex-rectangular-grid.md` (with a "moved" note) and leaves the rest.
