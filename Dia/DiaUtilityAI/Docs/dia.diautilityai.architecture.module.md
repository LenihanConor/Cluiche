---
schema: dia.module.v1
module_id: dia.utilityai
name: DiaUtilityAI
owner_team: TBD
layer: domain/gameplay/ai
status: active
maturity: dev

path: Dia/DiaUtilityAI
language: cpp
parent_module_id: dia.root

summary: >
  Score-based action selection layer. ResponseCurve (6 shapes), ActionDef (prerequisite guard + scorers),
  UtilitySet (winner-takes-all Evaluate, async EvaluateAsync, JSON-loadable), GroupConsiderationContext
  (group-wide max_concurrent enforcement), UtilitySetComponent (IComponent wrapper for DiaEntity),
  PersonalityProfile (per-action score multipliers + eval-period ticks).

intent: >
  Provides a data-driven utility AI layer that scores all eligible ActionDefs each evaluation frame
  and selects the single highest-scoring action (winner-takes-all). Actions are guarded by ConditionExpr
  prerequisites (DiaCondition), dispatched via RuleActionRegistry (DiaRules), and optionally time-sliced
  as a one-shot via SimTimeBudget (DiaSimTime).

responsibilities:
  - ResponseCurve — maps normalised float [0,1] input to score [0,1] via 6 named easing shapes
  - ActionDef — ConditionExpr prerequisite + DynamicArrayC<ScorerDef> + cooldown + max_concurrent
  - UtilitySet — JSON-loadable immutable collection; winner-takes-all Evaluate() + EvaluateAsync()
  - GroupConsiderationContext — tracks active action counts; enforces max_concurrent group-wide
  - UtilitySetComponent — IComponent attaching UtilitySet + registry + optional group context
  - PersonalityProfile — per-action score multipliers + eval period ticks; optional parameter to Evaluate()
  - Test utilities under DiaUtilityAI/Testing/: AssertWinner, AssertNoSelection, AssertCurveOutput

non_responsibilities:
  - All-matching rule evaluation — DiaRules
  - Condition/expression evaluation logic — DiaCondition
  - Blackboard slot registration — DiaBlackboard
  - Budget enforcement or time-slicing logic — DiaSimTime
  - Thread-safe evaluation — caller synchronizes
  - Score overlay visualisation — DiaUtilityAIVisualDebugger (separate module)

dependent_modules:
  - dia.condition
  - dia.rules
  - dia.simtime
  - dia.entity

public_api:
  headers:
    - Dia/DiaUtilityAI/ResponseCurve.h
    - Dia/DiaUtilityAI/ActionDef.h
    - Dia/DiaUtilityAI/GroupConsiderationContext.h
    - Dia/DiaUtilityAI/UtilitySet.h
    - Dia/DiaUtilityAI/UtilitySetComponent.h
    - Dia/DiaUtilityAI/PersonalityProfile.h
    - Dia/DiaUtilityAI/Testing/UtilityTestHelpers.h
  namespaces:
    - Dia::UtilityAI
    - Dia::UtilityAI::Testing

dependencies:
  required:
    - dia.core
    - dia.condition
    - dia.rules
    - dia.simtime
    - dia.entity
  forbidden:
    - dia.blackboard
    - dia.applicationflow
    - dia.statemachine
    - dia.visualdebugger
---
