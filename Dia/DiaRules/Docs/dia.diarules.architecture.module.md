---
schema: dia.module.v1
module_id: dia.rules
name: DiaRules
owner_team: TBD
layer: domain/gameplay/core
status: active
maturity: dev

path: Dia/DiaRules
language: cpp
parent_module_id: dia.root

summary: >
  Forward-chaining rule engine. RuleActionRegistry (open handler table), RuleDef (condition guard + action IDs),
  RuleSet (JSON-loadable, all-matching Evaluate()), RuleSetComponent (IComponent wrapper for entities).

intent: >
  Provides a stateless, data-driven rule evaluation layer that fires every rule whose ConditionExpr guard
  passes in a single Evaluate() call. Actions are bound in code via a StringCRC registry; conditions are
  evaluated via IConditionContext (from DiaCondition). Consumers are game code and DiaUtilityAI.

responsibilities:
  - RuleActionRegistry — explicit handler table mapping StringCRC action names to callbacks
  - RuleDef — ConditionExpr guard + DynamicArrayC of action StringCRCs + optional rule ID
  - RuleSet — JSON-loadable immutable collection; all-matching Evaluate() returns count of rules fired
  - RuleSetComponent — IComponent attaching a RuleSet + RuleActionRegistry to a DiaEntity entity
  - Test utilities under DiaRules/Testing/: FireRuleSet, AssertActionsFired, AssertActionNotFired

non_responsibilities:
  - Per-action cooldowns or rate limiting — DiaUtilityAI
  - Score-based action selection — DiaUtilityAI
  - Budget-aware time-sliced evaluation — DiaAIBudget + DiaUtilityAI
  - Condition/expression evaluation logic — DiaCondition
  - Blackboard slot registration — DiaBlackboard
  - Thread-safe evaluation — caller synchronizes

dependent_modules:
  - dia.condition
  - dia.entity

public_api:
  headers:
    - Dia/DiaRules/RuleActionRegistry.h
    - Dia/DiaRules/RuleDef.h
    - Dia/DiaRules/RuleSet.h
    - Dia/DiaRules/RuleSetComponent.h
    - Dia/DiaRules/Testing/RulesTestHelpers.h
  namespaces:
    - Dia::Rules
    - Dia::Rules::Testing

dependencies:
  required:
    - dia.core
    - dia.condition
    - dia.entity
  forbidden:
    - dia.blackboard
    - dia.utilityai
    - dia.aibudget
    - dia.applicationflow
    - dia.statemachine
---
