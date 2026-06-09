**Spec:** @docs/specs/features/dia/diaeditor/python-console.md
**Status:** Done

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Scaffold `DiaPythonConsolePlugin` — `Dia/DiaPythonConsoleEditor/`, vcxproj, IEditorPlugin subclass, register in editor manifest | Editor starts with plugin loaded; appears in Plugin Browser | Done | sonnet | Dia/DiaPythonConsoleEditor/ created; added to Cluiche.sln + CluicheEditor.vcxproj; builds clean |
| 2 | Implement `python_console.execute` handler — calls `DiaPython::ExecuteString()`, captures stdout/stderr via `RedirectOutput()` scoped callback, returns JSON | Unit: execute `"1+1"` returns `"2"` in stdout; execute raise ValueError returns traceback in stderr | Done | sonnet | Scoped RedirectOutput pattern; returns {stdout, stderr, returnValue}; DiaPython project reference added |
| 3 | Implement `python_console.run_file` handler — calls `DiaPython::ExecuteScript()` with file path, same capture pattern | Unit: run a test script that prints and asserts; verify stdout/exit code | Done | sonnet | Returns {stdout, stderr, exitCode}; same scoped redirect pattern |
| 4 | Build React UI — input field, output area (scrollable div), Run File button, Clear button; wire to WebUIBridge handlers | Manual: type code → see result; click Run File → dialog opens → output appears; Clear resets | Done | sonnet | Colour-coded output (stdout white, stderr red, echo grey, system yellow); Enter-to-run; file picker for Run File; pipeline.toml updated |
| 5 | Register `python_console.execute` and `python_console.run_file` as DiaEditorAPI actions (dual-registration) | `dia_editor.python_console.execute` callable from Python smoke test | Done | sonnet | GetServices()-guarded; both actions in EditorActionRegistry with full descriptions |
