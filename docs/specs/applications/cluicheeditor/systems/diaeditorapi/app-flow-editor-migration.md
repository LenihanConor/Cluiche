# Feature Spec: app-flow-editor-migration

**Parent:** @docs/specs/applications/cluicheeditor/systems/diaeditorapi/diaeditorapi.md
**Status:** Done

## Summary

Dual-registers 10 existing `manifest.*`, `history.*`, `validation.*`, `types.*`, and `risk.*` WebUIBridge handlers in `DiaApplicationFlowEditorPlugin` with the `EditorActionRegistry`. No handler logic changes. After migration, every manifest editing operation is callable from Python via `dia_editor.manifest.*` and friends — giving scripts, automation tests, and DiaChatPlugin the ability to load, inspect, modify, validate, and save `.diaapp` manifests programmatically.

## Problem

The application flow editor has a rich command surface — load/save manifests, apply structural commands (add/remove PUs/modules/stages/streams), undo/redo, run validation — but none of it is reachable from Python or the AI layer. An AI that knows the manifest structure cannot drive changes without going through the JS UI.

## Goals

1. Expose all 10 selected handlers through DiaEditorAPI with correct thread dispatch.
2. No existing WebUIBridge behaviour changes — dual-registration only.
3. `DeregisterActionsForOwner` wired up on plugin unload.

## Acceptance Criteria

| # | Criterion |
|---|-----------|
| AC1 | All 10 actions are registered in `EditorActionRegistry` and appear in `GetManifest()` output |
| AC2 | All 10 are callable from Python via `dia_editor.manifest.*`, `dia_editor.history.*`, `dia_editor.validation.*`, `dia_editor.types.*`, `dia_editor.risk.*` |
| AC3 | All 10 actions use `kMainThread` — plugin state (`mEditorState`, `mCommandHistory`, `mTypeDiscovery`) is not thread-safe |
| AC4 | `DeregisterActionsForOwner(StringCRC("DiaApplicationFlowEditorPlugin"))` removes all actions on plugin unload |
| AC5 | Python smoke test: `manifest.load(path)` → `manifest.getState()` → `validation.run()` all return expected shapes with no `DIA_LOG_ERROR` |

## Excluded Handlers

| Handler | Reason |
|---------|--------|
| `types.refresh` | Implementation detail — reloads schema from disk in response to file system changes; no Python caller should trigger this manually |
| `risk.confirm` | UI-only guard prompt — Python callers accept risk implicitly; no round-trip confirmation needed |

## Action Manifest

### Thread assignments

**kMainThread (all 10 actions):**
All actions use `kMainThread`. `mEditorState`, `mCommandHistory`, and `mTypeDiscovery` are plugin member variables mutated by command execution, undo/redo, and load/save. Concurrent `kCallerThread` access would race with main-thread mutations.

---

### `manifest.load`

```
description:
  Loads a .diaapp manifest from disk into the editor. Parses the file, populates the
  in-memory manifest state, resets the command history, and starts the file watcher.
  Call this before any manifest.applyCommand or manifest.save calls. Returns ok=false
  with an error string if the file is missing, malformed, or fails schema validation.

category:  manifest
owner:     DiaApplicationFlowEditorPlugin
dispatch:  kMainThread
params:
  path  string  required  "Absolute path to the .diaapp manifest file to load."
returns:  { "ok": bool, "error"?: string }
```

---

### `manifest.save`

```
description:
  Saves the current in-memory manifest state to disk. Runs validation first — if there
  are any validation errors, the save is blocked and the errors are returned. Clears the
  dirty flag and resets the command history save point on success. No-op if no manifest
  is loaded.

category:  manifest
owner:     DiaApplicationFlowEditorPlugin
dispatch:  kMainThread
params:    none
returns:   { "ok": bool, "error"?: string, "errorCount"?: number }
```

---

### `manifest.getState`

```
description:
  Returns the full current manifest state: file path, dirty flag, and the complete
  manifest structure (stages, processingUnits, streams, initialStage, version). If no
  manifest is loaded but a pending path exists (set during project open), retries the
  load before responding. Use this to read the manifest after load or after applying
  commands.

category:  manifest
owner:     DiaApplicationFlowEditorPlugin
dispatch:  kMainThread
params:    none
returns:   { "ok": bool, "state"?: { "filePath": string, "isDirty": bool, "hasManifest": bool, "manifest"?: object }, "error"?: string }
```

