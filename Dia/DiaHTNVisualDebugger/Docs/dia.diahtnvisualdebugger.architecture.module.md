---
schema: dia.module.v1
module_id: dia.htnvisualdebugger
name: DiaHTNVisualDebugger
layer: domain/gameplay/tools
path: Dia/DiaHTNVisualDebugger
status: active
maturity: dev
parent_module_id: dia.htn

summary: >
  IDebugDomain panel visualization for the HTN planner — active plan list, task
  cursor, divergence flag.

dependencies:
  required:
    - dia.core
    - dia.htn
  forbidden: []

public_api:
  headers:
    - DiaHTNVisualDebugger/HTNVisualDebugger.h
  namespaces:
    - Dia::HTN
  entry_points:
    - HTNVisualDebugger

non_responsibilities:
  - HTN plan execution
  - World-space entity labels
  - Automatic domain registration
---
