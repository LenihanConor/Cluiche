---
schema: dia.module.v1
module_id: dia.steering
name: DiaSteering
owner_team: TBD
layer: domain/gameplay/core
status: active
maturity: dev

path: Dia/DiaSteering
language: cpp
parent_module_id: dia.root

summary: >
  Local movement behaviour system for mass-unit steering — eight stateless
  behaviour free functions (Seek/Flee/Arrive/Wander/Pursue/Evade/ObstacleAvoidance/Separation),
  priority-group SteeringPipeline, and SteeringSystem agent registry + output cache.

intent: >
  Provides the micro-movement layer below DiaPathfinding/DiaFlowField. Callers
  feed flow-field samples or path waypoints as Seek targets; DiaSteering returns
  per-agent desired velocity each frame. One Dijkstra result can drive 100+ agents
  with independent avoidance and separation.

responsibilities:
  - SteeringAgent — per-agent state (position, velocity, maxSpeed, maxForce)
  - Seek / Flee — normalised desired velocity toward / away from target position
  - Arrive — decelerating Seek within configurable slowingRadius
  - Wander — smooth random drift on a projected circle; caller owns wander angle state
  - Pursue / Evade — Seek/Flee to predicted future position (targetPos + velocity * dt)
  - ObstacleAvoidance — lateral steer away from nearest obstacle intersecting detection box
  - Separation — inverse-distance push away from neighbours within desiredSeparation radius
  - SteeringPipeline — priority-group composition: AddContribution/Clear/Evaluate; first non-zero group wins (SD-003)
  - SteeringSystem — agent registry and output cache: AddAgent/RemoveAgent/UpdateAgentState/GetOutput/Update
  - DIA_LOG_INFO on agent add/remove; DIA_LOG_WARNING on zero output for registered agent
  - Test utilities under Testing/ subdirectory

non_responsibilities:
  - Position integration (DiaSteering outputs desired velocity; callers own the transform)
  - Macro pathfinding (DiaPathfinding / DiaFlowField provide routes)
  - Dense crowd avoidance at scale (RVO/ORCA — future DiaRVO)
  - Formation / squad positioning (higher-level concern)
  - DiaAIBudget integration (caller manages update frequency)
  - Visual debugger overlay (future DiaSteeringVisualDebugger)
  - Thread-safe SteeringSystem (single-threaded, called from SimPU)

dependent_modules:
  - dia.core
  - dia.maths
  - dia.observation

public_api:
  headers:
    - Dia/DiaSteering/SteeringAgent.h
    - Dia/DiaSteering/Behaviours.h
    - Dia/DiaSteering/SteeringPipeline.h
    - Dia/DiaSteering/SteeringSystem.h
    - Dia/DiaSteering/SteeringLogChannel.h
  namespaces:
    - Dia::Steering
  entry_points:
    - SteeringAgent
    - Seek
    - Flee
    - Arrive
    - Wander
    - Pursue
    - Evade
    - ObstacleAvoidance
    - Separation
    - SteeringPipeline
    - SteeringSystem
    - SteeringAgentId

dependencies:
  required:
    - dia.core
    - dia.maths
    - dia.observation
  forbidden:
    - dia.pathfinding
    - dia.flowfield
    - dia.blackboard
    - dia.application
    - dia.graphics
    - dia.entity
---
