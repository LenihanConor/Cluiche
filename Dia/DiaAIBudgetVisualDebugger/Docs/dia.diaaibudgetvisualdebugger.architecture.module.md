---
schema: dia.module.v1
module_id: dia.aibudgetvisualdebugger
name: DiaAIBudgetVisualDebugger
owner_team: TBD
layer: domain/gameplay/tools
status: active
maturity: dev

path: Dia/DiaAIBudgetVisualDebugger
language: cpp
parent_module_id: dia.root

summary: >
  IDebugDomain implementation for DiaAIBudget. Panel-only (no world drawers).
  Emits budget fill bar (usedMs/budgetMs) and per-system run/deferred timing
  table. Entire module is #ifdef DIA_DEBUG guarded.

intent: >
  Separate static library that adds debug budget visibility for DiaAIBudget
  without creating a dependency from DiaAIBudget onto DiaVisualDebugger.

responsibilities:
  - AIBudgetVisualDebugger — IDebugDomain implementation
  - Budget fill bar emission (usedMs, budgetMs, fillPct)
  - Per-system timing table from AIBudgetResult.perSystem
  - Entire public API is DIA_DEBUG guarded

non_responsibilities:
  - Budget scheduling — DiaAIBudget
  - Per-system AI logic — individual IAIBudgetedSystem implementations

dependent_modules:
  - dia.aibudget

public_api:
  headers:
    - Dia/DiaAIBudgetVisualDebugger/AIBudgetVisualDebugger.h
  namespaces:
    - Dia::AIBudget

dependencies:
  required:
    - dia.core
    - dia.aibudget
    - dia.visualdebugger
  forbidden:
    - dia.entity
    - dia.applicationflow
    - dia.htn
    - dia.steering
---
