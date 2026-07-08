---
schema: dia.module.v1
module_id: dia.applicationflowinspector
name: DiaApplicationFlowInspector
layer: domain/visual/tools
path: Dia/DiaApplicationFlowInspector
status: active
dependent_modules:
  - dia.editor
  - dia.core
  - dia.json
  - dia.observation
public_api:
  headers:
    - DiaApplicationFlowInspector/DiaApplicationFlowInspectorPlugin.h
    - DiaApplicationFlowInspector/LiveStateStore.h
    - DiaApplicationFlowInspector/InspectorHealthReporter.h
  namespaces:
    - Dia::ApplicationFlow::Inspector
    - Dia::Editor
  entry_points:
    - REGISTER_EDITOR_PLUGIN(DiaApplicationFlowInspectorPlugin, "DiaApplicationFlowInspector")
    - InspectorHealthReporter (IHealthReporter)
responsibilities:
  - Own the live game connection lifecycle (connect, disconnect, getStatus, transitionTo, shutdown)
  - Maintain LiveStateStore with pushed runtime state (app state, module states, stream states)
  - Provide InspectorHealthReporter (Healthy=connected+active, Degraded=connected+no data, Unhealthy=disconnected)
  - Serve Inspector UI (4 tabs: Modules, Streams, Timing, Log) via dia://plugins/diaapplicationflowinspector/index.html
  - Broadcast live state topics to UI: live.connectionStatus, live.state, live.modules, live.streams, live.timings, live.event
non_responsibilities:
  - Static manifest editing (owned by DiaApplicationFlowEditor)
  - Persisting any state to disk
  - Owning the GameConnectionManager service (fetched from PluginServiceLocator)
---

# DiaApplicationFlowInspector

CluicheEditor plugin providing live runtime inspection of a running Dia application — connection lifecycle, real-time state, module and stream telemetry, and a 4-tab Inspector UI.
