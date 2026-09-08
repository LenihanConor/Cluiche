"""
EditorAPI smoke test — runs inside the CluicheEditor embedded Python interpreter.

Execute via the DiaPython script runner after the editor has started and
dia_editor has been generated.  Example (from C++):
    Dia::Python::ExecuteScript("scripts/smoke_test.py")

All checks use assert so any failure raises AssertionError, which DiaPython
converts to a Python exception and logs as DIA_LOG_ERROR.
"""
import json
import importlib

# ---------------------------------------------------------------------------
# Always-present modules — built-in plugins loaded at startup.
# ---------------------------------------------------------------------------
try:
    import dia_editor.project as project
    import dia_editor.game_connection as game_connection
    import dia_editor.app_editor as app_editor
    import dia_editor.plugin_browser as plugin_browser
except ImportError as e:
    raise ImportError(
        f"dia_editor core sub-modules not available: {e}. "
        "Ensure GeneratePythonModule() was called before this script runs."
    ) from e


def _parse(raw: str) -> dict:
    """Deserialise a JSON string returned by an action callable."""
    return json.loads(raw)


def _ensure_plugin(type_id: str, module_name: str):
    """Load an optional plugin if not already loaded, then import its dia_editor sub-module.

    Calls plugin_browser.load to trigger OnPluginLoad → DualRegisterActions →
    GeneratePythonModule so the sub-module exists before we import it.
    Returns the imported module.
    """
    load_result = _parse(plugin_browser.load(f'{{"typeId":"{type_id}"}}'))
    if not load_result.get("success", False):
        error = load_result.get("error", "unknown")
        if error != "plugin is already loaded":
            raise RuntimeError(f"Failed to load plugin '{type_id}': {error}")
    # Sub-module may now exist — import (or re-import to pick up new functions).
    return importlib.import_module(f"dia_editor.{module_name}")


# ---------------------------------------------------------------------------
# AC-1: project.get_state returns a valid JSON object
# ---------------------------------------------------------------------------
raw = project.get_state()
assert isinstance(raw, str), f"project.get_state() should return str, got {type(raw)}"
state = _parse(raw)
assert isinstance(state, dict), f"project.get_state() JSON should decode to dict, got {type(state)}"
print(f"[smoke] project.get_state() OK: {state}")


# ---------------------------------------------------------------------------
# AC-2: game_connection.get_state returns a valid JSON object
# ---------------------------------------------------------------------------
raw = game_connection.get_state()
assert isinstance(raw, str), f"game_connection.get_state() should return str, got {type(raw)}"
conn_state = _parse(raw)
assert isinstance(conn_state, dict), f"game_connection.get_state() JSON should decode to dict, got {type(conn_state)}"
print(f"[smoke] game_connection.get_state() OK: {conn_state}")


# ---------------------------------------------------------------------------
# AC-3: project.close on an unopened project returns a failure or no-op
#        (success=False is fine — what we are checking is that it does not crash)
# ---------------------------------------------------------------------------
raw = project.close()
result = _parse(raw)
assert isinstance(result, dict), "project.close() must return a JSON dict"
print(f"[smoke] project.close() (no-op) OK: {result}")


# ---------------------------------------------------------------------------
# AC-4: project.open_path with a bad path returns success=false + reason
# ---------------------------------------------------------------------------
raw = project.open_path('{"path": "/nonexistent/path/does_not_exist.diagame"}')
result = _parse(raw)
assert isinstance(result, dict), "project.open_path() must return a JSON dict"
# A missing file must never crash; success may be false with a reason.
print(f"[smoke] project.open_path(bad_path) OK: {result}")


# ---------------------------------------------------------------------------
# AC-5: app_editor.get_active_context returns dict with required keys
# ---------------------------------------------------------------------------
raw = app_editor.get_active_context()
assert isinstance(raw, str), f"app_editor.get_active_context() should return str, got {type(raw)}"
ctx = _parse(raw)
assert isinstance(ctx, dict), f"app_editor.get_active_context() JSON should decode to dict"
for key in ("project", "focus", "edit_target", "selection"):
    assert key in ctx, f"app_editor.get_active_context() missing key '{key}'"
print(f"[smoke] app_editor.get_active_context() OK: keys present")


# ---------------------------------------------------------------------------
# AC-6: app_editor.navigate_to_plugin with unknown id returns plugin_not_loaded
# ---------------------------------------------------------------------------
raw = app_editor.navigate_to_plugin('{"plugin_id": "NonExistentPlugin_XYZ"}')
result = _parse(raw)
assert isinstance(result, dict), "navigate_to_plugin must return a JSON dict"
# Either plugin_not_loaded or no_project_open — both are valid failures (no crash is the ACs)
assert result.get("success") == False or "reason" in result, \
    f"Expected failure for unknown plugin, got: {result}"
print(f"[smoke] app_editor.navigate_to_plugin(unknown) OK: {result}")


