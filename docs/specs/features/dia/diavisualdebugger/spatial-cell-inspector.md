# Feature Spec: Spatial Cell Inspector

## Parent System
@docs/specs/systems/dia/diavisualdebugger.md

---

## Problem Statement

Spatial structure overlays (SpatialGrid, HexGrid) show cell outlines but provide no way to inspect cell contents at runtime. Developers must set breakpoints or add logging to see which objects occupy a given cell. A click-to-inspect workflow closes this gap.

---

## Scope

**In scope:** SpatialGrid and HexGrid — both have meaningful cell coordinates (`WorldToCellClamped`, `WorldToHex`) and cell-level queries (`QueryCellRange`, `QueryHex`).

**Out of scope:** BVH and Quadtree — tree structures don't have user-meaningful "cells." Their nodes are internal traversal detail, not a grid the user can reason about spatially.

---

## Design

### Interaction Flow

1. User clicks inside a spatial structure's world bounds
2. The drawer resolves the click point to a cell coordinate (grid col/row or hex offset coord)
3. The clicked cell is highlighted with a distinct fill colour
4. An ImGui inspector panel shows: cell coordinate, cell bounds, object count, object IDs
5. Clicking outside all structures (or pressing Escape) clears the selection

### Selection State

Each drawer holds its own selection state — no shared selection mechanism:

```cpp
// In SpatialGridDrawer<T, Max>:
bool mHasSelection = false;
int  mSelectedCellX = -1;
int  mSelectedCellY = -1;

// In HexGridDrawer<T, Max>:
bool     mHasSelection = false;
HexCoord mSelectedHex{-1, -1};
```

### Mouse Input Path

```
InputStreamModule (mouse click + position)
  → FrameData::GetMousePixel()
  → ViewportTransform::ScreenToWorld(pixel)
  → Each drawer checks if point is within its structure's worldBounds
  → If yes: resolve to cell coord, set selection, consume click
```

Drawers read the mouse state from `FrameData` (already available in `Draw()`). Click detection is frame-based: "mouse button down this frame" from the FrameData mouse state.

### Inspector Panel (ImGui)

Each drawer's `DrawImGui()` shows the inspector when a cell is selected:

```
┌─ SpatialGrid Inspector ─────────┐
│ Cell: (2, 3)                     │
│ Bounds: [120,180] × [165,220]    │
│ Objects: 2                       │
│  #0: id=5                        │
│  #1: id=11                       │
│ [Clear]                          │
└──────────────────────────────────┘
```

### Cell Highlight

Selected cell drawn as a filled rect (SpatialGrid) or filled hexagon (HexGrid) with a semi-transparent highlight colour distinct from the depth-colour overlay (e.g. `RGBA(100, 180, 255, 80)`).

### Implementation Per Drawer (duplicated, not shared base)

- **SpatialGridDrawer**: Use `WorldToCellClamped(x, y, cx, cy)` → highlight cell rect → `QueryCellRange(cx, cy, cx, cy, out)` for contents
- **HexGridDrawer**: Use `WorldToHexClamped(pos, hex)` → highlight hex polygon → `QueryHex(hex, out)` for contents

---

## Acceptance Criteria

| # | Criterion | Verification |
|---|-----------|--------------|
| AC-1 | Clicking within a SpatialGrid's bounds highlights the containing cell | Visual inspection |
| AC-2 | Clicking within a HexGrid's bounds highlights the containing hex | Visual inspection |
| AC-3 | ImGui inspector panel shows cell coordinate, bounds, object count, and object IDs for selected cell | Visual inspection |
| AC-4 | Clicking outside all structures clears any active selection | Visual inspection |
| AC-5 | Highlight colour is visually distinct from the depth-colour overlay | Visual inspection |
| AC-6 | Selection state is per-drawer (no shared/global selection mechanism) | Code review |
| AC-7 | Inspector works in Geometry2DTestStage with the existing scatter data | E2E verification |

---

## Files Touched

| File | Change |
|------|--------|
| `Dia/DiaGeometry2DVisualDebugger/SpatialGridDrawer.h` | Add selection state, DrawImGui override |
| `Dia/DiaGeometry2DVisualDebugger/SpatialGridDrawer.inl` | Click detection, highlight draw, cell query |
| `Dia/DiaGeometry2DVisualDebugger/HexGridDrawer.h` | Add selection state, DrawImGui override |
| `Dia/DiaGeometry2DVisualDebugger/HexGridDrawer.inl` | Click detection, highlight draw, hex query |
| `Dia/DiaGeometry2DVisualDebugger/DiaGeometry2DVisualDebugger.vcxproj` | No new files — changes in existing templates |

---

## Binding Decisions

| ID | Decision | Compliance |
|----|----------|------------|
| PD-004 | No STL containers in public APIs | Template drawers use `DynamicArrayC` for query results |
| PD-001 | StringCRC for all IDs | Layer names already use StringCRC; no new IDs needed |

No other parent decisions constrain this feature.

---

## Open Design Questions

| # | Question | Answer |
|---|----------|--------|
| Q1 | How does the drawer detect "mouse clicked this frame"? FrameData carries mouse pixel position but not button state. Either extend FrameData with a click flag, or read ImGui's `IsMouseClicked()` inside `DrawImGui()` (which already has ImGui context). | **Option A — use `ImGui::IsMouseClicked(0)` + `ImGui::GetMousePos()` inside `DrawImGui()`.** No FrameData changes needed; ImGui already tracks mouse state. |
| Q2 | Should the inspector panel be collapsible/closeable independently of the layer toggle, or does disabling the layer also hide the inspector? | **Option A — disabling the layer hides everything** (overlay + inspector). Keep it simple; one toggle controls all. |

---

## Status

`Approved` — 2026-05-29
