# Feature Spec: Risky Change Warnings

## Parent System
@docs/specs/systems/dia/diaapplicationfloweditor.md

## Traceability

| Level | Spec | Decision IDs honored |
|-------|------|---------------------|
| Platform | @docs/specs/platform/Cluiche.md | PD-007 |
| Application | @docs/specs/applications/dia.md | AD-003 |
| System | @docs/specs/systems/dia/diaapplicationfloweditor.md | ED-007 |

## Purpose

Intercept destructive or high-impact edit commands before execution and warn the user about cascading consequences. Prevents accidental data loss from operations like deleting a PU with active modules or removing a stream that other modules reference.

## Acceptance Criteria

1. **Pre-execution intercept** — Before certain commands execute, check if they have cascading effects. If yes, show a warning dialog before proceeding.
2. **Risky operations detected:**
   - Delete PU that has modules assigned
   - Delete module that other modules depend on
   - Delete stream that modules reference in reads/writes
   - Remove a stage that has modules assigned to it
   - Rename a PU/module/stream (updates all references — inform user of scope)
3. **Warning dialog** — Shows: what will happen, list of affected elements, "Proceed" / "Cancel" buttons.
4. **Cascade summary** — Dialog explicitly lists what will be removed/modified as a consequence (e.g., "Deleting SimPU will also remove: 4 modules, 2 stream connections").
5. **Bypass not possible** — No "don't show again" option. Every risky action warns. Safety over convenience.
6. **Undo available** — Dialog mentions "You can undo this action with Ctrl+Z" to reduce friction.
7. **Connection check via PluginServiceLocator** — `RiskAssessor` queries `GameConnectionManager::IsConnected()` directly, not the Inspector plugin or LiveStateStore. Connection state is framework-level (DAFI-009).
8. **No Inspector dependency for risk dialog** — RiskyChangeDialog works identically whether Inspector is loaded or not. If connected (per GameConnectionManager), warning severity escalates ("change will affect running game"). If not connected, only structural risks shown.

## Design

### Risk Assessment

Before executing a command, the system calls a `RiskAssessor`:

```cpp
namespace Dia::ApplicationFlow::Editor {
    struct RiskWarning {
        const char* title;          // e.g., "Delete Processing Unit"
        const char* description;    // e.g., "SimPU has 4 modules that will be removed"
        // affected elements
        Dia::Core::Containers::DynamicArrayC<const char*, 16> affectedItems;
    };

    class RiskAssessor {
    public:
        // Returns null if no risk; populated warning if risky
        const RiskWarning* Assess(const ICommand* command, const ManifestDocument& doc);
    };
}
```

### Integration with Command Execution

The flow for any edit:
1. User action creates a command
2. `RiskAssessor::Assess(command, doc)` called
3. If no risk → execute immediately
4. If risky → send warning to frontend, await user confirmation
5. On "Proceed" → execute
6. On "Cancel" → discard command

### Warning Dialog (React)

```
RiskyChangeDialog (modal)
├── Title (e.g., "Delete Processing Unit?")
├── Description (what will happen)
├── AffectedList (bulleted list of cascading changes)
├── UndoHint ("You can undo this with Ctrl+Z")
├── ProceedButton ("Delete" / "Rename" — action-specific label)
└── CancelButton ("Cancel")
```

### Risky Command Registry

| Command Type | Risk Condition | Cascade Description |
|-------------|---------------|---------------------|
| `RemoveProcessingUnitCommand` | PU has modules | "N modules will be removed, M stream connections broken" |
| `RemoveModuleCommand` | Other modules depend on it | "N modules list this as a dependency — deps will be removed" |
| `RemoveStreamCommand` | Modules reference it in reads/writes | "N modules reference this stream — refs will be removed" |
| `RemoveStageCommand` | Modules assigned to this stage | "N modules will lose this stage assignment" |
| `RenamePUCommand` | Always (informational) | "N stream from/to references will be updated" |
| `RenameModuleCommand` | Other modules reference it | "N dependency references will be updated" |

### Frontend Communication

- `editor.risk.check(commandData)` → returns `{ isRisky, warning }` or executes directly
- Event: `onRiskWarning(warning)` → frontend shows dialog
- `editor.risk.confirm()` / `editor.risk.cancel()` → proceed or abort

## Tasks

| # | Task | Test | Status | Notes |
|---|------|------|--------|-------|
| 1 | `RiskAssessor` class with all risk checks | Unit test: each risk condition detected | Todo | |
| 2 | Integration with command execution pipeline | Unit test: risky commands don't execute without confirmation | Todo | |
| 3 | CEF message handlers for risk check/confirm/cancel | Integration test: round-trip | Todo | |
| 4 | React `RiskyChangeDialog` component | Manual: dialog shows correct info, buttons work | Todo | |

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-007 | C++20 required | Uses C++20. |
| AD-003 | Namespace Dia::\<Module\>:: | In `Dia::ApplicationFlow::Editor::`. |
| ED-007 | React + CEF frontend | Dialog in React; assessor in C++ backend. |

## Open Questions

None.

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Scope | Should rename always warn, even if there are zero references to update? | No — only warn if there are actual references that will be updated. Zero-reference rename is safe; no dialog needed. |
| 2 | UX | Should the dialog be blocking (modal) or a toast-style confirmation? | Modal — destructive actions deserve full attention. Toast could be missed or accidentally dismissed. |
| 3 | Threshold | Should there be a threshold (e.g., only warn if >N elements affected)? | No threshold — even removing one dependent module is worth warning about. The cost of the dialog is one click; the cost of accidental deletion is undo-hunting. |
| 4 | Batch | If a compound command has multiple risky sub-actions, show one combined warning or per-sub-action? | One combined warning for the top-level action. User doesn't need to confirm each sub-step — they're confirming the intent. |

## Status

`Approved` — 2026-05-19