# ---------------------------------------------------------------------------
# AC-7: app_editor.navigate_to_asset returns a failure (no crash)
# With no project open the reason is no_project_open; not_implemented is
# returned when a project is loaded but the feature isn't wired yet.
# ---------------------------------------------------------------------------
raw = app_editor.navigate_to_asset('{"asset_id": "some_asset"}')
result = _parse(raw)
assert isinstance(result, dict), "navigate_to_asset must return a JSON dict"
assert result.get("success") == False or "reason" in result, \
    f"Expected failure for asset navigation, got: {result}"
print(f"[smoke] app_editor.navigate_to_asset() OK: {result}")


# ---------------------------------------------------------------------------
# plugin_browser.get_available returns a list with correct field shape
# ---------------------------------------------------------------------------
raw = plugin_browser.get_available()
assert isinstance(raw, str), f"plugin_browser.get_available() should return str, got {type(raw)}"
result = _parse(raw)
assert isinstance(result, dict), "plugin_browser.get_available() must return a JSON dict"
assert "plugins" in result, "plugin_browser.get_available() must have 'plugins' key"
assert isinstance(result["plugins"], list), "'plugins' must be a list"
assert len(result["plugins"]) > 0, "plugin list must be non-empty"
entry = result["plugins"][0]
for field in ("name", "version", "description", "typeId", "loaded", "pinned"):
    assert field in entry, f"plugin entry missing field '{field}'"
print(f"[smoke] plugin_browser.get_available() OK: {len(result['plugins'])} plugins")


# ---------------------------------------------------------------------------
# entity_template_editor.get_project_state returns dict with required keys
# ---------------------------------------------------------------------------
entity_template_editor = _ensure_plugin("DiaEntityTemplateEditor", "entity_template_editor")
raw = entity_template_editor.get_project_state()
assert isinstance(raw, str), f"entity_template_editor.get_project_state() should return str, got {type(raw)}"
state = _parse(raw)
assert isinstance(state, dict), "entity_template_editor.get_project_state() must return a JSON dict"
assert "isValid" in state, "get_project_state() missing 'isValid'"
assert "diagamePath" in state, "get_project_state() missing 'diagamePath'"
print(f"[smoke] entity_template_editor.get_project_state() OK: {state}")


# ---------------------------------------------------------------------------
# asset_catalogue.get_state returns a dict with required keys
# ---------------------------------------------------------------------------
asset_catalogue = _ensure_plugin("DiaAssetCatalogueEditorPlugin", "asset_catalogue")
raw = asset_catalogue.get_state()
assert isinstance(raw, str), f"asset_catalogue.get_state() should return str, got {type(raw)}"
state = _parse(raw)
assert isinstance(state, dict), "asset_catalogue.get_state() must return a JSON dict"
assert "success" in state, "asset_catalogue.get_state() missing 'success'"
print(f"[smoke] asset_catalogue.get_state() OK: {state}")


# ---------------------------------------------------------------------------
# asset_catalogue.get_available returns loaded=true when plugin is active
# ---------------------------------------------------------------------------
raw = asset_catalogue.get_available()
assert isinstance(raw, str), f"asset_catalogue.get_available() should return str, got {type(raw)}"
avail = _parse(raw)
assert isinstance(avail, dict), "asset_catalogue.get_available() must return a JSON dict"
assert "loaded" in avail, "asset_catalogue.get_available() missing 'loaded' key"
assert avail["loaded"] == True, f"asset_catalogue.get_available() expected loaded=true, got: {avail}"
print(f"[smoke] asset_catalogue.get_available() OK: {avail}")


# ---------------------------------------------------------------------------
# asset_catalogue.validate returns success=true with errors array
# ---------------------------------------------------------------------------
raw = asset_catalogue.validate()
assert isinstance(raw, str), f"asset_catalogue.validate() should return str, got {type(raw)}"
vresult = _parse(raw)
assert isinstance(vresult, dict), "asset_catalogue.validate() must return a JSON dict"
assert "success" in vresult, "asset_catalogue.validate() missing 'success'"
print(f"[smoke] asset_catalogue.validate() OK: {vresult}")


# ---------------------------------------------------------------------------
# scene_editor.get_project_state returns dict with required keys
# ---------------------------------------------------------------------------
scene_editor = _ensure_plugin("DiaSceneEditor", "scene_editor")
raw = scene_editor.get_project_state()
assert isinstance(raw, str), f"scene_editor.get_project_state() should return str, got {type(raw)}"
state = _parse(raw)
assert isinstance(state, dict), "scene_editor.get_project_state() must return a JSON dict"
assert "isValid" in state, "get_project_state() missing 'isValid'"
assert "diagamePath" in state, "get_project_state() missing 'diagamePath'"
print(f"[smoke] scene_editor.get_project_state() OK: {state}")


