**Spec:** @docs/specs/features/dia/diaeditor/python-console.md
**Status:** In Progress

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Scaffold `DiaPythonConsolePlugin` — `Dia/DiaPythonConsoleEditor/`, vcxproj, IEditorPlugin subclass, register in editor manifest | Editor starts with plugin loaded; appears in Plugin Browser | Pending | sonnet | |
| 2 | Implement `python_console.execute` handler — calls `DiaPython::ExecuteString()`, captures stdout/stderr via `RedirectOutput()` scoped callback, returns JSON | Unit: execute `"1+1"` returns `"2"` in stdout; execute raise ValueError returns traceback in stderr | Pending | sonnet | |
| 3 | Implement `python_console.run_file` handler — calls `DiaPython::ExecuteScript()` with file path, same capture pattern | Unit: run a test script that prints and asserts; verify stdout/exit code | Pending | sonnet | |
| 4 | Build React UI — input field, output area (scrollable div), Run File button, Clear button; wire to WebUIBridge handlers | Manual: type code → see result; click Run File → dialog opens → output appears; Clear resets | Pending | sonnet | |
| 5 | Register `python_console.execute` and `python_console.run_file` as DiaEditorAPI actions (dual-registration) | `dia_editor.python_console.execute` callable from Python smoke test | Pending | sonnet | |