---

### `manifest.applyCommand`

```
description:
  Executes a structural edit command against the loaded manifest and pushes the updated
  state to the UI. All commands are undoable via history.undo. Returns ok=true with
  updated canUndo/canRedo/isDirty flags on success. Use types.get to enumerate valid
  typeId values for AddModule/AddPU. Use risk.check before risky commands when the
  editor is live-connected to a running game.

  Supported commandTypes and their required params:
    PU:      AddPU(instanceId, frequencyHz, dedicatedThread), RemovePU(instanceId),
             SetPUFrequency(instanceId, frequencyHz), SetPUThread(instanceId, dedicatedThread),
             ReorderPU(instanceId, newIndex)
    Module:  AddModule(puId, instanceId, typeId), RemoveModule(puId, instanceId),
             AddModuleDep(puId, instanceId, dependency), RemoveModuleDep(puId, instanceId, dependency),
             SetModuleStages(puId, instanceId, stages[]), SetModuleStartTimeout(puId, instanceId, startTimeoutMs),
             SetModuleStopTimeout(puId, instanceId, stopTimeoutMs),
             AddModuleChannel(puId, instanceId, streamId, role), RemoveModuleChannel(puId, instanceId, streamId, role)
    Stage:   AddStage(name, manifestPath), RemoveStage(name), RenameStage(oldName, newName),
             SetStageTrigger(name, isAuto), AddStageTransition(name, target),
             RemoveStageTransition(name, target), SetInitialStage(name), ReorderStage(name, newIndex)
    Stream:  AddStream(id, kind, payloadType), RemoveStream(streamId),
             SetStreamKind(streamId, value), SetStreamPayloadType(streamId, value),
             SetStreamFromPU(streamId, value), SetStreamToPU(streamId, value),
             SetStreamCapacity(streamId, value), SetStreamMaxReaders(streamId, value),
             SetStreamOverflow(streamId, value), SetStreamMultiWriter(streamId, value)

category:  manifest
owner:     DiaApplicationFlowEditorPlugin
dispatch:  kMainThread
params:
  commandType  string  required  "Command type string — see description for full list."
  ...                            "Additional params depend on commandType — see description."
returns:   { "ok": bool, "canUndo": bool, "canRedo": bool, "isDirty": bool, "error"?: string }
```

---

### `history.undo`

```
description:
  Undoes the last manifest.applyCommand operation. Returns ok=false if there is nothing
  to undo. On success, pushes the updated manifest state to the UI and returns updated
  canUndo/canRedo/isDirty flags. Call history.getState first to check canUndo.

category:  history
owner:     DiaApplicationFlowEditorPlugin
dispatch:  kMainThread
params:    none
returns:   { "ok": bool, "canUndo": bool, "canRedo": bool, "isDirty": bool }
```

---

### `history.redo`

```
description:
  Redoes the last undone manifest.applyCommand operation. Returns ok=false if there is
  nothing to redo. On success, pushes the updated manifest state to the UI and returns
  updated canUndo/canRedo/isDirty flags. Call history.getState first to check canRedo.

category:  history
owner:     DiaApplicationFlowEditorPlugin
dispatch:  kMainThread
params:    none
returns:   { "ok": bool, "canUndo": bool, "canRedo": bool, "isDirty": bool }
```

---

### `history.getState`

```
description:
  Returns the current command history state without modifying it. Use before calling
  history.undo or history.redo to check whether those operations are available.

category:  history
owner:     DiaApplicationFlowEditorPlugin
dispatch:  kMainThread
params:    none
returns:   { "ok": true, "canUndo": bool, "canRedo": bool, "count": number, "isDirty": bool }
```

---

### `validation.run`

```
description:
  Validates the loaded manifest against all structural rules — duplicate IDs, missing
  dependencies, unreachable stages, stream routing errors. Returns the full issue list
  with severity (error/warning), target entity (PU/module/stream), human-readable
  message, and an optional suggestedCommand that can be passed directly to
  manifest.applyCommand to auto-fix the issue. Returns ok=false if no manifest is loaded.

category:  validation
owner:     DiaApplicationFlowEditorPlugin
dispatch:  kMainThread
params:    none
returns:   { "ok": bool, "errorCount": number, "warningCount": number, "issues": [{ "ruleId": number, "severity": "error"|"warning", "message": string, "targetKind": string, "targetPuId": string, "targetModuleId": string, "targetStreamId": string, "suggestedActionLabel": string, "suggestedCommand": object|null }...], "error"?: string }
```

