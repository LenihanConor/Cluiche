---
schema: dia.module.v1
module_id: dia.aiinspector
name: DiaAIInspector
layer: domain/visual/tools
path: Dia/DiaAIInspector
dependencies:
  required:
    - DiaEditor
    - DiaCore
    - DiaObservation
  optional: []
  forbidden:
    - DiaVisualDebugger
    - DiaRules
    - DiaHTN
    - DiaUtilityAI
    - DiaAIBudget
public_api:
  namespace: Dia::AIInspector
  headers:
    - DiaAIInspector/DiaAIInspectorPlugin.h
  entry_points:
    - DiaAIInspectorPlugin — LiveConnectionPluginBase subclass registered via REGISTER_EDITOR_PLUGIN
responsibilities:
  - Subscribe to ai_budget.state, utility_ai.state, rules.state, htn.state topics
  - Forward each payload to HTML UI via WebUIBridge::NotifyUIDataChanged
non_responsibilities:
  - Game-side data collection — handled by InspectorSources in CluicheGameBaseline
  - Runtime AI evaluation — DiaRules, DiaUtilityAI, DiaHTN, DiaAIBudget
decisions:
  - Four controllers, one per topic (SD-001)
  - Plugin URL: dia://plugins/aiinspector/index.html
  - Read-only inspector in v1 (SD-004)
---
