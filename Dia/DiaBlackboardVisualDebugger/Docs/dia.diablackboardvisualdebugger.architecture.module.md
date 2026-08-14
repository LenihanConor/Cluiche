---
schema: dia.module.v1
module_id: dia.blackboardvisualdebugger
name: DiaBlackboardVisualDebugger
owner_team: TBD
layer: domain/gameplay/ai
status: active
maturity: dev

path: Dia/DiaBlackboardVisualDebugger
language: cpp
parent_module_id: dia.root

summary: >
  IDebugDomain implementation for DiaBlackboard. Panel-only (no world drawers).
  Emits slot table (key, type, value) via VisitSlots. Supports per-type display
  formatters via RegisterFormatter. Entire module is #ifdef DIA_DEBUG guarded.

intent: >
  Separate static library that adds debug slot visibility for DiaBlackboard
  without creating a dependency from DiaBlackboard onto DiaVisualDebugger.

responsibilities:
  - BlackboardVisualDebugger — IDebugDomain implementation
  - Slot table emission via VisitSlots; hex fallback for unformatted types
  - RegisterFormatter for per-type display callbacks
  - Entire public API is DIA_DEBUG guarded

non_responsibilities:
  - Blackboard read/write logic — DiaBlackboard
  - Per-type value semantics — caller-registered formatters only

dependent_modules:
  - dia.blackboard

public_api:
  headers:
    - Dia/DiaBlackboardVisualDebugger/BlackboardVisualDebugger.h
  namespaces:
    - Dia::Blackboard

dependencies:
  required:
    - dia.core
    - dia.blackboard
    - dia.visualdebugger
  forbidden:
    - dia.entity
    - dia.applicationflow
    - dia.statemachine
    - dia.aibudget
---
