# Feature Spec: Module Inspector

## Parent System
@docs/specs/systems/dia/diaapplicationfloweditor.md

## Traceability

| Level | Spec | Decision IDs honored |
|-------|------|---------------------|
| Platform | @docs/specs/platform/Cluiche.md | PD-001, PD-007, PD-010 |
| Application | @docs/specs/applications/dia.md | AD-003 |
| System | @docs/specs/systems/dia/diaapplicationfloweditor.md | ED-005, ED-006, ED-007, ED-008, ED-012 |

## Purpose

Provide a detailed sidebar inspector for a selected module. Shows and allows editing of dependencies, stream handles (reads/writes), timeout configuration, stage presence, and provenance (which `.diastage` file the module came from). This is the primary surface for understanding and editing a module's wiring.

## Acceptance Criteria

1. **Sidebar location** — Renders in the inspector sidebar when a module is selected (from PU Inspector card click, Presence Grid row click, or future search).
2. **Identity section** — Displays `instance_id`, `type_id`, owning PU name.
3. **Dependencies** — Editable list of module dependencies (other module instance_ids within same PU). Add/remove via autocomplete.
4. **Stream handles** — Shows `reads` and `writes` arrays. Each entry links to a stream. Add/remove available.
5. **Timeout config** — Editable `start_timeout_ms` and `stop_timeout_ms` fields. Default shown when not explicitly set.
6. **Stage presence dots** — Visual indicator of which stages this module is active in. Uses `.tl` dots per stage (ED-008). Green "all stages" badge for infrastructure modules (ED-005).
7. **Provenance** — If module was imported from a `.diastage` file, shows "from: <filename>" in gold text (ED-006). Modules from the base `.diaapp` show no provenance tag.
8. **Dependency order section** — Collapsed by default (ED-012). Shows this module's position in the PU's dependency chain.
9. **Edit via commands** — All edits issue commands through Undo/Redo.
10. **Traffic-light dot** — Module header has a `.tl` dot for live state (grey offline).

## Design

### React Component Structure

```
ModuleInspector (sidebar panel)
├── ModuleHeader (instance_id, type_id, .tl dot, provenance tag)
├── OwnerSection (PU name, clickable → back to PU Inspector)
├── StagePresenceSection
│   └── StageDots[] (.tl dot per stage, or "all" badge)
├── DependenciesSection
│   ├── DependencyList (instance_ids, removable)
│   └── AddDependency (autocomplete from same-PU modules)
├── StreamHandlesSection
│   ├── ReadsSubsection
│   │   ├── StreamRef[] (stream ID, clickable → Streams tab)
│   │   └── AddRead (autocomplete from manifest streams)
│   └── WritesSubsection
│       ├── StreamRef[] (stream ID, clickable → Streams tab)
│       └── AddWrite (autocomplete from manifest streams)
├── TimeoutSection
│   ├── StartTimeoutField (ms, number input)
│   └── StopTimeoutField (ms, number input)
└── DependencyOrderSection (collapsed by default, ED-012)
    └── PositionInChain (shows upstream/downstream modules)
```

### Commands

| Action | Command | Notes |
|--------|---------|-------|
| Add dependency | `AddModuleDependencyCommand` | Validates no cycle introduced |
| Remove dependency | `RemoveModuleDependencyCommand` | |
| Add read stream | `AddModuleReadCommand` | Stream must exist in manifest |
| Remove read stream | `RemoveModuleReadCommand` | |
| Add write stream | `AddModuleWriteCommand` | Stream must exist in manifest |
| Remove write stream | `RemoveModuleWriteCommand` | |
| Set start timeout | `SetModuleStartTimeoutCommand` | Must be > 0 |
| Set stop timeout | `SetModuleStopTimeoutCommand` | Must be > 0 |
| Change stage assignment | `SetModuleStagesCommand` | "all" or specific stage list |

### Provenance Display

Provenance is derived by checking if the module's parent PU was loaded from a `.diastage` import. The ManifestDocument tracks which file each module definition originated from during load. Displayed as a gold tag: `from: dummy_stage.diastage`.

### Stream Handle Autocomplete

When adding a read/write, the autocomplete shows:
- All streams declared in the manifest
- Filtered to streams where `from` or `to` matches the module's PU
- `$`-prefix streams are excluded from the add list (SD-018: read-only)

Existing `$`-prefix streams in reads/writes are displayed but not removable.

## Tasks

| # | Task | Test | Status | Notes |
|---|------|------|--------|-------|
| 1 | React `ModuleInspector` component shell | Manual: renders when module selected | Todo | |
| 2 | Identity + provenance display | Manual: shows id, type, provenance tag | Todo | ED-006 |
| 3 | Stage presence dots / "all" badge | Manual: correct visual for all vs specific | Todo | ED-005 |
| 4 | Dependencies section with add/remove | Manual: add/remove, autocomplete works | Todo | |
| 5 | Stream handles section with add/remove | Manual: add/remove, autocomplete filtered | Todo | |
| 6 | Timeout fields | Manual: edit, undo works | Todo | |
| 7 | Dependency order section (collapsed) | Manual: expand shows position | Todo | ED-012 |
| 8 | All edit commands (8 command classes) | Unit test: execute/undo for each | Todo | |
| 9 | Cycle detection on dependency add | Unit test: rejects cyclic dependency | Todo | |

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for all IDs | Module/stream/PU IDs matched by CRC. Display as strings. |
| PD-007 | C++20 required | Command code uses C++20. |
| PD-010 | .diagame root, .diastage metadata | Provenance tracks which .diastage file a module came from. |
| AD-003 | Namespace Dia::\<Module\>:: | C++ in `Dia::ApplicationFlow::Editor::`. |
| ED-005 | "all" modules visually distinct | Green "all stages" badge vs individual stage dots. |
| ED-006 | Provenance shown for stage-imported modules | Gold "from: <file>" tag for .diastage-sourced modules. |
| ED-007 | React + CEF frontend | Implemented in React. |
| ED-008 | Traffic-light dot primitive | Header dot + stage presence dots. |
| ED-012 | Dependency Order collapsed by default | Section collapsed; expandable on demand. |

## Open Questions

None.

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Edit | Should editing a module imported from a .diastage be allowed, or should it redirect to edit the source file? | Allowed — the editor works on the merged manifest. Save writes back to the correct source file (the .diastage's .diaapp). |
| 2 | Deps | Should dependency autocomplete filter to only modules in the same PU? | Yes — cross-PU dependencies aren't valid. Only same-PU modules appear. |
| 3 | Streams | Should adding a stream read/write that mismatches PU (stream's from/to doesn't include this module's PU) be blocked or warned? | Warned via real-time validation (separate feature). Module Inspector allows the add — validation catches the mismatch immediately. |
| 4 | Delete | Should there be a "Delete Module" button on this inspector? | Yes — at the bottom, styled as destructive action. Issues a compound command (remove module + remove from other modules' dependencies). Risky Change Warnings feature may intercept. |

## Status

`Approved` — 2026-05-19
