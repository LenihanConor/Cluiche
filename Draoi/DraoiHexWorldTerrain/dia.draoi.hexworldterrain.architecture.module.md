---
schema: dia.module.v1
module_id: draoi.hexworldterrain
name: DraoiHexWorldTerrain
path: Draoi/DraoiHexWorldTerrain
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
    - dia.pathfinding
    - dia.attribute
    - dia.observation
  forbidden: []
public_api:
  headers:
    - DraoiHexWorldTerrain/TerrainGenerator.h
    - DraoiHexWorldTerrain/BiomeClassify.h
    - DraoiHexWorldTerrain/RegionsAndSites.h
    - DraoiHexWorldTerrain/SiteRegistry.h
    - DraoiHexWorldTerrain/GuaranteeSpec.h
    - DraoiHexWorldTerrain/WorldGenSession.h
    - DraoiHexWorldTerrain/WorldJson.h
    - DraoiHexWorldTerrain/WorldOverlayGeometry.h
  namespaces:
    - Draoi::HexWorld::
  entry_points: []
responsibilities:
  - S-1 style hex world generator (terrain, biome classification, regions, sites, validation, session, JSON and files)
non_responsibilities:
  - Game content such as guarantees.json, biomes.json and strata.json (stays in the application)
  - Modules, wiring and execution ordering (application)
  - View host
---
# DraoiHexWorldTerrain

Draoi package for the hex world terrain generator. Static library, depends on Dia and DraoiHexWorld. Source moved in from CoW/Shared/WorldGeneration under @docs/specs/features/draoi/extract-hexworld-terrain.md.
