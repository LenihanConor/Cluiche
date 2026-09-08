---
schema: dia.module.v1
module_id: dia.condition
name: DiaCondition
owner_team: TBD
layer: domain/gameplay/core
status: active
maturity: dev

path: Dia/DiaCondition
language: cpp
parent_module_id: dia.root

summary: >
  Shared boolean expression evaluation layer. IConditionContext interface + ConditionRegistry
  (callback-based accessor map) + ConditionExpr (JSON-loadable AND/OR/NOT tree) +
  ConditionGuardAdapter (wires expressions into DiaStateMachine guards).

intent: >
  Provides a data-driven way to express and evaluate conditions against any data source
  (blackboard slots, game state, component values) without coupling to a specific runtime.
  DiaRules and DiaUtilityAI consume this as a shared evaluation primitive.

responsibilities:
  - IConditionContext — pure virtual GetFloat/GetBool by StringCRC pair
  - ConditionRegistry — concrete IConditionContext with registered accessor callbacks
  - ConditionExpr — JSON-loadable AND/OR/NOT boolean expression tree, evaluated via IConditionContext
  - ConditionGuardAdapter — RegisterAsGuard() free function that wires ConditionExpr into DiaStateMachine CallbackRegistry
  - Test utilities under DiaCondition/Testing/: MockConditionContext, AssertExprResult

non_responsibilities:
  - Blackboard slot registration or typed slot ownership — DiaBlackboard
  - BlackboardConditionContext implementation — DiaBlackboard owns that bridge
  - Rule set evaluation or action dispatch — DiaRules
  - Score-based utility evaluation — DiaUtilityAI
  - Thread-safe evaluation — caller synchronizes

dependent_modules: []

public_api:
  headers:
    - Dia/DiaCondition/IConditionContext.h
    - Dia/DiaCondition/ConditionRegistry.h
    - Dia/DiaCondition/ConditionExpr.h
    - Dia/DiaCondition/ConditionGuardAdapter.h
    - Dia/DiaCondition/Testing/ConditionTestHelpers.h
  namespaces:
    - Dia::Condition
    - Dia::Condition::Testing
  entry_points:
    - IConditionContext
    - ConditionRegistry
    - ConditionExpr
    - RegisterAsGuard

dependencies:
  required:
    - dia.core
    - dia.statemachine
  forbidden:
    - dia.blackboard
    - dia.rules
    - dia.utilityai
    - dia.applicationflow
---
