# Feature Spec: Stage Configuration

## Parent System
@docs/specs/systems/dia/diaapplicationfloweditor.md

## Traceability

| Level | Spec | Decision IDs honored |
|-------|------|---------------------|
| Platform | @docs/specs/platform/Cluiche.md | PD-001, PD-007, PD-010 |
| Application | @docs/specs/applications/dia.md | AD-003 |
| System | @docs/specs/systems/dia/diaapplicationfloweditor.md | ED-007 |
| System (upstream) | @docs/specs/systems/dia/diaapplicationflow.md | SD-001, SD-002, SD-004, SD-005 |

## Purpose

Provide UI for managing stage declarations in the manifest: add/remove stages, configure auto vs manual trigger, set initial stage, and reorder stages. Stages are the application's runtime modes — this feature lets developers define which stages exist and how they transition.

## Acceptance Criteria

1. **Stage list** — Display all declared stages with their properties: name, trigger type (auto/manual), whether it's the initial stage.
2. **Add stage** — Dialog to create a new stage: name (unique), trigger type.
3. **Remove stage** — Delete a stage. Compound command: removes stage + unassigns all modules that reference only this stage (or removes stage from multi-stage modules' lists).
4. **Set trigger type** — Toggle between auto (advances automatically when all modules ready) and manual (requires explicit TransitionTo call).
5. **Set initial stage** — Dropdown or radio to designate which stage is entered first.
6. **Reorder stages** — Drag or number field to reorder the `stages` array (ordering affects display, not runtime behavior beyond initial_stage).
7. **Auto-stages list** — Show which stages are marked auto-advance. Toggling trigger type updates this list.
8. **Edit via commands** — All edits through Undo/Redo.

## Design

### React Component Structure

```
StageConfiguration (sidebar section, shown contextually or as dedicated panel)
├── StageListHeader
│   └── AddStageButton → AddStageDialog
├── StageList (reorderable)
│   └── StageItem[]
│       ├── StageName (editable)
│       ├── TriggerBadge ("auto" green / "manual" grey)
│       ├── InitialBadge ("initial" if this is initial_stage)
│       ├── SetInitialButton (or radio)
│       └── DeleteButton
└── StageOrderNote ("Ordering affects display. Initial stage determines entry point.")
```

### Access Points

Stage Configuration appears in the sidebar when:
- User clicks "Stages" section in PU Inspector
- A dedicated "Stage Configuration" panel option is available from a menu/toolbar
- Context: available regardless of Graph/Presence/Streams tab since stages are global

### Commands

| Action | Command | Notes |
|--------|---------|-------|
| Add stage | `AddStageCommand` | Name must be unique, non-empty |
| Remove stage | `RemoveStageCommand` | Compound: removes from stages array + auto_stages + module stage assignments |
| Rename stage | `RenameStageCommand` | Updates all module stage references |
| Set trigger type | `SetStageTriggerCommand` | Adds/removes from auto_stages array |
| Set initial stage | `SetInitialStageCommand` | Updates initial_stage field |
| Reorder stages | `ReorderStagesCommand` | Reorders stages array |

### Validation Integration

Real-time validation (separate feature) will flag:
- Stages with no modules assigned (warning: empty stage)
- Modules referencing a non-existent stage (error)
- initial_stage not in stages array (error)

This feature ensures the model stays consistent via compound commands (e.g., rename updates all references).

## Tasks

| # | Task | Test | Status | Notes |
|---|------|------|--------|-------|
| 1 | React `StageConfiguration` panel | Manual: renders stage list | Todo | |
| 2 | Add Stage dialog with validation | Manual: creates stage, blocks duplicates | Todo | |
| 3 | Remove Stage compound command | Unit test: removes stage + cleans module refs | Todo | |
| 4 | Rename Stage compound command | Unit test: updates all module stage arrays | Todo | |
| 5 | Trigger type toggle (auto/manual) | Manual: toggle updates auto_stages | Todo | |
| 6 | Set initial stage | Manual: radio/dropdown changes initial_stage | Todo | |
| 7 | Reorder stages (drag or number) | Manual: reorder updates stages array | Todo | |

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for all IDs | Stage names stored as StringCRC. |
| PD-007 | C++20 required | Command code uses C++20. |
| PD-010 | .diagame root, .diastage metadata | Stage names align with .diastage file declarations. Editor respects the hierarchy. |
| AD-003 | Namespace Dia::\<Module\>:: | C++ in `Dia::ApplicationFlow::Editor::`. |
| ED-007 | React + CEF frontend | Implemented in React. |
| SD-001 | Config is sole source of truth | Stages defined in manifest — editor edits config. |
| SD-002 | Stages replace Phases | Editor manages stages, not phases. |
| SD-004 | TransitionTo is app-wide | All PUs participate in all stages — stage config is global, not per-PU. |
| SD-005 | Transitions async, queued | Editor shows trigger type (auto/manual) which determines how transitions fire. |

## Open Questions

None.

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Naming | Should stage names have character restrictions (alphanumeric only, no spaces)? | Alphanumeric + underscore. No spaces, no special chars. Same rules as C++ identifiers since they become StringCRC keys. |
| 2 | Delete | Should deleting the initial_stage be blocked or should it auto-promote another stage? | Blocked — user must first change initial_stage to another stage before deleting. Clear error message. |
| 3 | Minimum | Should there always be at least one stage? | Yes — manifest requires at least one stage. Delete is blocked on the last remaining stage. |
| 4 | Order | Does stage array order have any runtime significance beyond initial_stage? | No — only initial_stage matters for runtime. Order is editorial (display in Presence Grid columns, dropdown ordering). |

## Status

`Approved` — 2026-05-19
