---
schema: dia.module.v1
module_id: dia.economyinspector
name: DiaEconomyInspector
layer: tools/inspector
path: Dia/DiaEconomyInspector
dependencies:
  required:
    - DiaEditor
    - DiaCore
    - DiaObservation
  optional: []
  forbidden:
    - DiaVisualDebugger
    - DiaEconomy
public_api:
  namespace: Dia::EconomyInspector
  headers:
    - DiaEconomyInspector/DiaEconomyInspectorPlugin.h
  entry_points:
    - DiaEconomyInspectorPlugin — LiveConnectionPluginBase subclass registered via REGISTER_EDITOR_PLUGIN
responsibilities:
  - Subscribe to economy.instances, economy.modifiers, economy.events, economy.schema topics
  - Forward each payload to HTML UI via WebUIBridge::NotifyUIDataChanged
non_responsibilities:
  - Game-side data collection — handled by InspectorSources in CluicheGameBaseline
  - Runtime economy evaluation — DiaEconomy
decisions:
  - Four controllers, one per topic (EI-001)
  - Plugin URL: dia://plugins/economyinspector/index.html
  - Read-only inspector in v1
---
