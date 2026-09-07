---
schema: dia.module.v1
module_id: dia.behaviourtreevisualdebugger
name: DiaBehaviourTreeVisualDebugger
layer: domain/gameplay/tools
path: Dia/DiaBehaviourTreeVisualDebugger
status: active
maturity: dev
parent_module_id: dia.behaviourtree

summary: >
  IDebugDomain panel visualization for the behaviour tree — node visit sequence,
  per-node result badge, TreeView toggle.

dependencies:
  required:
    - dia.core
    - dia.behaviourtree
  forbidden: []

public_api:
  headers:
    - DiaBehaviourTreeVisualDebugger/BehaviourTreeVisualDebugger.h
  namespaces:
    - Dia::BehaviourTree
  entry_points:
    - BehaviourTreeVisualDebugger

non_responsibilities:
  - Behaviour tree execution
  - World-space entity labels
  - Automatic domain registration
---
