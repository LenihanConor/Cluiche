---
schema: dia.module.v1
module_id: dia.htn
name: DiaHTN
owner_team: TBD
layer: domain/gameplay/ai
status: active
maturity: dev

path: Dia/DiaHTN
language: cpp
parent_module_id: dia.root

summary: >
  Depth-first forward-chaining HTN planner. HTNDomain (JSON-loaded, immutable),
  HTNPlanner (stateless), HTNPlan (cursor + divergence snapshot), OperatorRegistry (explicit, not singleton),
  RegisterRuleActionAsOperator bridge, HTNPlannerComponent (IComponent wrapper).

intent: >
  Provides a stateless, data-driven HTN planning layer that decomposes high-level goal tasks
  into ordered sequences of primitive operators. Operators are bound via OperatorRegistry
  (StringCRC → OperatorFn with TaskResult lifecycle). Re-planning is always caller-triggered.
  Async path submits to SimTimeBudget's one-shot queue (DiaSimTime); caller drives Tick() each frame.

responsibilities:
  - HTNDomain — JSON-loadable compound + primitive task definition; immutable after load; validates cycles + dangling refs
  - HTNPlanner — stateless depth-first forward-chaining planner with ordered method selection
  - HTNPlan — ordered flat operator sequence with execution cursor and world-state divergence check
  - OperatorRegistry — explicit handler table mapping StringCRC operator names to OperatorFn callbacks
  - RegisterRuleActionAsOperator — bridge adapter wrapping RuleActionFn as instant-succeed OperatorFn
  - HTNPlannerComponent — IComponent attaching domain + registry + active plan to a DiaEntity; caller drives Tick()
  - Test utilities under DiaHTN/Testing/: MockHTNContext, AssertPlanEquals, AssertPlanFails, AssertPlanOperators

non_responsibilities:
  - Executing operator callbacks each tick — HTNPlannerComponent provides Tick(); plan execution is caller-driven
  - Automatic re-planning on divergence detection — callers detect divergence and call Replan()/ReplanAsync()
  - Goal priority ordering or multi-goal management — caller selects the active root task
  - Blackboard slot registration or world state writing — DiaBlackboard + game code
  - State machine integration — caller wires FSM transitions from HTN output
  - Behaviour tree nodes or GOAP operators — separate systems
  - Visual debugging — future DiaHTNVisualDebugger
  - Thread-safe planning or execution — caller synchronises

dependent_modules:
  - dia.condition
  - dia.rules
  - dia.simtime
  - dia.entity

public_api:
  headers:
    - Dia/DiaHTN/TaskResult.h
    - Dia/DiaHTN/OperatorRegistry.h
    - Dia/DiaHTN/RuleActionBridge.h
    - Dia/DiaHTN/HTNDomain.h
    - Dia/DiaHTN/HTNPlan.h
    - Dia/DiaHTN/HTNPlanner.h
    - Dia/DiaHTN/HTNPlannerComponent.h
    - Dia/DiaHTN/Testing/HTNTestHelpers.h
  namespaces:
    - Dia::HTN
    - Dia::HTN::Testing

dependencies:
  required:
    - dia.core
    - dia.condition
    - dia.simtime
    - dia.entity
    - dia.observation
  optional:
    - dia.rules
  forbidden:
    - dia.blackboard
    - dia.applicationflow
    - dia.statemachine
---
