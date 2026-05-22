# Feature Spec: DiaAutomation Checkpoint Registry

## Parent System
@docs/specs/systems/dia/diaautomation.md

## Depends On
@docs/specs/features/dia/diaautomation/module-and-build.md

## Purpose

Modules register named validation functions (checkpoints) with the AutomationService. The orchestrator calls `dia.automation.validate` to run a checkpoint and get a pass/fail/message result. Checkpoints are automatically cleared when the owning module stops.

## Acceptance Criteria

| # | Criterion | Verification |
|---|-----------|--------------|
| AC1 | `RegisterCheckpoint(owner, name, fn)` stores the entry; `HasCheckpoint(name)` returns true. | Unit test |
| AC2 | `RegisterCheckpoint` rejects duplicate names (same name regardless of owner); logs error; returns without registering. | Unit test |
| AC3 | `UnregisterCheckpoints(owner)` removes all entries owned by that module. | Unit test |
| AC4 | `RunCheckpoint(name)` invokes the registered fn and returns its `CheckpointResult`. | Unit test |
| AC5 | `RunCheckpoint` for an unknown name returns `{passed: false, message: "checkpoint not found", durationMs: 0}`. | Unit test |
| AC6 | `dia.automation.validate` command registered; params `{"checkpoint": "<name>"}`. Returns `{"passed": bool, "message": "...", "duration_ms": float}`. | Unit test via ExecuteCommandJson |
| AC7 | `dia.automation.validate` for unknown checkpoint returns `{"success": false, "error": "checkpoint not found: '<name>'"}`. | Unit test |
| AC8 | After `UnregisterCheckpoints(owner)`, `HasCheckpoint(name)` returns false for that owner's checkpoints. | Unit test |

## Traceability

| Level | Spec | Decision IDs honored |
|-------|------|---------------------|
| Platform | @docs/specs/platform/Cluiche.md | PD-001, PD-004 |
| Application | @docs/specs/applications/dia.md | AD-002, AD-003 |
| System | @docs/specs/systems/dia/diaautomation.md | SD-AUT-001, SD-AUT-002, SD-AUT-005, SD-AUT-007 |

## Binding Decisions Compliance

| Binding Decision | Source | How This Feature Honors It |
|-----------------|--------|---------------------------|
| PD-001 (StringCRC for IDs) | Platform | Checkpoint names are `StringCRC`; command name `dia.automation.validate` is StringCRC |
| PD-004 (No STL containers) | Platform | Registry uses `DynamicArrayC<CheckpointEntry, 64>`; `CheckpointFn` is `std::function` (callable) |
| SD-AUT-002 (Checkpoint lifetime tied to owner) | DiaAutomation | `UnregisterCheckpoints(Module*)` clears all entries by that owner |
| SD-AUT-005 (Consistent envelope) | DiaAutomation | `dia.automation.validate` uses `{success, data/error}` envelope |

## AI Review Questions

| # | Question | Answer |
|---|----------|--------|
| 1 | Should `RegisterCheckpoint` return bool (success/fail) or void? | Returns void; duplicate registration is a programming error (debug assert + log). Consistent with TransitionGuardFn pattern. |
| 2 | Does `dia.automation.validate` need to reset the heartbeat? | Yes — SD-AUT-007: any incoming automation command resets heartbeat. Implemented in `RegisterCommands()` wrapper. |

## Status

`Approved` (2026-05-21)
