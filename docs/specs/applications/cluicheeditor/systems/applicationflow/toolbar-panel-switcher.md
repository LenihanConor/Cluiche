# Feature Spec: Toolbar Panel Switcher

**Status:** Approved
**Application:** CluicheEditor
**System:** CluicheEditor ApplicationFlow
**Research:** docs/research/editor_tab_nav/summary.md

## Parent Specs

- Platform: @docs/specs/platform/Cluiche.md
- Application: @docs/specs/applications/cluicheeditor/cluicheeditor.md
- System: @docs/specs/applications/cluicheeditor/systems/applicationflow/applicationflow.md

## Summary

Replace the single-letter toggle buttons in the editor taskbar with full-name pill buttons. When the total pill width exceeds the available toolbar space, excess panels collapse into a `⋯ +N` overflow dropdown. The change is confined entirely to `Toolbar.tsx` — no C++, no bridge contract changes.

## Problem

`Toolbar.tsx` renders each registered panel as a 24px button labelled `p.name.charAt(0)`. With 5 panels today (H, O, G, P, A) this is barely readable. As more plugins are added, initials collide (two panels starting with the same letter), there is no overflow strategy, and a new user cannot identify any panel without prior knowledge.

## Goals

1. Make every panel immediately identifiable by its full name
2. Handle any number of plugins without breaking the layout
3. Keep the change minimal — `Toolbar.tsx` only, no C++ or bridge changes

## Acceptance Criteria

| ID | Criterion |
|----|-----------|
| AC1 | Each panel button displays the full `PanelInfo.name` string, not a single character |
| AC2 | A panel that is currently visible has a filled blue (`#0e639c`) background; a hidden panel has a transparent background with a `#3c3c3c` border — same visual language as today |
| AC3 | When the combined width of all pills exceeds the available toolbar space, a `⋯ +N` overflow button appears at the right of the pill group (where N is the number of overflowed panels) |
| AC4 | Clicking the overflow button opens a dropdown listing all overflowed panels with full names and a visible/hidden indicator; clicking an item toggles that panel |
| AC5 | Clicking any pill (visible or in overflow) calls `EditorBridge.togglePanelVisibility(name)` — no change to the bridge contract |
| AC6 | Resizing the browser window recalculates which pills are visible vs overflowed dynamically |

## Out of Scope

- Icons or colour-coded avatars per panel (future work)
- Grouping / separators between plugin domains (future work)
- Moving the toolbar to a left sidebar (Candidate 3 — revisit if plugin count exceeds ~15)
- Changes to `PanelInfo` C++ struct, `DockingLayout`, or `WebUIBridge`
- Changes to `DockingManager.tsx`, `EditorBridge`, or `ProjectContextButton.tsx` internals

## Tasks

| # | Task | Notes |
|---|------|-------|
| T1 | Replace letter buttons with full-name pill buttons in `Toolbar.tsx` | Remove `p.name.charAt(0)`, use `p.name`; widen button to `fit-content` with `padding: 0 10px` |
| T2 | Implement overflow detection and `⋯ +N` button | Use `ResizeObserver` on the pill group container; hide pills that overflow; show `⋯ +N` count button (N = all overflowed panels) |
| T3 | Implement overflow dropdown | Absolute-positioned panel listing overflowed panels with full names; active panels shown with filled-blue indicator; click toggles panel and closes dropdown; closes on outside click |
| T4 | Wire resize recalculation | `ResizeObserver` callback re-runs overflow layout on toolbar width change |
| T5 | Move `ProjectContextButton` to right side | Remove from centred `flex: 1` zone; place adjacent to connection indicator on the right |

## Traceability

| Level | Spec | Key Constraint |
|-------|------|---------------|
| Platform | Cluiche.md | PD-004: No STL in public APIs — not applicable here (pure TSX) |
| Application | cluicheeditor.md | AED-005: UI built with React + DiaUICEF; this change is React-only |
| System | applicationflow.md | SCED-003: Shutdown via RequestShutdown — not affected |

## Binding Decisions Compliance

| ID | Decision (plain language) | How this feature complies |
|----|--------------------------|--------------------------|
| PD-001 | Use StringCRC for all identifiers | Not applicable — panel names are passed as `const char*` strings over the JSON bridge; the C++ side already uses StringCRC; this feature does not touch the C++ layer |
| PD-002 | ProcessingUnit/Phase/Module for app structure | Not applicable — feature is pure React TSX, no DiaApplicationFlow involvement |
| PD-003 | Component-based entities | Not applicable — UI shell component, not a game entity |
| PD-004 | No STL containers in public APIs | Not applicable — change is confined to TypeScript/React; no C++ public API is modified |
| PD-005 | x64 Windows only | Compliant — no platform-specific code added |
| PD-006 | VS project files are source of truth | Compliant — no `.vcxproj` changes; this is the React UI sub-project |
| PD-007 | C++20 required | Not applicable — TypeScript/React layer |
| AED-001 | DiaEditor is a pure library; CluicheEditor owns application flow | Compliant — no DiaEditor library changes; change is in the CluicheEditor UI layer |
| AED-005 | UI built with React + DiaUICEF + react-mosaic | Compliant — change is React-only, consistent with established UI stack |
| SCED-001 | Phases deleted, not migrated | Not applicable |
| SCED-002 | Single Running stage | Not applicable |
| SCED-003 | Shutdown via RequestShutdown | Not applicable |

## AI Review Questions

| # | Section | Question | Suggested Default | Answer |
|---|---------|----------|------------------|--------|
| 1 | AC3/T2 | Should overflow be computed via `ResizeObserver` (DOM-based, async) or by measuring pill widths in render (synchronous, may cause flicker)? | `ResizeObserver` — avoids layout thrashing and is the React-idiomatic approach | Use `ResizeObserver` |
| 2 | AC4 | Should the overflow dropdown close when a panel is toggled, or stay open to allow toggling multiple panels in one interaction? | Close on toggle — simpler; user can re-open if they want to toggle more | Close on toggle |
| 3 | AC3 | Should the `⋯ +N` count reflect only hidden-overflowed panels, or all overflowed panels (including those that happen to be visible but pushed into overflow)? | All overflowed panels (visible or not) — count reflects navigation availability, not just hidden count | All overflowed panels |
| 4 | Out of Scope | The `ProjectContextButton` currently sits in the centre of the toolbar. Should it stay centred, or move to the right alongside the connection indicator once pills take up more space? | Keep centred — no layout change in this spec; revisit if pills regularly crowd it | Move to the right alongside the connection indicator |
| 5 | T3 | Should overflowed panels that are currently visible (open in the mosaic) be shown with the filled-blue style in the dropdown, or just a checkmark? | Filled-blue badge / active indicator — consistent with the pill style | Filled-blue active indicator — consistent with pill style |

## Open Questions

None.
