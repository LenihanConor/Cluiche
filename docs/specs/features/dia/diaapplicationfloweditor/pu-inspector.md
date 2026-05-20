# Feature Spec: PU Inspector

## Parent System
@docs/specs/systems/dia/diaapplicationfloweditor.md

## Traceability

| Level | Spec | Decision IDs honored |
|-------|------|---------------------|
| Platform | @docs/specs/platform/Cluiche.md | PD-001, PD-007 |
| Application | @docs/specs/applications/dia.md | AD-003 |
| System | @docs/specs/systems/dia/diaapplicationfloweditor.md | ED-002, ED-007, ED-008, ED-012 |

## Purpose

Provide an editable sidebar inspector panel that displays and allows modification of ProcessingUnit properties when a PU is selected (from Graph View or other selection sources). Shows frequency, thread configuration, startup order, stage list, and module cards for the selected PU.

## Acceptance Criteria

1. **Sidebar location** — Renders in the inspector sidebar (right side). Adapts based on selection context — shows PU Inspector when a PU is selected.
2. **PU properties** — Displays and allows editing: `instance_id`, `frequency_hz`, `dedicated_thread` (checkbox), startup order (position in array).
3. **Stage list** — Shows which stages exist in the manifest. Indicates which stages this PU participates in (all PUs participate in all stages per SD-004).
4. **Module cards** — Lists all modules belonging to this PU as compact cards: instance_id, type_id, stage badge ("all" or specific), traffic-light dot.
5. **Dependency order section** — Collapsed by default (ED-012). Expands to show module startup dependency order within this PU.
6. **Edit via commands** — All property edits issue commands through Undo/Redo system.
7. **Traffic-light dots** — Module cards use `.tl` dots (ED-008). Grey offline; colored in live mode.
8. **Module card click** — Clicking a module card switches to Module Inspector for that module.
9. **Add Module affordance** — A "+ Add Module" button at the bottom of the Modules section. Click reveals an inline form: `instanceId` text input + `typeId` dropdown populated from `types.get` (TypeDiscoveryService). Enter or "Add" submits `AddModule` command; Escape or "Cancel" discards.
10. **Remove Module affordance** — Each module card shows a `×` button (consistent with the chip-list pattern used for dependencies/reads/writes in Module Inspector). Click issues `RemoveModule` command. ModuleInspector still owns its own destructive "Delete Module" footer button (per `module-inspector.md` AI Review #4) for users coming from that surface.

## Design

### React Component Structure

```
PUInspector (sidebar panel)
├── PUHeader (instance_id, traffic-light dot)
├── PropertiesSection
│   ├── FrequencyField (number input, Hz)
│   ├── ThreadToggle (dedicated_thread checkbox)
│   └── StartupOrderField (number or drag-reorder)
├── StageListSection
│   └── StageTag[] (stage names, informational)
├── ModuleListSection
│   ├── ModuleCard[] (clickable, has × remove button)
│   │   ├── ModuleLabel (instance_id / type_id)
│   │   ├── StageBadge ("all" or stage name)
│   │   ├── TrafficLightDot
│   │   └── RemoveButton (×)
│   └── AddModuleAffordance
│       ├── "+ Add Module" button (collapsed)
│       └── InlineForm (when expanded)
│           ├── InstanceIdInput (text, required, must be unique within PU)
│           ├── TypeIdDropdown (populated from types.get)
│           └── ConfirmButtons (Add / Cancel; Enter / Escape)
└── DependencyOrderSection (collapsed by default, ED-012)
    └── OrderedModuleList (topological sort of dependencies)
```

### Property Editing

Each editable field emits a command on change (debounced for text fields, immediate for toggles):

| Field | Command | Validation |
|-------|---------|-----------|
| frequency_hz | `SetPUFrequencyCommand` | Must be > 0, integer |
| dedicated_thread | `SetPUThreadCommand` | Boolean toggle |
| startup order | `ReorderPUCommand` | Reorders the PU array in manifest |
| add module | `AddModuleCommand` | `instanceId` must be non-empty and unique within the PU; `typeId` must come from TypeDiscoveryService |
| remove module | `RemoveModuleCommand` | Removes the module card and (per `module-inspector.md` AI Review #4) compound-cleans dependencies referring to it via the same path used by ModuleInspector's Delete Module footer |

### Module Cards

Compact cards showing:
- Instance ID (bold)
- Type ID (muted text below)
- Stage badge: green "all" or specific stage name(s)
- `.tl` dot: grey offline, runtime state in live mode

Cards are read-only here — clicking navigates to Module Inspector for full editing.

### Dependency Order

The collapsed section (ED-012) shows a topologically-sorted list of modules within this PU based on their `dependencies` arrays. Visualizes the framework's actual start order. Read-only — dependency editing happens in Module Inspector.

## Tasks

| # | Task | Test | Status | Notes |
|---|------|------|--------|-------|
| 1 | React `PUInspector` component shell with sections | Manual: renders when PU selected | Todo | |
| 2 | Properties section with editable fields | Manual: edit frequency/thread, undo works | Todo | Via Undo/Redo |
| 3 | Module cards with stage badges and .tl dots | Manual: cards render correct data | Todo | |
| 4 | Module card click → switch to Module Inspector | Manual: click navigates | Todo | |
| 5 | Dependency order section (collapsed default) | Manual: expand shows topo-sorted list | Todo | ED-012 |
| 6 | Command integration (SetPUFrequency, SetPUThread, ReorderPU) | Unit test: commands execute/undo correctly | Todo | |
| 7 | "+ Add Module" inline form: instanceId + type dropdown from types.get; submits AddModule | `PUInspector.test.tsx`: empty input → no command; valid → AddModule with puId + instanceId + typeId; Escape cancels | Todo | Reuses existing `AddModuleCommand` and `types.get` |
| 8 | Module card × button issues RemoveModule | `PUInspector.test.tsx`: × calls RemoveModule with puId + instanceId | Todo | Reuses existing `RemoveModuleCommand` |

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for all IDs | PU and module IDs displayed as strings, matched by CRC. |
| PD-007 | C++20 required | Backend command code uses C++20. |
| AD-003 | Namespace Dia::\<Module\>:: | C++ commands in `Dia::ApplicationFlow::Editor::`. |
| ED-002 | Three-tab layout | PU Inspector is in the sidebar, not a tab. Adapts to selection. |
| ED-007 | React + CEF frontend | Implemented in React. |
| ED-008 | Traffic-light dot primitive | Module cards and PU header use `.tl` dots. |
| ED-012 | Dependency Order collapsed by default | Section starts collapsed; expandable on demand. |

## Open Questions

None.

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Startup Order | How does the user change startup order — drag in a list, or a number field? | Number field showing current position (1-based). Changing the number reorders the PU array. Drag-reorder is nice-to-have but not MVP. |
| 2 | Instance ID | Should instance_id be editable (rename PU)? | Yes — editable text field. Rename updates all references (stream from/to, provenance). Implemented as a compound command. |
| 3 | Validation | Should invalid frequency (0 or negative) be blocked at input or flagged as validation error? | Blocked at input — field rejects non-positive integers. No need to surface as async validation error for a simple constraint. |
| 4 | Empty state | What shows when no PU is selected? | "Select a Processing Unit" placeholder text with a hint to click on the Graph View. |

## Status

`Approved` — 2026-05-19
