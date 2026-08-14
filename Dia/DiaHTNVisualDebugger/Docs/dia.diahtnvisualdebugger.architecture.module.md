---
module: dia.diahtnvisualdebugger.v1
id: DiaHTNVisualDebugger
layer: tools
parent: DiaHTN
description: IDebugDomain panel visualization for the HTN planner — active plan list, task cursor, divergence flag.
status: active
dependent_modules:
  - DiaHTN
  - DiaVisualDebugger
  - DiaCore
public_headers:
  - HTNVisualDebugger.h
namespaces:
  - Dia::HTN
entry_points:
  - HTNVisualDebugger
non_responsibilities:
  - HTN plan execution
  - World-space entity labels
  - Automatic domain registration
compile_guard: DIA_DEBUG
---
