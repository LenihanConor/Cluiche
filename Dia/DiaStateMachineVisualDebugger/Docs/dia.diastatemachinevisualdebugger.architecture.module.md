---
schema: dia.module.v1
module_id: dia.statemachinevisualdebugger
name: DiaStateMachineVisualDebugger
owner_team: TBD
layer: domain/gameplay/tools
status: active
maturity: dev

path: Dia/DiaStateMachineVisualDebugger
language: cpp
parent_module_id: dia.root

summary: >
  IDebugDomain implementation for DiaStateMachine. Panel-only (no world drawers).
  Registers as ITransitionListener to cache guard results live.
  Entire module is #ifdef DIA_DEBUG guarded.

intent: >
  Separate static library that adds debug state visibility for DiaStateMachine
  without creating a dependency from DiaStateMachine onto DiaVisualDebugger.

responsibilities:
  - StateMachineVisualDebugger — IDebugDomain + ITransitionListener implementation
  - Caches TransitionEvent guard results; emits states, history, guard results as JSON
  - Entire public API is DIA_DEBUG guarded

non_responsibilities:
  - State machine execution — DiaStateMachine
  - Guard evaluation — DiaCondition
  - World-space entity labels — DiaEntityVisualDebugger

dependent_modules:
  - dia.statemachine

public_api:
  headers:
    - Dia/DiaStateMachineVisualDebugger/StateMachineVisualDebugger.h
  namespaces:
    - Dia::StateMachine

dependencies:
  required:
    - dia.core
    - dia.statemachine
    - dia.visualdebugger
  forbidden:
    - dia.entity
    - dia.applicationflow
    - dia.condition
    - dia.aibudget
---
