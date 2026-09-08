# Research: Ideate — Editor Taskbar Navigation

**Input:** docs/research/editor_tab_nav/explore.md

## Candidates

### Candidate 1: Full-Label Pill Bar with Overflow Menu
**Home module/system:** CluicheEditor UI — `Toolbar.tsx`
**Size:** S
**Description:** Replace the single-character buttons with full-name pill buttons (`min-width: fit-content`, `padding: 0 10px`). When the total width exceeds the toolbar, overflow pills collapse into a `⋯` dropdown button on the right that lists the remaining panels. No C++ changes needed — `PanelInfo.name` is already the full string.

This is the lowest-risk, smallest-scope fix. It directly solves the label collision and readability problems while keeping the existing toggle-visibility model, bottom toolbar position, and `EditorBridge` contract intact. The overflow menu is a standard HTML `position: absolute` dropdown, no extra library required.

**Primary value:** Panels are readable and the bar works at any plugin count without layout restructuring.

---

### Candidate 2: Abbreviated Label + Tooltip (3-char truncation)
**Home module/system:** CluicheEditor UI — `Toolbar.tsx`
**Size:** S
**Description:** Truncate each panel name to 3 characters (`"Gam"`, `"Out"`, `"Plu"`), display in the same 24px-tall bar, and rely on the existing `title` attribute (already present) for the full name on hover. Add a thin underline or left-border accent when visible. No overflow needed for ≤15 panels in a typical editor window.

This is a cosmetic patch — it solves collision (3 chars is usually unique) and gives more hint than one letter without changing the bar size or any structural code. It does not solve the problem at large plugin counts; it only defers it.

**Primary value:** Minimal diff, still compact, eliminates most label collisions today.

---

### Candidate 3: Left Activity Bar (VS Code–style)
**Home module/system:** CluicheEditor UI — `DockingManager.tsx` + `Toolbar.tsx` → new `ActivityBar.tsx`
**Size:** M
**Description:** Move panel navigation from the bottom horizontal strip to a 40px-wide vertical column on the left edge of the shell. Each entry is a square icon zone: a generated letter-avatar (coloured circle with initial) or future custom icon, with a tooltip showing the full name. Active panels get a left-edge accent bar. The `DockingManager` flex layout changes from `column` to `row` (activity bar left, mosaic right).

This is the VS Code model. It scales to 20+ panels naturally (vertical scroll if needed), adds zero horizontal cost, and leaves room for future icon assets without requiring them now (letter avatar as fallback). It is a moderate layout restructure — the toolbar strip is replaced, not patched.

**Primary value:** Unlimited scale, zero horizontal cost, clear visual identity per panel, familiar to developers who use VS Code.

---

### Candidate 4: Popover Panel Picker ("+" Button)
**Home module/system:** CluicheEditor UI — `Toolbar.tsx`
**Size:** S
**Description:** Replace all per-panel buttons with a single `⊞ Panels` button. Clicking it opens a floating popover listing all registered panels with a checkbox (visible/hidden) and full name. A search/filter input handles large lists. The toolbar retains the connection indicator; the panel buttons disappear entirely.

This frees the toolbar from any scaling concern — the popover handles arbitrary counts. The tradeoff is discoverability: the user must open the popover to see which panels are available. A compact "active panels" chip row (showing only currently visible panel names, read-only) could sit beside the button to restore at-a-glance awareness.

**Primary value:** Decouples panel count from toolbar width entirely; cleanest toolbar surface.

---

### Candidate 5: Grouped Sections with Separators
**Home module/system:** CluicheEditor UI — `Toolbar.tsx` + `PanelInfo` (C++ + bridge)
**Size:** M
**Description:** Add a `group` field to `PanelInfo` (e.g. `"tool"`, `"debug"`, `"game"`). The toolbar renders groups as labelled clusters separated by a thin vertical rule: `[Home] | [Output · Console] | [GameConn] | [AppFlow · Stages]`. Group labels appear as dim uppercase headings above or inline. Panels within a group use abbreviated names.

