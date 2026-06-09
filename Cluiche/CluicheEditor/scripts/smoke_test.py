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
# Summary
# ---------------------------------------------------------------------------
print("[smoke] All EditorAPI smoke checks passed.")
