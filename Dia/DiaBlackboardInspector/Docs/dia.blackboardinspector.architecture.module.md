---
schema: dia.module.v1
module_id: dia.blackboardinspector
name: DiaBlackboardInspector
layer: domain/visual/tools
path: Dia/DiaBlackboardInspector
dependencies:
  required:
    - DiaEditor
    - DiaBlackboard
    - DiaCore
    - DiaObservation
  optional: []
  forbidden: []
public_api:
  namespace: Dia::Editor
  headers:
    - DiaBlackboardInspector/DiaBlackboardInspectorPlugin.h
  entry_points:
    - DiaBlackboardInspectorPlugin — LiveConnectionPluginBase subclass registered via REGISTER_EDITOR_PLUGIN
responsibilities:
  - Implement DiaBlackboardInspectorPlugin (LiveConnectionPluginBase) for CluicheEditor
  - Subscribe to "blackboard.state" topic and forward payload to HTML panel via GetBridge()->NotifyUIDataChanged
  - Provide dockable HTML/JS panel (UI/index.html) rendering board list, slot rows, observer chips
non_responsibilities:
  - Field value serialization — handled by BlackboardRegistry::RegisterSerializer<T> in game modules
  - Editing slot values at runtime — read-only inspector in v1
  - Blackboard slot value history — v1 shows current state only
decisions:
  - Plugin URL is dia://plugins/blackboardinspector/index.html (SBI-001)
  - Panel is dockable, not floating (SBI-002)
  - No sub-controllers needed in v1 — single OnBlackboardStateUpdate handler (SBI-003)
---
