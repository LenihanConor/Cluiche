---
schema: dia.module.v1
module_id: dia.perfinspector
name: DiaPerfInspector
layer: tools/inspector
path: Dia/DiaPerfInspector
status: active
dependent_modules:
  - dia.editor
  - dia.core
  - dia.json
  - dia.observation
public_api:
  headers:
    - DiaPerfInspector/DiaPerfInspectorPlugin.h
  namespaces:
    - Dia::Editor
  entry_points:
    - REGISTER_EDITOR_PLUGIN(DiaPerfInspectorPlugin, "DiaPerfInspector")
responsibilities:
  - Subscribe the perf.timing game topic and forward it to the UI as perf_inspector.timing
  - Relay perf.window and perf.frame queries to the connected game and push replies as perf_inspector.query_result
  - Serve the Inspector UI via dia://plugins/diaperfinspector/index.html (shell: connection banner and lanes area)
non_responsibilities:
  - Lane maths or chart rendering (UI-side, later tasks)
  - Owning the GameConnectionManager service (fetched from PluginServiceLocator)
  - Persisting any state to disk
---

# DiaPerfInspector

CluicheEditor Inspector-tier plugin showing live frame and module timing of a running Dia application over DiaDebugServer. Currently a shell: connection state, the perf.timing subscription store and typed query helpers.
