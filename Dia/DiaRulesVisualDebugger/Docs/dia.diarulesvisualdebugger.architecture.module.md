---
schema: dia.module.v1
module_id: dia.rulesvisualdebugger
name: DiaRulesVisualDebugger
owner_team: TBD
layer: domain/gameplay/ai
status: active
maturity: dev

path: Dia/DiaRulesVisualDebugger
language: cpp
parent_module_id: dia.root

summary: >
  IDebugDomain implementation for DiaRules. Panel-only (no world drawers).
  Emits per-rule fired/not-fired table with dispatched actions for the current
  frame's Evaluate() call. Entire module is #ifdef DIA_DEBUG guarded.

intent: >
  Separate static library that adds debug rule evaluation visibility for DiaRules
  without creating a dependency from DiaRules onto DiaVisualDebugger.

responsibilities:
  - RulesVisualDebugger — IDebugDomain implementation
  - Full rule inventory emission (fired + unfired) cross-referenced against GetLastFireReport()
  - FireLog drawer toggle gates the rules[] section
  - Entire public API is DIA_DEBUG guarded

non_responsibilities:
  - Rule condition evaluation — DiaCondition / DiaRules
  - Action dispatch — DiaRules Evaluate()
  - Entity position or world-space drawing — no world-space anchor for rule evaluation

dependent_modules:
  - dia.rules

public_api:
  headers:
    - Dia/DiaRulesVisualDebugger/RulesVisualDebugger.h
  namespaces:
    - Dia::Rules

dependencies:
  required:
    - dia.core
    - dia.rules
    - dia.visualdebugger
  forbidden:
    - dia.entity
    - dia.applicationflow
    - dia.statemachine
    - dia.aibudget
    - dia.condition
---
