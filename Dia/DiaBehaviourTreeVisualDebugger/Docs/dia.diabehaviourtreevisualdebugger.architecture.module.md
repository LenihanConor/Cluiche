---
module: dia.diabehaviourtreevisualdebugger.v1
id: DiaBehaviourTreeVisualDebugger
layer: tools
parent: DiaBehaviourTree
description: IDebugDomain panel visualization for the behaviour tree — node visit sequence, per-node result badge, TreeView toggle.
status: active
dependent_modules:
  - DiaBehaviourTree
  - DiaVisualDebugger
  - DiaCore
public_headers:
  - BehaviourTreeVisualDebugger.h
namespaces:
  - Dia::BehaviourTree
entry_points:
  - BehaviourTreeVisualDebugger
non_responsibilities:
  - Behaviour tree execution
  - World-space entity labels
  - Automatic domain registration
compile_guard: DIA_DEBUG
---
