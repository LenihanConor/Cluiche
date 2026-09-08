# Research: Evaluate — Plugin Browser Scaling

**Input:** docs/research/plugin_browser/ideate.md

## Scoring Criteria

- **Engine Value** (0.25): Improves DiaEditor/plugin infrastructure reusability or capability
- **Game Value** (0.20): Improves the CluicheEditor experience for developers working on Cluiche games
- **Implementation Cost** (0.25): Inverse of effort — 5 = very cheap, 1 = very expensive
- **Risk** (0.15): Inverse of uncertainty — 5 = well-understood, 1 = highly uncertain
- **Cluiche Fit** (0.15): Aligns with module structure, AED-002/PD-010 manifest patterns, and future-profile compatibility

## Scores

| Candidate | Engine (0.25) | Game (0.20) | Cost (0.25) | Risk (0.15) | Fit (0.15) | Total |
|-----------|---------------|-------------|-------------|-------------|------------|-------|
| C1 — Project-Scoped Filtering | 4 | 5 | 4 | 5 | 5 | **4.45** |
| C2 — Search Bar | 2 | 4 | 5 | 5 | 4 | **3.80** |
| C3 — Category Grouping | 4 | 4 | 3 | 3 | 4 | **3.60** |
| C4 — game_id Affinity + Badge | 3 | 4 | 2 | 3 | 4 | **3.15** |
| C5 — Filter Chips | 2 | 4 | 5 | 5 | 4 | **3.80** |
| C6 — Pinned Section (dropped) | 1 | 2 | 5 | 5 | 2 | **2.75** |
| C7 — Plugin Packs | 5 | 4 | 1 | 2 | 5 | **3.30** |
| C8 — Command Palette Actions | 3 | 4 | 4 | 5 | 5 | **3.95** |
| **Compact + Detail layout** | 2 | 5 | 4 | 5 | 4 | **3.85** |

> Note: C6 is scored for completeness but has been dropped from consideration by user decision. "Compact + Detail layout" is scored as a distinct UI candidate surfaced during mockup review — not in the original ideate list but confirmed as the chosen layout.

## Top 3 Candidates

### Rank 1: C1 — Project-Scoped Filtering (score: 4.45)
**Why:** The highest-leverage single change in the list. Zero UI work — C++ simply omits plugins whose `.diaapp` manifest is not reachable from the active `.cluicheproj`. Solves context contamination completely for the common case (CoW plugins never appear in CluicheTest). Aligns perfectly with PD-010 (`.diagame` is the project root) and AED-002 (plugins declared in `.diaapp`). Risk is minimal — the filter boundary is the same manifest chain that already exists.
**Watch out for:** Cold-start state (no project loaded) must show all plugins or a clear "open a project first" prompt. Pinned plugins from out-of-scope manifests need a defined behaviour (show greyed? omit silently?). Since pin is being dropped, the second concern is moot.

### Rank 2: Compact + Detail layout (score: 3.85)
**Why:** Confirmed by mockup review as the preferred layout. Tight rows (name + load/unload button) with a persistent detail pane at the bottom eliminates the scrolling wall without hiding information. Scales to 40+ plugins without restructuring the component. The detail pane provides a natural home for future metadata (category, deps, profile actions) as they arrive. Pure React/TypeScript — no C++ changes.
**Watch out for:** Detail pane height needs to be user-resizable or at a comfortable fixed height; too short and it clips the deps/actions row. Panel iframe height will vary by dock position — test in a narrow dock.

### Rank 3: C8 — Command Palette Plugin Actions (score: 3.95)
**Why:** fuse.js and the command registry are already wired. Adding `plugin.load`, `plugin.unload` commands to C++ and surfacing them in the palette costs ~half a day of C++ work and zero UI work. Keyboard-first developers never need to open the Plugin Browser for known plugins. Completely orthogonal — stacks with every other candidate and makes the whole system feel polished.
**Watch out for:** Command names must be stable StringCRC IDs (PD-001) — `plugin.load.application_flow_editor` not a display string. Autocomplete in the palette needs the plugin list at palette-open time (one extra `getPlugins()` call or a cached push).

## Recommendation

The chosen combination is **C1 + Compact+Detail + C2 + C5 + C8** — project-scoped filtering at the C++ layer, compact master-detail layout in the panel UI, search + status chips for in-list navigation, and palette commands for keyboard access. C6 (pinned section) is dropped; Pin action is deferred to the profiles pass.

This combination respects PD-010 (`.diagame` as project root drives C1 filtering), AED-002 (plugins declared in `.diaapp` manifests), and leaves the detail pane's action slot clean for profile integration. C3/C7 category grouping is the natural next step once 10+ plugins exist — the compact list's section-label element is already in place to receive grouped rendering without layout changes.
