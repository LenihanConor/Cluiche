---
module: dia.diaobjective.architecture.module.v1
id: DiaObjective
parent: DiaCondition
layer: gameplay
namespace: Dia::Objective
status: active
---

# DiaObjective

Data-driven gameplay goal tracking. Evaluates completion and failure conditions from any `IConditionContext`, latches on first-true, and notifies `IObjectiveObserver` subscribers.

## Public API

- `IObjectiveObserver` — Activated/Completed/Failed callbacks
- `RewardPayload` / `RewardEntry` — generic key-value reward bag
- `ObjectiveDef` — immutable data record (id, classification, completion/failure ConditionExpr, reward, prerequisites)
- `ObjectiveSet` — runtime collection: LoadFromJson, Validate, Evaluate, observer management, inspection
- `ObjectiveSetComponent` — DIA_COMPONENT entity wrapper

## Dependencies

- DiaCondition — ConditionExpr, IConditionContext, ConditionRegistry
- DiaCore — StringCRC, DynamicArrayC, Json, DIA_ASSERT
- DiaEntity — IComponent, DIA_COMPONENT, DIA_READONLY
