---
schema: dia.module.v1
module_id: draoi.root
name: Draoi
owner_team: TBD
layer: draoi
status: active
maturity: dev

path: Draoi
language: cpp

summary: >
  Groups the Draoi packages: reusable gameplay libraries with no one game's content
  (hex world foundation, terrain generation, game speed). Layer order is Application -> Draoi -> Dia.

intent: >
  Provide a module boundary and navigation surface for the Draoi layer. Draoi packages are plain
  libraries: no modules, no wiring, scheduler-agnostic sims. Dia must never depend on Draoi.

responsibilities:
  - Define the module boundary and ownership for the Draoi layer
  - Expose stable module identifiers for the Draoi packages

non_responsibilities:
  - Implementing functionality directly (owned by the packages)
  - Module wiring and execution ordering (application)
  - Game content (application)

dependent_modules:
  - draoi.gamespeed
  - draoi.hexworld
  - draoi.hexworldterrain
  - draoi.hexworldweather
---
# Draoi

Root grouping module for the Draoi layer. See @docs/specs/applications/draoi/draoi.md.
