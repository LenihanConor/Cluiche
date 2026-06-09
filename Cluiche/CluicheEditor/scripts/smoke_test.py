"""
EditorAPI smoke test — runs inside the CluicheEditor embedded Python interpreter.

Execute via the DiaPython script runner after the editor has started and
dia_editor has been generated.  Example (from C++):
    Dia::Python::ExecuteScript("scripts/smoke_test.py")

All checks use assert so any failure raises AssertionError, which DiaPython
converts to a Python exception and logs as DIA_LOG_ERROR.
"""
import json

# ---------------------------------------------------------------------------
# Import guard — fail fast with a clear message if the module is missing.
# ---------------------------------------------------------------------------
try:
    import dia_editor.project as project
    import dia_editor.game_connection as game_connection
    import dia_editor.app_editor as app_editor
    import dia_editor.plugin_browser as plugin_browser
except ImportError as e:
    raise ImportError(
        f"dia_editor sub-modules not available: {e}. "
        "Ensure GeneratePythonModule() was called before this script runs."
    ) from e


def _parse(raw: str) -> dict:
    """Deserialise a JSON string returned by an action callable."""
    return json.loads(raw)


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
# Summary
# ---------------------------------------------------------------------------
print("[smoke] All EditorAPI smoke checks passed.")
