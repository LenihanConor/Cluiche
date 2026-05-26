# Research: Choice — Editor Taskbar Navigation

**Date:** 2026-05-21
**Chosen candidate:** Full-Label Pill Bar with Overflow Menu (Candidate 1)

## Rationale

Chosen directly after reviewing mockups for Candidate 1 and Candidate 3. The pill bar solves the scaling problem (label collisions, unreadable single letters) with the smallest possible code change — only `Toolbar.tsx` needs editing. No C++ changes, no layout restructure, no new components. The overflow `⋯ +N` dropdown handles arbitrary plugin counts cleanly.

Candidate 3 (activity bar) was the main alternative — it has zero vertical cost and scales better visually, but requires restructuring the shell layout (`DockingManager.tsx` from column to row flex) which is a larger change for no immediate gain at today's plugin count.

## What Was Ruled Out

| Candidate | Reason not chosen |
|-----------|------------------|
| 2 — 3-char abbreviation | Still collides at scale; just defers the problem |
| 3 — Left activity bar | Better long-term scale but larger layout restructure; revisit if plugin count grows past ~15 |
| 4 — Popover panel picker | Two-click path to toggle a panel; worse ergonomics for frequent switching |
| 5 — Grouped sections | Needs C++ `PanelInfo` change; overengineered for current panel count |
| 6 — Tab strip (open/close) | Changes mental model (tabs = open, not registered); harder to discover hidden panels |
| 7 — Icon + label sidebar | Needs icon story + C++ change; too much scope for the problem at hand |
| 8 — Command palette nav | Removes the persistent visual toggle surface; mouse-only workflow regression |

## Pre-Spec Commitments

- Change is confined to `Cluiche/CluicheEditor/UI/src/layout/Toolbar.tsx`
- Must preserve existing `EditorBridge.togglePanelVisibility` / `panels_changed` contract — no bridge changes
- No C++ changes (`PanelInfo`, `DockingLayout`, `WebUIBridge` stay as-is)
- Overflow dropdown appears only when pills exceed available width (responsive — works at any window size)
- Active (visible) panels shown with filled blue background; inactive with ghost border — same visual language as today
- `title` attribute on each pill retains full name tooltip (already present in current code)
- Connection indicator and project context button stay in their current positions

## Next Step

Run `/spec-feature` with this candidate as input.
Suggested parent system: CluicheEditor UI (`Cluiche/CluicheEditor/UI/`)
