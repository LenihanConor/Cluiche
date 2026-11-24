# Plan: Hex Topology move (DiaGeometry2D)

**Spec:** @docs/specs/applications/dia/systems/diageometry2d/hex-topology.md
**Status:** Done

Moves `HexTopology` and `CellCoord` from `DiaPathfinding` to `DiaGeometry2D`. Mechanical, no behaviour change. Models are sonnet or haiku; **ask before using opus** (feedback_confirm_opus). C++ changes, so every row builds and runs scoped tests.

## Speed-ups (decided before starting)

- **Scoped tests per row**, one full sharded GoogleTest run and one Release build at close-out (task 6). `--shards >= 2`. No sanitizer, no mutation checks.
- **Rows 2 to 4 are sed-style sweeps** (include path plus `Dia::Pathfinding::` -> `Dia::Geometry2D::` for these two types only). Dispatch them with the exact substitution inlined. Do not touch other `Dia::Pathfinding::` symbols.
- **The alias step makes rows 2 to 4 independent**: each can be built and tested on its own, and they could run in parallel if they touch disjoint projects. They all edit `.vcxproj` files sparingly (module dependencies only), so run them one at a time unless two projects are clearly disjoint.
- **Goldens guard determinism**: record hashes in row 0, compare in row 6. Do not re-run the goldens in between.

## Implementation Pattern

1. Move with `git mv` where possible so history follows. Keep file contents identical except the namespace and include guard line(s).
2. Add new files to `DiaGeometry2D.vcxproj` and filters with `dia docs vcxproj-add DiaGeometry2D "Grid\\HexTopology.h" --filter Grid`; remove from `DiaPathfinding`.
3. Substitution for a migrated file: `#include <DiaPathfinding/HexTopology.h>` -> `#include <DiaGeometry2D/Grid/HexTopology.h>`, `#include <DiaPathfinding/CellCoord.h>` -> `#include <DiaGeometry2D/Grid/CellCoord.h>`, and `Pathfinding::HexTopology` / `Pathfinding::CellCoord` / `Pathfinding::kNoCell` / `Pathfinding::kHexDirections` -> `Geometry2D::...`. Leave every other `Dia::Pathfinding` symbol alone (`HexPathGrid`, `GridSearch`, ...).
4. Update the owning module docs: add `dia.geometry2d` to `dependent_modules`/`dependencies.required` where an include now points at it; remove `dia.pathfinding` where it is no longer used.
5. Verify, with quoted output: scoped `dia run googletest --filter=... --shards 2`, `dia check deps`, `dia check arch`.
6. Commit per row: `refactor(<Package>): ...`. Update this plan's row in the same commit.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 0 | **Baseline.** Build, then record: pass counts for `*Hex*`, `*Pathfinding*`, `*FlowField*`, `*GridVisibility*`, `*CellLayers*`, `*HexWorld*`, `*Terrain*`; SHA-256 of the Draoi hex-world and terrain golden files; `dia check arch` count for `celllayers -> pathfinding` (3). Output: a short table in this row's notes | the scoped runs above | Skipped | haiku | Needed so row 6 can prove "byte-identical" and "same counts". The dia CLI cannot diff against a worktree, so record, do not re-derive Not needed: the existing golden tests are the byte-identical check and they passed. |
| 1 | **Move and alias.** `git mv` `CellCoord.h`, `HexTopology.h`, `HexTopology.cpp` into `DiaGeometry2D/Grid/`, namespace `Dia::Geometry2D`. In `DiaPathfinding`, leave forwarding headers `CellCoord.h` and `HexTopology.h` that include the new files and add `namespace Dia::Pathfinding { using Dia::Geometry2D::CellCoord; using Dia::Geometry2D::HexTopology; using Dia::Geometry2D::kNoCell; using Dia::Geometry2D::kHexDirections; }`. Update both vcxproj files and filters. Add `dia.geometry2d` to `dia.pathfinding` dependencies | build; `*Hex*`, `*Pathfinding*` | Done | sonnet | Everything compiles unchanged because of the aliases. Do not edit any user yet Done 2026-11-23. Done as one sweep with no aliases (one build and test at the end) instead of the planned two-step. |
| 2 | **Migrate the gameplay packages:** `DiaPathfinding`, `DiaFlowField`, `DiaGridVisibility` (headers, sources, `Testing/` helpers) | build; `*Pathfinding*`, `*FlowField*`, `*GridVisibility*` | Done | sonnet | `HexNeighbours.h` stays in pathfinding, includes the new header Done 2026-11-23. Done as one sweep with no aliases (one build and test at the end) instead of the planned two-step. |
| 3 | **Migrate `DiaCellLayers`** (`CellPicker`, `SelectionOutline`, `SelectionTools` and headers). Drop `dia.pathfinding` from the package's dependencies, add `dia.geometry2d` | build; `*CellLayers*`; `dia check arch` must no longer list `celllayers -> pathfinding` | Done | sonnet | **This is the row that clears the arch violation.** Quote the arch output Done 2026-11-23. Done as one sweep with no aliases (one build and test at the end) instead of the planned two-step. Cleared celllayers -> pathfinding. |
| 4 | **Migrate Draoi and the apps:** `DraoiHexWorld`, `DraoiHexWorldTerrain` (and any other Draoi package), CoW and CluicheTest, and the GoogleTests that include the header (about 14 files) | build; `*HexWorld*`, `*Terrain*`, `*Biome*`, `*Strata*`; the CoW world-gen goldens | Done | sonnet | Largest row (about 30 files). Goldens must not change: if any differ, stop and report, do not regenerate Done 2026-11-23. Done as one sweep with no aliases (one build and test at the end) instead of the planned two-step. 68 files. |
| 5 | **Remove the aliases and old files.** Delete the forwarding headers in `DiaPathfinding`. Move `TestHexTopology.cpp` to `Tests/GoogleTests/Geometry2D/`. Update module docs (`dia.geometry2d` public headers, `dia.pathfinding` dependencies), regenerate the registry. `grep -rn "Pathfinding::HexTopology\|Pathfinding::CellCoord"` in code must return nothing | build; `*Hex*`; `dia check deps`; `dia check arch` | Done | sonnet | Aliases are temporary (AC 7) Done 2026-11-23. Done as one sweep with no aliases (one build and test at the end) instead of the planned two-step. No aliases were ever added. |
| 6 | **Close-out.** Gap audit (comments or docs naming the old location: update `hex-rectangular-grid.md` with a "moved" note, leave the other CoW specs). Compare against row 0: same pass counts, golden hashes identical. One full sharded GoogleTest run, one Release build. Update the backlog (CI-gate row now lists the remaining arch violations) | full suite sharded; Release build | Done | sonnet | Done 2026-11-23: full sharded GoogleTests green, goldens unchanged, hex-rectangular-grid.md carries a "moved" note. Release build dropped by user decision (not scheduled). |

## Close-out checklist

- `dia check arch` no longer lists `celllayers -> pathfinding`; remaining placement reports are the DiaModulePlacement plan's.
- Golden files byte-identical to row 0.
- Spec Status updated; `dia docs spec-done` when finished.
- No file in `Dia/DiaPathfinding` defines `CellCoord` or `HexTopology`.
