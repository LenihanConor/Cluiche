---
schema: dia.module.v1
module_id: draoi.hexworldweather
name: DraoiHexWorldWeather
path: Draoi/DraoiHexWorldWeather
parent_module_id: draoi.root
layer: draoi
status: active
maturity: alpha
version: "1.0"
dependencies:
  required:
    - draoi.hexworld
    - dia.core
    - dia.maths
    - dia.attribute
    - dia.parallelexecutor
    - dia.geometry2d
    - dia.observation
  forbidden: []
public_api:
  headers:
    - DraoiHexWorldWeather/DraoiHexWorldWeather.h
  namespaces:
    - Draoi::HexWorld::Weather::
  entry_points: []
responsibilities:
  - Pure weather simulation for a hex world (A4): data and parameter types, seeding, the cellular-automaton pipeline, save codec data half
non_responsibilities:
  - Modules, bus, save participants, debug domains, SimPU or step-chain wiring (application)
  - Game content such as the parameter sidecar, baseline table and presets (stays in the application)
---
# DraoiHexWorldWeather

Draoi package for the A4 weather simulation. Static library, depends on Dia and DraoiHexWorld. Built under @docs/specs/applications/cow/systems/a4-weather/a4-weather.md (Draoi layer amendment); Feature 1 plan: @docs/specs/applications/cow/systems/a4-weather/weather-data.plan.md.
