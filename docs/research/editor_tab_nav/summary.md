# Research Summary — Editor Taskbar Navigation

**Session folder:** docs/research/editor_tab_nav/
**Date:** 2026-05-21

## One-Line Answer

Replace the single-letter toggle buttons in `Toolbar.tsx` with full-name pill buttons and an overflow `⋯ +N` dropdown — no C++ changes, no layout restructure.

## Journey

1. **Explored:** The bottom taskbar renders one 24px button per registered panel using `p.name.charAt(0)` — breaks on duplicate initials, unreadable, no overflow handling. The fix is pure React/TSX.
2. **Ideated:** 8 candidates generated, spanning S–M scope; ranged from 3-char abbreviation patches through to VS Code-style activity bar and full icon+label sidebars.
3. **Evaluated:** Candidates 1 and 3 reviewed via HTML mockups; both solve the core scaling problem cleanly.
4. **Chose:** Candidate 1 selected by user after seeing both mockups — smallest diff, preserves layout, solves the problem immediately.

## Chosen Work Item

**Name:** Full-Label Pill Bar with Overflow Menu
**Home module:** CluicheEditor UI — `Cluiche/CluicheEditor/UI/src/layout/Toolbar.tsx`
**Suggested spec type:** Feature
**Estimated size:** S

## Key Insights from Exploration

- The entire problem is in one file (`Toolbar.tsx:69`) — `p.name.charAt(0)` is the only change needed to the label
- Overflow needs ~20 lines of width-measurement logic (or a CSS-only approach with `overflow: hidden` + a `⋯` button)
- No C++ changes required — `PanelInfo.name` already carries the full string
- The `EditorBridge.togglePanelVisibility` / `panels_changed` contract is unchanged
- Candidate 3 (activity bar) is the natural next step if the editor grows past ~15 panels; the pill bar should be designed so it can be swapped out
- Collision in the current bar becomes visible at 8+ plugins (two panels starting with 'A', 'S', etc.)

## Discarded Candidates

| Candidate | Why discarded |
|-----------|--------------|
| 2 — 3-char abbreviation | Still collides at scale |
| 3 — Activity bar | Right long-term direction but larger layout change than needed today |
| 4 — Popover picker | Two-click to toggle; ergonomics regression |
| 5 — Grouped sections | Needs C++ struct change; overengineered |
| 6 — Tab strip | Changes mental model, harder to discover hidden panels |
| 7 — Icon + label sidebar | Needs icon story + C++ change |
| 8 — Command palette nav | Removes persistent visual toggle surface |

## References

- docs/research/editor_tab_nav/explore.md
- docs/research/editor_tab_nav/ideate.md
- docs/research/editor_tab_nav/mockup_c1_pill_bar.html
- docs/research/editor_tab_nav/mockup_c3_activity_bar.html
- docs/research/editor_tab_nav/choose.md