---

### `types.get`

```
description:
  Returns the list of all known module types and processing unit types available to be
  added to the manifest. Each entry has a typeId and a description. Use typeId values
  with manifest.applyCommand AddModule and AddPU commands. Types are loaded from
  registeredtypes.diaschema at plugin load time.

category:  types
owner:     DiaApplicationFlowEditorPlugin
dispatch:  kMainThread
params:    none
returns:   { "ok": true, "moduleTypes": [{ "id": string, "description": string }...], "puTypes": [{ "id": string, "description": string }...] }
```

---

### `risk.check`

```
description:
  Checks whether a given commandType is considered risky when the editor is live-connected
  to a running game instance. Returns hasRisk=false if not live-connected or if the command
  is safe. Returns hasRisk=true with a human-readable description of the risk if the command
  could destabilise the running process. Call this before manifest.applyCommand for any
  structural change (RemovePU, RemoveModule, SetStreamCapacity, SetStreamMaxReaders,
  SetPUFrequency, RemoveStage) when automation is driving a live session.

category:  risk
owner:     DiaApplicationFlowEditorPlugin
dispatch:  kMainThread
params:
  commandType  string  required  "The commandType string you are about to pass to manifest.applyCommand."
returns:   { "ok": true, "hasRisk": bool, "condition"?: string, "description"?: string }
```

---

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Add `DualRegisterActions()` to `DiaApplicationFlowEditorPlugin`; call from `OnPluginLoad()` after existing `RegisterHandler` calls; call `DeregisterActionsForOwner(StringCRC("DiaApplicationFlowEditorPlugin"))` in `OnPluginUnload()` | `GetManifest()` lists all 10 actions after load; `DeregisterActionsForOwner` removes them on unload | — | sonnet | |
| 2 | Register 3 read/query actions: `manifest.getState`, `history.getState`, `types.get` — all `kMainThread` | Python: `dia_editor.manifest.getState()` and `dia_editor.history.getState()` return expected shapes | — | haiku | |
| 3 | Register 4 mutating manifest actions: `manifest.load`, `manifest.save`, `manifest.applyCommand`, `validation.run` — all `kMainThread` | Python: `manifest.load(path)` + `manifest.applyCommand({commandType: "AddStage", ...})` + `validation.run()` work end-to-end | — | sonnet | |
| 4 | Register 3 remaining actions: `history.undo`, `history.redo`, `risk.check` — all `kMainThread` | Python: `history.undo()` returns `{ok: false}` on empty history; `risk.check({commandType: "RemovePU"})` returns `hasRisk=false` when not live-connected | — | haiku | |
| 5 | Python smoke test: `manifest.load(path)` → `manifest.getState()` (assert `hasManifest=true`) → `validation.run()` (assert `ok=true`, fields present) → `history.getState()` (assert `canUndo=false` after fresh load) | All 4 assertions pass; no `DIA_LOG_ERROR` in output | — | haiku | |

## Modules Touched

| Module | Change |
|--------|--------|
| `Dia/DiaApplicationFlowEditor` | `DualRegisterActions()` + `DeregisterActionsForOwner` in unload |

## Binding Decisions

No binding constraints beyond the DiaEditorAPI system spec (EAPI-001 through EAPI-009) apply.

## Open Design Questions

| # | Question | Resolution |
|---|----------|------------|
| ODQ-1 | `manifest.applyCommand` passes a polymorphic params object whose shape depends on `commandType`. The EditorActionParamSchema system expects a fixed param list per action. Should the schema declare the `commandType` param only, with a reference to the description for the full command catalogue? | Resolved — declare `commandType` as the single required param in the schema; document the full per-command param set in the description field (source of truth for `.pyi` docstrings). This is consistent with how the existing JS handler works. |
| ODQ-2 | `manifest.getState` retries a deferred load if `mPendingManifestPath` is set but the manifest hasn't loaded yet. Should this retry side-effect be preserved for Python callers, or should Python callers always call `manifest.load(path)` explicitly? | Resolved — preserve the retry behaviour. It's a safety net that makes the action idempotent from the caller's perspective. Python callers benefit from the same robustness the UI gets. |
