**Spec:** @docs/specs/features/cluicheeditor/diaeditorapi/app-flow-editor-migration.md
**Status:** In Progress

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Add `DualRegisterActions()` scaffold to `DiaApplicationFlowEditorPlugin` — method declaration in .h, empty body in .cpp, call from `OnPluginLoad()` after `RegisterHandler` calls, call `DeregisterActionsForOwner` in `OnPluginUnload()` | Editor builds without error; `OnPluginUnload` calls deregister | Pending | sonnet | |
| 2 | Register 3 read/query actions: `manifest.getState`, `history.getState`, `types.get` — all `kMainThread` | Python: `dia_editor.manifest.getState()` and `dia_editor.history.getState()` return expected shapes | Pending | haiku | |
| 3 | Register 4 mutating manifest actions: `manifest.load`, `manifest.save`, `manifest.applyCommand`, `validation.run` — all `kMainThread` | Python: `manifest.load(path)` + `manifest.applyCommand({commandType: "AddStage", ...})` + `validation.run()` work end-to-end | Pending | sonnet | |
| 4 | Register 3 remaining actions: `history.undo`, `history.redo`, `risk.check` — all `kMainThread` | Python: `history.undo()` returns `{ok: false}` on empty history; `risk.check({commandType: "RemovePU"})` returns `hasRisk=false` when not live-connected | Pending | haiku | |
| 5 | Python smoke test: `manifest.load(path)` → `manifest.getState()` (assert `hasManifest=true`) → `validation.run()` (assert `ok=true`, fields present) → `history.getState()` (assert `canUndo=false` after fresh load) | All 4 assertions pass; no `DIA_LOG_ERROR` in output | Pending | haiku | |
