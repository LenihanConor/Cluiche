# Feature Spec: Module Presence Grid

## Parent System
@docs/specs/systems/dia/diaapplicationfloweditor.md

## Traceability

| Level | Spec | Decision IDs honored |
|-------|------|---------------------|
| Platform | @docs/specs/platform/Cluiche.md | PD-001, PD-007 |
| Application | @docs/specs/applications/dia.md | AD-003 |
| System | @docs/specs/systems/dia/diaapplicationfloweditor.md | ED-001, ED-002, ED-005, ED-007, ED-008, ED-014 |

## Purpose

Provide a full-tab matrix view showing which modules are active in which stages, organized by ProcessingUnit. This is the definitive "who runs where" visualization — at a glance you see infrastructure modules (all stages) vs. stage-specific modules, spot coverage gaps, and in live mode see runtime state per-module.

## Acceptance Criteria

1. **Full-tab layout** — Grid occupies the entire tab content area (ED-001). Not a sidebar panel.
2. **Matrix structure** — Rows = modules (grouped by PU), Columns = stages. Cell indicates whether module is active in that stage.
3. **"all" distinction** — Modules with `stages: ["all"]` show a green "all stages" badge spanning all columns, visually distinct from stage-specific dots (ED-005).
4. **Per-PU grouping** — Rows grouped by ProcessingUnit with collapsible section headers. PU name as group header.
5. **Traffic-light dots** — Cells use the `.tl` dot primitive (ED-008). Offline mode: green = required in that stage, grey = not assigned.
6. **Two visual modes (ED-014):**
   - **Offline** — All columns: green dot = module assigned to stage, grey = not assigned.
   - **Live** — Active-stage column: green/amber/red+pulse = runtime state. Inactive columns: outline-green = required, grey = not assigned. Active-stage column header highlighted.
7. **Virtual scrolling** — Handles 50+ modules via virtual scrolling for rows (system AI Review Q3).
8. **Sticky headers** — Column headers (stage names) remain visible during vertical scroll.
9. **Per-PU filtering** — Toolbar filter to show all PUs or a single PU's modules.
10. **Click-select** — Clicking a module row selects it, populating the Module Inspector sidebar.

## Design

### React Component Structure

```
ModulePresenceGrid (tab content)
├── GridToolbar
│   ├── PUFilter (dropdown: All / MainPU / SimPU / RenderPU)
│   └── LiveModeIndicator (reflects connection state)
├── GridHeader (sticky)
│   └── StageColumn[] (stage names, active-stage highlighted in live mode)
├── GridBody (virtual-scrolled)
│   └── PUGroup[] (collapsible)
│       ├── PUGroupHeader (PU name, expand/collapse)
│       └── ModuleRow[]
│           ├── ModuleLabel (instance_id, type_id)
│           ├── AllBadge (if stages=["all"], spans columns)
│           └── StageCell[] (traffic-light dot per stage)
└── (sidebar) ModuleInspector (populated on row click)
```

### Cell Rendering Logic

```
if module.stages includes "all":
    render green "all stages" badge spanning all stage columns
else:
    for each stage column:
        if stage in module.stages:
            offline → green dot
            live + active stage → runtime state dot (green/amber/red with pulse)
            live + inactive stage → outline-green dot
        else:
            grey dot (not assigned)
```

### Virtual Scrolling

Uses a windowed rendering approach (e.g., react-window or custom):
- Only renders rows visible in the viewport + small overscan buffer
- Row height fixed for predictable scroll position calculation
- Grouped sections collapse to just the header row

### Selection

Clicking a `ModuleRow` dispatches selection:
1. Highlights the row
2. Sends `{ puId, moduleId }` to the sidebar context → Module Inspector renders

### Live Mode Integration

This feature renders the offline grid by default. When Live State Overlay feature is active:
- The grid receives runtime module states per tick
- Active-stage column header gets a highlight class
- Cells in active-stage column animate `.tl` dots with runtime state
- Inactive columns switch to outline-green style for assigned modules

## Tasks

| # | Task | Test | Status | Notes |
|---|------|------|--------|-------|
| 1 | React `ModulePresenceGrid` component with static grid layout | Manual: renders matrix with correct dimensions | Todo | |
| 2 | PU grouping with collapsible sections | Manual: groups collapse/expand | Todo | |
| 3 | Cell rendering: green/grey dots + "all" badge | Manual: visual distinction correct | Todo | ED-005 |
| 4 | Virtual scrolling for large module counts | Manual: smooth scroll with 50+ rows | Todo | |
| 5 | Sticky column headers | Manual: headers visible during scroll | Todo | |
| 6 | PU filter toolbar | Manual: filter shows/hides PU groups | Todo | |
| 7 | Click-select → Module Inspector integration | Manual: click populates sidebar | Todo | |
| 8 | Live mode visual switch (offline vs live cell rendering) | Manual: dots change style when live | Todo | ED-014 |

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for all IDs | Module/stage/PU names displayed as strings, matched by CRC internally. |
| PD-007 | C++20 required | Backend code uses C++20. Grid is primarily React/JS. |
| AD-003 | Namespace Dia::\<Module\>:: | Any C++ support code in `Dia::ApplicationFlow::Editor::`. |
| ED-001 | Presence Grid is full tab | Grid occupies entire tab area — not a sidebar panel. |
| ED-002 | Three-tab layout | Module Presence Grid is the second tab (Presence). |
| ED-005 | "all" modules visually distinct | Green "all stages" badge vs. individual stage dots. |
| ED-007 | React + CEF frontend | Implemented in React. |
| ED-008 | Traffic-light dot primitive | All cells use the `.tl` dot (grey/amber/green/red + pulse). |
| ED-014 | Two visual modes (Offline/Live) | Offline: green/grey. Live: runtime state in active column, outline-green in inactive. |

## Open Questions

None.

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Scale | What's the maximum realistic number of stages (columns)? | ~10 stages max for any real game. Horizontal scroll if needed, but unlikely to be necessary. |
| 2 | Edit | Can users toggle module stage assignment directly in the grid (click cell to add/remove)? | No — grid is read-only visualization. Stage assignment edited via Module Inspector sidebar. Keeps the grid simple and avoids accidental clicks. |
| 3 | Sort | Should module rows within a PU group be sortable (alphabetical, dependency order)? | Default: config array order (dependency order). No user sort — config order is the meaningful order. |
| 4 | Badge | Should "all" modules show individual dots in addition to the badge? | No — just the green badge spanning all columns. Individual dots would be redundant and noisy. |

## Status

`Approved` — 2026-05-19
