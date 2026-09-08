---
schema: dia.module.v1
module_id: dia.utilityaivisualdebugger
name: DiaUtilityAIVisualDebugger
owner_team: TBD
layer: domain/gameplay/tools
status: active
maturity: dev

path: Dia/DiaUtilityAIVisualDebugger
language: cpp
parent_module_id: dia.root

summary: >
  Score overlay visualiser for DiaUtilityAI. UtilityScoreDrawer implements IVisualDebugger,
  reads last-frame scores from UtilitySet, draws per-action score bars + ImGui table.
  Entire module is #ifdef DIA_DEBUG guarded.

intent: >
  Separate static library that adds debug score visualisation for DiaUtilityAI without
  creating a dependency from DiaUtilityAI onto DiaVisualDebugger or DiaGraphics.
  Follows the same pattern as DiaRigidBody2DVisualDebugger.

responsibilities:
  - UtilityScoreDrawer — IVisualDebugger implementation; reads GetLastFrameScores(); draws per-action bars and ImGui table
  - Entire public API is DIA_DEBUG guarded

non_responsibilities:
  - Utility score evaluation — DiaUtilityAI
  - Action dispatch — DiaRules
  - General debug drawing infrastructure — DiaCore

dependent_modules:
  - dia.utilityai

public_api:
  headers:
    - Dia/DiaUtilityAIVisualDebugger/UtilityScoreDrawer.h
  namespaces:
    - Dia::UtilityAI

dependencies:
  required:
    - dia.core
    - dia.utilityai
  forbidden:
    - dia.rules
    - dia.condition
    - dia.aibudget
    - dia.entity
    - dia.applicationflow
---
