**Spec:** @docs/specs/applications/cluicheeditor/systems/diaeditorapi/entity-template-migration.md
**Status:** Done

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Add `DualRegisterActions()` to `DiaBlueprintEditorPlugin`; call from `OnPluginLoad()` after existing `RegisterHandler` calls; call `DeregisterActionsForOwner` in `OnPluginUnload()` | `GetManifest()` lists all 9 `entity_template_editor.*` actions | Pending | sonnet | |
| 2 | Register 5 kMainThread actions: `get_project_state`, `get_list`, `get_available_components`, `get_usage`, `load` | Python: `dia_editor.entity_template_editor.get_project_state()` returns expected shape | Pending | haiku | Note: spec resolved all 9 to kMainThread (thread safety) |
| 3 | Register 4 kMainThread actions: `save`, `update_field`, `add_component`, `remove_component` | Python: callable from dia_editor.entity_template_editor.* | Pending | sonnet | |
| 4 | Python smoke test: add `entity_template_editor.get_project_state` check to smoke_test.py | Smoke test passes; no errors in output | Pending | haiku | |
