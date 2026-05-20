# Feature Spec: Graph View

## Parent System
@docs/specs/systems/dia/diaapplicationfloweditor.md

## Traceability

| Level | Spec | Decision IDs honored |
|-------|------|---------------------|
| Platform | @docs/specs/platform/Cluiche.md | PD-001, PD-007 |
| Application | @docs/specs/applications/dia.md | AD-003 |
| System | @docs/specs/systems/dia/diaapplicationfloweditor.md | ED-002, ED-007, ED-008, ED-010, ED-011 |

## Purpose

Provide the primary visual representation of the application's ProcessingUnit topology. PUs are rendered as nodes, streams as directed edges between them. The graph is interactive — click to select (populates sidebar inspector), drag to reposition, and startup order is visible via badges. This is the first tab in the three-tab layout (ED-002: Graph, Presence, Streams).

## Acceptance Criteria

1. **PU nodes** — Each ProcessingUnit in the manifest rendered as a node showing: instance_id, frequency, thread type (dedicated/main), and a startup-order badge (1, 2, 3…).
2. **Stream edges** — Directed edges between PU nodes representing streams. Edge label shows stream ID. Arrow indicates `from → to` direction.
3. **Click-select** — Clicking a PU node selects it, populating the PU Inspector sidebar. Clicking empty space deselects.
4. **Drag-reposition** — PU nodes can be dragged to new positions. Positions are stored per-session (not in the manifest). Edges re-route on drag.
5. **Startup order badges** — Each PU node shows its startup order number (array position in config).
6. **Traffic-light dot** — Each PU node has a `.tl` dot (ED-008). Offline: grey. Live mode: green (running), amber (loading), red (failed).
7. **Stream label navigation** — Clicking a stream edge label navigates to the Streams tab with that stream selected. Hover shows hint "→ Streams tab" (ED-010).
8. **Add PU ghost node** — A dashed ghost node on the canvas as the "add PU" affordance (ED-011). Clicking it opens the Add PU dialog.
9. **Delete PU** — Right-click context menu on a PU node includes "Delete PU" (routed through Undo/Redo as a command).
10. **Auto-layout** — Initial layout computed automatically (left-to-right by startup order). Manual drag overrides auto-layout for that node.

## Design

### React Component Structure

```
GraphView (tab content)
├── GraphCanvas (SVG or Canvas2D)
│   ├── PUNode (per PU)
│   │   ├── NodeBody (instance_id, frequency, thread badge)
│   │   ├── StartupBadge (order number)
│   │   └── TrafficLightDot (.tl primitive)
│   ├── StreamEdge (per stream, directed arrow + label)
│   └── GhostNode (dashed "+" affordance)
├── ContextMenu (right-click actions)
└── AddPUDialog (modal on ghost-node click)
```

### Layout Algorithm

Initial layout uses a simple left-to-right (LTR) arrangement:
- Nodes placed by startup order (leftmost = first to start)
- Vertical spacing to avoid overlap
- Stream edges drawn as curved paths (bezier) between node ports

When a user drags a node, its position is pinned. Other nodes retain auto-layout unless also dragged. Positions stored in React component state (session-scoped, not persisted to manifest).

### Selection Model

Single-selection. Clicking a PU node dispatches a `select` action that:
1. Highlights the node (border change)
2. Sends selection to the sidebar inspector context
3. De-selects any previously selected node

Clicking empty canvas area clears selection.

### Commands (Undo/Redo integration)

| Action | Command | Description |
|--------|---------|-------------|
| Add PU | `AddProcessingUnitCommand` | Adds a new PU with default properties |
| Delete PU | `RemoveProcessingUnitCommand` | Compound: removes PU + its stream connections + its modules |

Node drag is cosmetic (session state) — not an undoable command.

### Frontend Communication

- `editor.manifest.getState()` — provides PU list and stream list for rendering
- `editor.manifest.execute({ type: "addPU", ... })` — issues commands via Undo/Redo system
- `onManifestChanged` — re-render graph when model changes

### Live Mode Overlay

When connected (separate Live Connection feature), the traffic-light dots animate:
- Grey → green (running), amber+pulse (loading/transition), red (failed)
- Applied per-PU based on live state data

This feature renders the dot but only shows grey in offline mode. Live mode activation handled by Live State Overlay feature.

## Tasks

| # | Task | Test | Status | Notes |
|---|------|------|--------|-------|
| 1 | React `GraphCanvas` component with SVG rendering | Manual: renders empty canvas | Todo | Foundation |
| 2 | `PUNode` component with labels, badges, traffic-light dot | Manual: nodes render with correct data | Todo | |
| 3 | `StreamEdge` component with directed arrows and labels | Manual: edges connect correct nodes | Todo | |
| 4 | Auto-layout algorithm (LTR by startup order) | Unit test: positions computed correctly | Todo | |
| 5 | Click-select with inspector integration | Manual: click selects, sidebar updates | Todo | |
| 6 | Drag-reposition with edge re-routing | Manual: drag moves node, edges follow | Todo | |
| 7 | Ghost node (Add PU) + AddPUDialog | Manual: ghost visible, dialog creates PU | Todo | |
| 8 | Delete PU context menu + compound command | Unit test: delete removes PU + streams + modules | Todo | Via Undo/Redo |
| 9 | Stream label click → navigate to Streams tab | Manual: click navigates, hover shows hint | Todo | ED-010 |

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for all IDs | PU instance IDs displayed as human-readable strings (resolved from CRC). Selection uses StringCRC matching. |
| PD-007 | C++20 required | Backend code (if any) uses C++20. Graph View is primarily React/JS. |
| AD-003 | Namespace Dia::\<Module\>:: | Any C++ support code in `Dia::ApplicationFlow::Editor::`. |
| ED-002 | Three-tab layout | Graph View is the first tab. |
| ED-007 | React + CEF frontend | Implemented entirely in React. |
| ED-008 | Traffic-light dot primitive | Each PU node includes a `.tl` dot. Grey offline, colored in live mode. |
| ED-010 | Stream labels navigate to Streams tab | Click on stream edge label switches to Streams tab with that stream selected. |
| ED-011 | Add PU is a dashed ghost node | Ghost node on canvas, not a toolbar button. |

## Open Questions

None.

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Layout | Should node positions persist across sessions (e.g., in a `.diaapp.layout` sidecar file)? | No — session-scoped only. Manifests change structure; stale positions would cause confusion. Auto-layout is good enough for fresh opens. |
| 2 | Scale | How should the graph handle 10+ PUs? | Pan and zoom on canvas. Scroll-wheel zooms, middle-mouse-drag pans. Minimap is overkill for <20 nodes. |
| 3 | Edges | Should stream edges show direction via arrowhead or animation? | Arrowhead on the `to` end. Static — no animation in offline mode. Live mode could add flow animation (future, not this spec). |
| 4 | Delete | Should deleting a PU with active modules warn the user? | Yes — handled by the Risky Change Warnings feature (separate spec). This feature emits the delete command; that feature intercepts and warns. |

## Status

`Approved` — 2026-05-19
