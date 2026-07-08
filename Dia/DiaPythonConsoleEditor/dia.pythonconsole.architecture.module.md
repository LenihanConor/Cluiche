---
schema: dia.module.v1
module_id: dia.pythonconsole
name: DiaPythonConsoleEditor
parent_module_id: dia.root
layer: domain/visual/tools
path: Dia/DiaPythonConsoleEditor
type: editor_plugin
dependencies:
  required:
    - dia.editor
    - dia.python
    - dia.core
---
DiaPythonConsoleEditor — dockable Python REPL plugin for CluicheEditor.
Exposes python_console.execute and python_console.run_file editor actions.
