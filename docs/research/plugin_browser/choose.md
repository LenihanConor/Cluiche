# Research: Choice — Plugin Browser Scaling

**Date:** 2026-05-22
**Chosen candidate:** C1 + Compact+Detail layout + C2 + C5 + C8

## Rationale

The combination was chosen for maximum impact at minimum risk. C1 (project-scoped filtering) eliminates context contamination at the C++ layer with zero UI work — the `.diaapp` manifest boundary already exists and is the correct architectural seam. The compact master-detail layout solves the scaling problem for 40+ plugins with a pure React change. C2 (search) and C5 (filter chips) are both additive, pure-UI improvements that reuse existing fuse.js infrastructure. C8 (command palette actions) adds a keyboard-first path for power users at ~half a day of C++ work.

Collectively the combination scores highest across the evaluation and defers all schema-heavy work (C3 categories, C7 packs) until the plugin count actually demands it.

## What Was Ruled Out

| Candidate | Reason not chosen |
|-----------|------------------|
| C3 — Category Grouping | Requires .diaapp schema extension + C++ interface change; premature until 10+ plugins exist |
| C4 — game_id Affinity Badge | Overlaps C1 at higher cost; "show greyed out-of-scope plugins" model less clean than omitting them |
| C6 — Pinned Section | Dropped earlier — pin UX deferred to profiles pass |
| C7 — Plugin Packs | L-size; right long-term direction but overengineered for current scale |
| C8 alone | Orthogonal improvement; chosen as part of the combination, not as a standalone |

## Pre-Spec Commitments

- C1 filter activates only when a `.cluicheproj` is loaded; cold-start shows all plugins
- Pinned plugins from out-of-scope manifests: omit silently (pin is being deferred anyway)
- Compact+Detail layout: tight rows (name + load/unload button), persistent detail pane at bottom; detail pane height fixed initially, resizable later
- Search (C2): client-side fuse.js over the filtered plugin list; no new bridge calls
- Filter chips (C5): status chips only for now (`All | Loaded | Available`); category chips deferred to C3
- Command palette (C8): `plugin.load.<id>` and `plugin.unload.<id>` commands; StringCRC IDs per PD-001
- No pin actions in this feature — deferred

## Next Step

Run `/spec-feature` with this candidate as input.
Suggested parent system: CluicheEditor — Plugin Browser panel + DiaEditor C++ backend
