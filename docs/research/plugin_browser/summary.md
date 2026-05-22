# Research Summary — Plugin Browser Scaling

**Session folder:** docs/research/plugin_browser/
**Date:** 2026-05-22

## One-Line Answer

Replace the flat scrollable Plugin Browser with project-scoped C++ filtering + compact master-detail layout + search bar + status filter chips + command palette load/unload actions — solving both context contamination and discovery at scale with incremental, low-risk changes.

## Journey

1. **Explored:** The Plugin Browser is a panel iframe rendering a flat list of plugins via `getPlugins()`. With 4 plugins it's manageable; at 20–40 (engine plugins + per-game plugins) it breaks on both discovery and context contamination. Two converging problems: flat lists don't scale, and CoW plugins have no business appearing in CluicheTest workspaces.
2. **Ideated:** 8 candidates generated across S–L scope: project-scoped C++ filter, search bar, category grouping, game_id affinity badge, filter chips, pinned section (dropped), plugin packs, and command palette actions.
3. **Evaluated:** C1 (project-scoped filtering) scored highest at 4.45; the compact master-detail layout (surfaced during mockup review) and C8 (command palette) followed. The combination of all five outperforms any single candidate.
4. **Chose:** C1 + Compact+Detail + C2 + C5 + C8 — user confirmed the combination as the right balance of impact vs. cost.

## Chosen Work Item

**Name:** Plugin Browser Scaling — Project Filter + Compact Master-Detail + Search + Chips + Palette
**Home module:** CluicheEditor UI (React/TSX panel) + DiaEditor C++ (`EditorPluginRegistry`)
**Suggested spec type:** Feature (under CluicheEditor system spec)
**Estimated size:** M (C1 = S, layout + search + chips = S, C8 = S; combined effort ~1–2 weeks)

## Key Insights from Exploration

- The project-scope filter boundary already exists in the `.diaapp` → `.cluicheproj` → `.diagame` chain (PD-010); C1 exploits it with no schema changes
- fuse.js is already a dependency (used in `CommandPalette.tsx`); search is nearly free
- The compact master-detail layout scales to 40+ plugins with a fixed-height detail pane; no layout restructure required
- Category grouping (C3) and plugin packs (C7) are the correct next steps once 10+ plugins exist — design the detail pane's action slot to receive them cleanly
- Cold-start state (no project loaded) must show all plugins or a clear prompt — the filter is opt-in via project context
- Command names must use stable StringCRC IDs (PD-001): `plugin.load.application_flow_editor` not display strings

## Discarded Candidates

| Candidate | Why discarded |
|-----------|--------------|
| C3 — Category Grouping | Requires .diaapp schema extension + C++ interface change; premature |
| C4 — game_id Affinity Badge | Overlaps C1 at higher cost; greyed-out model less clean than omission |
| C6 — Pinned Section | Pin UX deferred to profiles pass |
| C7 — Plugin Packs | L-size; overengineered for current scale |

## References

- docs/research/plugin_browser/explore.md
- docs/research/plugin_browser/ideate.md
- docs/research/plugin_browser/evaluate.md
- docs/research/plugin_browser/choose.md
