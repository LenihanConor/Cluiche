---
schema: dia.module.v1
module_id: dia.observation
name: DiaObservation
owner_team: TBD
layer: foundation/services
status: active
maturity: dev

path: Dia/DiaObservation
language: cpp
parent_module_id: dia.root

summary: >
  Observation pillars: logs (DIA_LOG_*), traces, profiling (DIA_PROFILE_SCOPE), metrics (MetricRegistry),
  health (IHealthReporter), frame capture and job observation.

intent: >
  One shared instrumentation surface so every Dia, Draoi and application module reports logs, metrics,
  health and performance the same way. This doc exists so module dependency checks can resolve includes
  of DiaObservation/... to a module id.

responsibilities:
  - Logging, trace zones, profile scopes, metric registry and health reporter registry
  - Observation configuration loading and frame capture plumbing

non_responsibilities:
  - Transport and display of observations (DiaDebugServer, visual debuggers, editor panels)

dependencies:
  required:
    - dia.core
    - dia.threading
---
# DiaObservation

Instrumentation library shared by all layers. Module doc added by docs/refactors/draoi-tooling/plan.md Task 5 so `dia check deps` can resolve `DiaObservation/...` includes.
