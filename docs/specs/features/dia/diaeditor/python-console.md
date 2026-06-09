# Feature Spec: python-console

**Parent:** [diaeditor.md](../../../systems/dia/diaeditor.md)

**Status:** Done

**Plan:** @docs/specs/features/dia/diaeditor/python-console.plan.md

## Summary

A dockable editor plugin that provides an interactive Python REPL and script runner inside CluicheEditor. Executes code via `DiaPython::ExecuteString()` / `ExecuteScript()` with stdout/stderr captured and displayed in a scrollable output pane. Primary use case: validating DiaEditorAPI actions interactively and running automation scripts like `smoke_test.py` without leaving the editor.

## Problem

DiaEditorAPI Phase 1 shipped `dia_editor.*` Python actions, but there's no way to invoke them interactively from the running editor. Validating the scripting surface currently requires restarting with a hardcoded `ExecuteScript()` call or writing C++ test harnesses. A Python console eliminates this friction — type `dia_editor.project.get_state()` and see the result immediately.

## Goals

- Dockable editor plugin (`DiaPythonConsolePlugin`) following `IEditorPlugin` pattern
- Single-line input field with Enter to execute
- Scrollable output area showing interleaved input echo, stdout, stderr, and return values
- Stdout/stderr captured via `DiaPython::RedirectOutput()` callbacks during execution
- "Run File" button that opens a native file dialog, selects a `.py` file, and executes it via `ExecuteScript()`
- Output area distinguishes stdout (white), stderr (red), input echo (grey), and system messages (yellow)
- Clear button to reset output area
- Plugin registers as `"python_console"` in plugin manifest

## Non-Goals

- Multi-line editing (Shift+Enter) — keep it simple; use "Run File" for scripts
- Code completion / IntelliSense — future enhancement
- Variable inspector / object browser — DiaChatPlugin territory
- Persistent command history across sessions — session-only for now
- Syntax highlighting in the input field

## Acceptance Criteria

1. **AC1:** User types `dia_editor.project.get_state()` in the input, presses Enter, and sees the JSON result in the output area within the same frame cycle
2. **AC2:** User clicks "Run File", selects `scripts/smoke_test.py`, and sees all `print()` output and any `AssertionError` traceback in the output area
3. **AC3:** Stderr output (e.g., `import nonexistent_module`) renders in red; stdout in white
4. **AC4:** Plugin appears in the Plugin Browser and can be shown/hidden/docked like any other panel
5. **AC5:** Executing long-running or erroring Python does not block the editor main thread beyond the existing DiaPython 5s timeout (inherits `EditorActionQueue` timeout behaviour)

## Public Interfaces

### C++ (plugin side)

```cpp
// Dia/DiaPythonConsoleEditor/DiaPythonConsolePlugin.h
class DiaPythonConsolePlugin : public Dia::Editor::IEditorPlugin {
public:
    static const Dia::Core::StringCRC kUniqueId;  // "python_console"
    // Standard IEditorPlugin lifecycle
};
```

### JS → C++ handlers (WebUIBridge)

| Handler | Params | Returns |
|---------|--------|---------|
| `python_console.execute` | `{ "code": "..." }` | `{ "stdout": "...", "stderr": "...", "returnValue": "..." }` |
| `python_console.run_file` | `{ "path": "..." }` | `{ "stdout": "...", "stderr": "...", "exitCode": 0 }` |

Both handlers execute on main thread (kMainThread dispatch) and capture output via `DiaPython::RedirectOutput()` scoped to the call duration.

## Binding Decisions

| Source | ID | Constraint | Compliance |
|--------|----|-----------|------------|
| DiaEditor | SED-001 | Plugin interface is minimal and stable | Standard `IEditorPlugin` — `Initialize`, `Shutdown`, `OnNavigate` |
| DiaEditor | SED-003 | Plugin lives at `Dia/Dia<System>Editor/` | `Dia/DiaPythonConsoleEditor/` |
| DiaEditor | SED-015 | DiaEditor is a pure library — no ApplicationFlow dependency | Plugin only calls into DiaPython and WebUIBridge; no Module/Phase |
| DiaEditor | SED-022 | PluginServiceLocator for inter-plugin services | No services consumed or provided by this plugin |

## Tasks

| # | Task | Test |
|---|------|------|
| 1 | Scaffold `DiaPythonConsolePlugin` — `Dia/DiaPythonConsoleEditor/`, vcxproj, `IEditorPlugin` subclass, register in editor manifest | Editor starts with plugin loaded; appears in Plugin Browser |
| 2 | Implement `python_console.execute` handler — calls `DiaPython::ExecuteString()`, captures stdout/stderr via `RedirectOutput()` scoped callback, returns JSON | Unit: execute `"1+1"` returns `"2"` in stdout; execute `"import sys; print(sys.version)"` returns version string; execute `"raise ValueError('x')"` returns traceback in stderr |
| 3 | Implement `python_console.run_file` handler — calls `DiaPython::ExecuteScript()` with file path, same capture pattern | Unit: run a test script that prints and asserts; verify stdout contains print output; verify exit code 0 on success, non-zero on exception |
| 4 | Build React UI — input field, output area (scrollable div), Run File button, Clear button; wire to WebUIBridge handlers | Manual: type code → see result; click Run File → dialog opens → output appears; Clear resets |
| 5 | Register `python_console.execute` and `python_console.run_file` as DiaEditorAPI actions (dual-registration) | `dia_editor.python_console.execute` callable from Python smoke test |