This requires a 1-line struct change in `DockingLayout::PanelInfo` (C++) plus bridge serialization. It adds semantic organisation that becomes valuable once the editor has 10+ panels across distinct domains. For the current 5-panel set it is overengineered, but it creates a foundation for the full editor vision.

**Primary value:** Semantic grouping makes the toolbar self-documenting as the plugin library grows.

---

### Candidate 6: Tab Strip with Open/Close State
**Home module/system:** CluicheEditor UI — `Toolbar.tsx`
**Size:** S
**Description:** Treat the toolbar as a tab strip rather than a toggle bar. Panels that are currently open in the mosaic appear as tabs with full names and a `×` close affordance. Hidden panels disappear from the strip entirely. A `+` button at the right opens a flat list of all registered-but-hidden panels to reopen them.

This mirrors a browser/IDE tab bar. It changes the mental model: the toolbar is "what is open" rather than "what exists". It aligns naturally with the mosaic's add/remove behaviour — closing a mosaic tile also removes its tab, opening a panel adds one. The tradeoff is that the strip width grows with the number of open panels.

**Primary value:** Toolbar directly mirrors what is on screen; tab metaphor is universally understood.

---

### Candidate 7: Icon + Label Sidebar (full identity)
**Home module/system:** CluicheEditor UI — `DockingManager.tsx` → new `Sidebar.tsx` + `PanelInfo` icon field (C++ + bridge)
**Size:** M
**Description:** A 120px-wide left sidebar showing icon (or letter avatar) + full panel name per entry, stacked vertically. Plugins can optionally register an SVG icon path via a new `iconPath` field in `PanelInfo`; absent icons fall back to a coloured letter avatar generated from the panel name's CRC. This is the JetBrains / Rider side-nav model — higher information density than the activity bar, though wider.

Requires the same C++ struct change as Candidate 5 (icon field), plus a new sidebar component. The 120px cost is significant when the editor window is narrow, so the sidebar should be collapsible to icon-only (40px) mode.

**Primary value:** Maximum label clarity per panel; natural home for future rich plugin metadata (badge counts, status indicators).

---

### Candidate 8: Command Palette as Primary Nav (keyboard-first)
**Home module/system:** CluicheEditor UI — `CommandPalette.tsx` (already exists)
**Size:** S
**Description:** Extend the existing `CommandPalette.tsx` so that typing `> panel` surfaces toggle-panel commands for every registered panel. The current bottom toolbar shrinks to a minimal status bar (connection dot + project name only). Panel navigation is entirely keyboard-driven via `Ctrl+Shift+P`. A secondary "quick pick" shortcut (`Ctrl+P` or similar) could show only panel commands.

`CommandPalette.tsx` already exists and already handles commands. This is a wiring task: on `panels_changed`, register/unregister `"Toggle: <panel name>"` commands into the palette. No toolbar redesign, no C++ changes. The tradeoff is that mouse-only workflows have no panel switcher at all — a real regression for a graphical editor.

**Primary value:** Zero scaling concern; fully aligns with keyboard-first developer workflow; builds on existing infrastructure.

---

## Coverage Map

The eight candidates span the full design-axis range from explore.md:

- **Minimal patch** (no layout change, no C++ change): Candidates 1, 2, 8
- **Moderate restructure** (layout change, no C++ change): Candidates 3, 4, 6
- **Full redesign** (layout change + C++ PanelInfo extension): Candidates 5, 7
- **Scope range**: S×5, M×3, no L or XL — all are buildable quickly
- **Orientation coverage**: horizontal bar (1,2,4,6,8), vertical sidebar (3,7), grouped hybrid (5)
- **Label density coverage**: abbreviated (2), full label (1,4,5,6,7,8), icon+label (7)
- **Overflow strategy coverage**: dropdown (1), popover (4), no overflow needed (3,7), tab strip (6)
