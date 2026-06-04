# Research: Choice — diaentitytemplate Visual Debugger & Editor Options

**Date:** 2026-05-24
**Chosen candidate:** All remaining candidates — organised into three system specs

## Rationale

All seven candidates (Candidate 1 dropped as redundant to Candidate 2) are desirable and
non-overlapping. User confirmed all should be specced. Candidates are grouped by natural
home system. Build order within each system follows dependency order from the evaluation.

## What Was Ruled Out

| Candidate | Reason not chosen |
|-----------|------------------|
| 1 — In-game ImGui Inspector | Redundant once Candidate 2 (editor panel) exists. Dropped before evaluation. |

## Three System Targets

### System Target 1: DiaVisualDebugger (additions to existing system)

| Feature | Candidate | Size |
|---------|-----------|------|
| `debug-entity-picking` | C3 — Viewport Picking | S |
| `entity-labels-draw-class` | C4 — Viewport Overlay | S |

Build order: C3 first (picking seam), then C4. Combined ≤2 weeks.

### System Target 2: DiaEntityEditor (new system)

| Feature | Candidate | Size |
|---------|-----------|------|
| `entity-inspector-panel` | C2 — Editor Inspector Panel | M |
| `query-browser-tab` | C5 — Query Browser Tab | S |
| `mailbox-traffic-monitor` | C6 — Mailbox Traffic Monitor | M |
| `entity-watch-list` | C8 — Entity Watch List | M |

Build order: C2 → C5 → C6 → C8.

### System Target 3: DiaEntityEntityTemplateEditor (new system)

| Feature | Candidate | Size |
|---------|-----------|------|
| `export-component-schema` (DiaCLI prereq) | — | S |
| `blueprint-file-editor` | C7 — Blueprint JSON Editor | L |

## UI Mockup Decision

**Chosen layout:** Option B — Split panel (master-detail)
**File:** docs/research/diaentit_visual_debug/mockup_b_split.html

- Persistent 30% left column: entity list with search + component-type filter chips, tree-indented hierarchy
- 70% right column: context strip (entity name + tags) + four sub-tabs: **Fields / Queries / Mailbox / Watch**
- Entity list always visible while switching between right-side tabs
- Draggable column divider

This mockup is the visual acceptance gate for all four DiaEntityEditor feature specs.

## Pre-Spec Commitments

- **DiaDebugProtocol** needs new `entity.inspect` message type before DiaEntityEditor can be built
- **IEntityInspectable** will receive additive amendments as DiaEntityEditor features are specced
- **Picking seam** (C3) must be completed before any consumer can test end-to-end entity selection
- **SD-ENT-016 Tier (c) gate**: all three systems render add/remove/create/destroy controls as visually present but disabled in v1
- **Interim mitigation for C7**: `dia pipeline export-component-schema` DiaCLI command before the full editor panel

## Next Steps

1. `/spec-feature` → `debug-entity-picking` inside DiaVisualDebugger ✓ Done
2. `/spec-feature` → `entity-labels-draw-class` inside DiaVisualDebugger ✓ Done
3. `/spec-system` → `DiaEntityEditor` ✓ Done
4. `/spec-feature` → `entity-inspector-panel` inside DiaEntityEditor
5. `/spec-feature` → `query-browser-tab` inside DiaEntityEditor
6. `/spec-feature` → `mailbox-traffic-monitor` inside DiaEntityEditor
7. `/spec-feature` → `entity-watch-list` inside DiaEntityEditor
8. `/spec-system` → `DiaEntityEntityTemplateEditor`
