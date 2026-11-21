---
schema: dia.module.v1
module_id: draoi.hexworld
name: DraoiHexWorld
path: Draoi/DraoiHexWorld
parent_module_id: dia.root
layer: foundation/application
status: active
maturity: stub
version: "1.0"
dependencies:
  required:
    - dia.core
  forbidden: []
public_api:
  headers:
    - DraoiHexWorld/DraoiHexWorld.h
  namespaces:
    - Draoi::HexWorld::
  entry_points: []
responsibilities:
  - Content-free hex world foundation shared by Draoi world packages (cell vocabulary, hex grid, world data types, biome and stratum registries)
non_responsibilities:
  - World generation, weather, hydrology and ecology logic (sibling Draoi packages)
  - Game content such as biomes.json and strata.json (stays in the application)
  - Modules, wiring and execution ordering (application)
---
# DraoiHexWorld

Draoi core package for the hex world foundation. Static library, depends on Dia only. Currently a placeholder: the CoW world foundation moves in under @docs/specs/features/draoi/extract-hexworld-core.md (Tasks 2-3).
