---
schema: dia.module.v1
module_id: dia.aibudget
name: DiaAIBudget
owner_team: TBD
layer: domain/gameplay/ai
status: active
maturity: dev

path: Dia/DiaAIBudget
language: cpp
parent_module_id: dia.root

summary: >
  Frame-budget scheduler for AI systems on SimPU. IAIBudgetedSystem interface + AIBudgetScheduler
  dispatches wall-clock time slices to registered systems in order. AIBudgetModule drives the
  scheduler each tick and reports utilisation metrics.

intent: >
  Prevents AI work from crowding out physics and simulation by capping total AI CPU time per frame.
  Any AI system implements IAIBudgetedSystem and registers with the scheduler to participate in
  time-sliced execution.

responsibilities:
  - IAIBudgetedSystem interface — GetSystemId() + UpdateBudgeted(float budgetMs)
  - AIBudgetScheduler — fixed-capacity (16) ordered dispatch; wall-clock time measurement via steady_clock; skips systems when budget exhausted
  - AIBudgetModule — Module subclass on SimPU; reads budgetUs from manifest; registers DiaObservation metrics
  - Metrics — ai.budget.used_us (Gauge), ai.budget.systems_run (Counter), ai.budget.systems_deferred (Counter)

non_responsibilities:
  - Priority tiers (Critical/Normal/Background) — DiaAIBudgetTiers feature spec
  - Per-entity work tracking — systems manage their own queues
  - Starvation prevention / aging — deferred to tiers
  - Cross-PU scheduling — all AI runs on SimPU
  - Adapters for PathfindingSystem, StateMachine — live in game code

dependent_modules: []

public_api:
  headers:
    - Dia/DiaAIBudget/IAIBudgetedSystem.h
    - Dia/DiaAIBudget/AIBudgetScheduler.h
    - Dia/DiaAIBudget/AIBudgetModule.h
  namespaces:
    - Dia::AIBudget
  entry_points:
    - IAIBudgetedSystem
    - AIBudgetScheduler
    - AIBudgetModule

dependencies:
  required:
    - dia.core
    - dia.applicationflow
    - dia.observation
  forbidden:
    - dia.blackboard
    - dia.statemachine
    - dia.order
    - dia.pathfinding
    - dia.geometry2d
    - dia.geometry3d
---
