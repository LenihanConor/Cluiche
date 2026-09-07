---
schema: dia.module.v1
module_id: dia.sensor
name: DiaSensor
parent_module_id: dia.entity
layer: domain/gameplay/core
path: Dia/DiaSensor
status: active
maturity: dev
version: "1.0"
dependencies:
  required:
    - dia.core
    - dia.maths
    - dia.entity
    - dia.entityspatial
    - dia.blackboard
    - dia.observation
  forbidden: []
public_api:
  headers:
    - DiaSensor/SensorResultsComponent.h
    - DiaSensor/SensorModule.h
    - DiaSensor/SensorBlackboardAdapter.h
    - DiaSensor/SightSensorComponent.h
    - DiaSensor/ProximitySensorComponent.h
    - DiaSensor/DamageSensorComponent.h
    - DiaSensor/SoundSensorComponent.h
  namespaces:
    - Dia::Sensor::
  entry_points:
    - SensorModule
    - SensorResultsComponent
    - SensorBlackboardAdapter
responsibilities:
  - Entity perception framework (sight, proximity, damage, sound)
  - Writes raw perception results to SensorResultsComponent per tick
  - Distils raw results to blackboard slots via SensorBlackboardAdapter
  - Manages per-sensor tick countdown to spread sensor cost across frames
  - Defines DamageReceivedComponent stub so DiaSensor does not couple to a combat system
non_responsibilities:
  - Does not own blackboard slot types as external contracts
  - Does not couple to a specific combat system (DamageReceivedComponent is a stub that combat fills)
  - Does not implement AI decision logic (that is DiaRules / DiaUtilityAI)
---
# DiaSensor

Entity perception framework for the Dia engine. Provides sight, proximity, damage, and sound sensors that write raw results into `SensorResultsComponent` and distil them to blackboard slots via `SensorBlackboardAdapter`.

Each sensor component manages its own tick countdown so perception cost is spread across frames. The `SensorModule` drives all sensor components each frame.

`DamageReceivedComponent` is defined here as a minimal data stub — the combat system writes into it and DiaSensor reads it. This deliberately avoids coupling DiaSensor to any specific combat implementation.
