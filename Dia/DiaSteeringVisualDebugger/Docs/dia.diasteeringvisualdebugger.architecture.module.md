---
schema: dia.module.v1
module_id: dia.steeringvisualdebugger
name: DiaSteeringVisualDebugger
owner_team: TBD
layer: domain/gameplay/tools
status: active
maturity: dev

path: Dia/DiaSteeringVisualDebugger
language: cpp
parent_module_id: dia.root

summary: >
  IDebugDomain implementation for DiaSteering. World-space domain with three
  drawers: velocity arrows (current + desired), separation radius circles,
  and detection box outlines. Entire module is #ifdef DIA_DEBUG guarded.

intent: >
  Separate static library adding steering debug visibility without creating
  a compile-time dependency from DiaSteering onto DiaVisualDebugger.

responsibilities:
  - SteeringVisualDebugger — IDebugDomain implementation
  - VelocityArrowsDrawer — current/desired velocity lines per agent
  - SeparationRadiusDrawer — separation radius circles per agent
  - DetectionBoxDrawer — detection box outlines per agent (off by default)

non_responsibilities:
  - Steering pipeline execution — DiaSteering
  - Entity position tracking beyond SteeringAgent

dependent_modules:
  - dia.steering

public_api:
  headers:
    - Dia/DiaSteeringVisualDebugger/SteeringVisualDebugger.h
  namespaces:
    - Dia::Steering

dependencies:
  required:
    - dia.core
    - dia.steering
    - dia.visualdebugger
  forbidden:
    - dia.entity
    - dia.applicationflow
    - dia.aibudget
---