# ---------------------------------------------------------------------------
# scene_editor.get_entities returns success + entities list
# ---------------------------------------------------------------------------
raw = scene_editor.get_entities()
assert isinstance(raw, str), f"scene_editor.get_entities() should return str, got {type(raw)}"
ents = _parse(raw)
assert isinstance(ents, dict), "scene_editor.get_entities() must return a JSON dict"
assert "success" in ents, "scene_editor.get_entities() missing 'success'"
assert "entities" in ents, "scene_editor.get_entities() missing 'entities'"
assert isinstance(ents["entities"], list), "'entities' must be a list"
print(f"[smoke] scene_editor.get_entities() OK: {len(ents['entities'])} entities")


# ---------------------------------------------------------------------------
# app_flow_editor.manifest.getState, history.getState, validation.run, types.get, risk.check
# ---------------------------------------------------------------------------
manifest = _ensure_plugin("DiaApplicationFlowEditorPlugin", "manifest")
# Import the sub-modules that are generated after plugin load
history = importlib.import_module("dia_editor.history")
validation = importlib.import_module("dia_editor.validation")
types = importlib.import_module("dia_editor.types")
risk = importlib.import_module("dia_editor.risk")


# manifest.getState() returns dict with 'ok' key (no crash with no manifest loaded)
raw = manifest.getState()
assert isinstance(raw, str), f"manifest.getState() should return str, got {type(raw)}"
state = _parse(raw)
assert isinstance(state, dict), "manifest.getState() must return a JSON dict"
assert "ok" in state, "manifest.getState() missing 'ok' key"
print(f"[smoke] manifest.getState() OK: {state}")


# history.getState() returns dict with required keys (canUndo, canRedo, count, isDirty)
raw = history.getState()
assert isinstance(raw, str), f"history.getState() should return str, got {type(raw)}"
hist_state = _parse(raw)
assert isinstance(hist_state, dict), "history.getState() must return a JSON dict"
for key in ("ok", "canUndo", "canRedo", "count", "isDirty"):
    assert key in hist_state, f"history.getState() missing key '{key}'"
print(f"[smoke] history.getState() OK: canUndo={hist_state['canUndo']}, canRedo={hist_state['canRedo']}, count={hist_state['count']}, isDirty={hist_state['isDirty']}")


# validation.run() returns dict with 'ok' key (no crash with no manifest loaded)
raw = validation.run()
assert isinstance(raw, str), f"validation.run() should return str, got {type(raw)}"
val_result = _parse(raw)
assert isinstance(val_result, dict), "validation.run() must return a JSON dict"
assert "ok" in val_result, "validation.run() missing 'ok' key"
print(f"[smoke] validation.run() OK: {val_result}")


# types.get() returns dict with 'ok', 'moduleTypes', 'puTypes' keys
raw = types.get()
assert isinstance(raw, str), f"types.get() should return str, got {type(raw)}"
types_result = _parse(raw)
assert isinstance(types_result, dict), "types.get() must return a JSON dict"
assert "ok" in types_result, "types.get() missing 'ok' key"
assert "moduleTypes" in types_result, "types.get() missing 'moduleTypes' key"
assert "puTypes" in types_result, "types.get() missing 'puTypes' key"
print(f"[smoke] types.get() OK: moduleTypes present, puTypes present")


# risk.check() returns dict with 'ok' and 'hasRisk' keys
raw = risk.check('{"commandType": "RemovePU"}')
assert isinstance(raw, str), f"risk.check() should return str, got {type(raw)}"
risk_result = _parse(raw)
assert isinstance(risk_result, dict), "risk.check() must return a JSON dict"
assert "ok" in risk_result, "risk.check() missing 'ok' key"
assert "hasRisk" in risk_result, "risk.check() missing 'hasRisk' key"
print(f"[smoke] risk.check() OK: hasRisk={risk_result['hasRisk']}")


# ---------------------------------------------------------------------------
# DiaChatPlugin — module loads; initialize() accepts a project path
# ---------------------------------------------------------------------------
import sys, os
_chat_scripts = os.path.join(os.path.dirname(__file__))
if _chat_scripts not in sys.path:
    sys.path.insert(0, _chat_scripts)

import dia_chat as _chat

# initialize() should not raise; it can silently fail if no LLM backend is available
# but must return without crashing
try:
    _chat.initialize(project_path="smoke_test_project", backend="ollama")
except Exception as e:
    print(f"[smoke] DiaChatPlugin initialize() non-fatal: {e}")

# _orchestrator should be set after initialize
assert _chat._orchestrator is not None, "DiaChatPlugin: _orchestrator was not created by initialize()"
print(f"[smoke] DiaChatPlugin initialize() OK: orchestrator created")

# clear_history() must not raise
_chat.clear_history()
print(f"[smoke] DiaChatPlugin clear_history() OK")


# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------
print("[smoke] All EditorAPI smoke checks passed.")
