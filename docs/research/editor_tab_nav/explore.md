# Research: Explore — Editor Taskbar Navigation

**Session date:** 2026-05-21
**Folder:** docs/research/editor_tab_nav/

## Problem Space Overview

The CluicheEditor taskbar (`Toolbar.tsx`) renders one 24×22 px button per registered panel. The label is `p.name.charAt(0)` — a single capital letter. This works today with 5 panels (H, O, G, P, A) but has no room to grow: a 6th panel may share an initial letter, there is no label, and there is no overflow handling. The user must already know what H/O/G/P/A mean; a new contributor cannot.

The editor is a dynamic plugin host. Plugins self-register via `DockingLayout::RegisterPanel()`, so the number and names of panels are not fixed at compile time. The toolbar must survive plugins being added, removed, or renamed at runtime. The current design hardcodes one narrow affordance per panel with no visual identity beyond a single character.

The fix is purely a React/CSS concern — `Toolbar.tsx` is ~105 lines of TSX, no C++ changes are needed. The design space is wide: from minimal label expansion through to a sidebar nav with icons, overflow menus, or grouped sections. The right answer should stay ergonomic for 5–20 panels and fit the existing VS Code–dark aesthetic used throughout the shell.

## Existing Approaches

Industry tab/toolbar navigation patterns observed in comparable tools:

- **Icon-only sidebar** (VS Code activity bar): 48px wide column of icons, tooltip on hover, no label; scales to 20+ items; requires an icon set
- **Icon + label sidebar** (JetBrains toolbox, Rider side nav): vertical text label below icon; readable, but takes more space
- **Horizontal pill tabs with full labels** (Chrome DevTools, browser tabs): auto-shrink with ellipsis or `+N more` overflow; familiar but wastes vertical space
- **Horizontal abbreviated tabs** (current): first letter only; fails on name collision, no discoverability
- **Dropdown/hamburger overflow** (toolbar overflow pattern): fixed set of "pinned" primary items, extras in a `⋯` dropdown; used by Office ribbon, VS toolbar
- **Grouped sections** (Unreal Editor tabs): related panels grouped under a section header; good for large sets
- **Command palette as primary nav** (VS Code ⇧⌘P): nav via search rather than visual buttons; pair it with a minimal persistent bar for pinned items
- **Popover panel picker** (Unity editor): a "+" button reveals a searchable list; panels toggle on/off; good for optional panels
- **Tab strip with close buttons** (editors/IDEs generally): tabs appear when panel is open, disappear when closed; panel can be "hidden" but stays registered

## Design Axes

| Axis | Options | Notes |
|------|---------|-------|
| **Orientation** | Horizontal bar (current), Vertical sidebar | Sidebar scales to more items; horizontal saves vertical px |
| **Label density** | None, Initial, Abbreviated (3–5 chars), Full name | Current = Initial; full name is most readable |
| **Overflow strategy** | None (current), Wrap, Scroll, `+N more` dropdown, Popover picker | None breaks past ~10 panels |
| **Panel identity** | Letter, Emoji/icon, Custom icon per plugin | Icons require an icon story; letters are trivial to generate |
| **Selection model** | Toggle visibility (current), Active focus (only one visible), Mix | Current model is toggle — any subset can be visible simultaneously |
| **Pinning / ordering** | Static (current), User-draggable, Plugin-declared priority | Current is insertion order; no user control |
| **Grouping** | None (current), By plugin type (tool/debug/game), By user group | Useful above ~10 panels |
| **Height budget** | 28px (current), 40px (more room for labels), Full sidebar | Sidebar is zero height cost |

## Known Tradeoffs

- Full labels are most readable but constrain horizontal space — more panels = narrower buttons or overflow
- Icons are visually distinctive but require a maintained icon set; no icon library is currently in the stack
- A vertical sidebar takes zero horizontal space but is a bigger layout change (the current shell is a column-flex with toolbar at the bottom)
- Overflow dropdowns are discoverable but add a two-click path for panels that are "off screen"
- Command palette navigation is powerful but non-visual — doesn't replace a persistent toggle surface
- Tooltip-only identity (no label) requires memorisation but keeps buttons compact
- Mixing "always-visible pinned" + "rest in overflow" matches VS Code/browser UX norms but adds pinning state to manage

## Known Pitfalls (C++ / game engine context)

- Not applicable here — the toolbar is pure React TSX, no C++ involvement
- Plugin names come from `PanelInfo.name` (char[64] in C++) via JSON bridge — must not assume names are short or unique initials
- Any icon solution that requires a separate asset file per plugin creates a dependency on plugin authors supplying assets; a fallback strategy (auto-generated letter avatar) is essential
- Saving/restoring panel visibility state already goes through `EditorBridge` — nav design must not break the existing `togglePanelVisibility` / `panels_changed` contract

## Cluiche-Specific Opportunities

### Relevant Existing Modules

| Module | Relevance |
|--------|-----------|
| DiaEditor (C++) | Owns `DockingLayout` — `PanelInfo` carries `name` and `uiPath`; no icon field currently |
| CluicheEditor UI (React/TSX) | `Toolbar.tsx` + `DockingManager.tsx` — all changes live here |
| EditorBridge | JSON bridge; `panels_changed` event delivers `PanelInfo[]` — schema change needed if icons are added |
| WebUIBridge (C++) | Passes `PanelInfo` as JSON; adding an `icon` field is a 1-line struct + serialize change |

### Platform Decision Constraints

| Decision | Implication for this topic |
|----------|---------------------------|
| PD-001 StringCRC | Panel identity on the C++ side uses StringCRC; not directly relevant to the visual nav |
| PD-004 No STL in public APIs | `PanelInfo` uses `char[]` arrays, not `std::string` — any new icon field must follow the same pattern |
| PD-006 VS project files are source of truth | No build impact; this is pure TSX/CSS |
| PD-007 C++20 required | No impact on TSX layer |

## Open Questions for Ideation

- Should the nav stay at the bottom (toolbar) or move to a left sidebar?
- Is the goal only to fix the label problem, or also to improve discoverability and grouping?
- Should panel order be user-customisable, or is plugin-declared order sufficient?
- Is adding an icon field to `PanelInfo` (C++ + bridge change) in scope, or should the solution work with names only?
- Should "all panels always reachable in one click" remain a hard requirement, or is a two-click overflow acceptable?
- Should hidden panels still appear in the nav (greyed) or disappear entirely?
