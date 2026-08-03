---
schema: dia.module.v1
module_id: dia.scalarfield
name: DiaScalarField
owner_team: TBD
layer: domain/gameplay/core
status: active
maturity: dev

path: Dia/DiaScalarField
language: cpp
parent_module_id: dia.root

summary: >
  Topology-agnostic, grid-sized float field primitive for the Dia engine. Stores a
  scalar value per cell and provides diffusion/decay propagation, write shapes (point/
  radial/box), gradient queries, spatial queries, and weighted field combination.
  Grid topology (square vs hex) and propagation policy are template parameters.

intent: >
  Provides a generic spatial data structure for influence maps, danger fields, resource
  fields, and other per-cell float data. All AI/faction semantics stay in game code;
  DiaScalarField is the pure spatial primitive.

responsibilities:
  - CFieldTopology concept (ForEachNeighbour + GetCellCount)
  - SquareFieldTopology (4/8-connected, stateless, constexpr offsets)
  - HexFieldTopology (axial coords, 6-connected, stateless)
  - DiaScalarField<Topology, Policy> double-buffered float field
  - UniformDecayPolicy (diffusion factor + decay rate)
  - Blocked cell bitset mask (propagation stops at blocked cells)
  - Static modifier float map (pre-baked per-cell multiplier)
  - Value clamping (configurable [min, max] per field)
  - Write shapes API: WritePoint, WriteRadial, WriteBox (queued, flushed before Tick)
  - Tick() — flush writes, propagate, swap buffers, clamp
  - GetGradient() — central difference → normalized Vector2
  - FindLocalMaxima / FindCellsAboveThreshold spatial queries
  - Combine() static utility — weighted multi-field combination
  - DIA_LOG_INFO on field construction
  - Test utilities under Testing/ subdirectory
  - Optional adaptors (header-only): RulesPropagationPolicy, ScalarFieldOverlay

non_responsibilities:
  - Faction wiring or influence map semantics
  - Per-entity AI decisions
  - Concurrent writes (single-writer model in v1)
  - Pathfinding graph queries
  - Visual rendering beyond debug overlay

dependent_modules:
  - dia.core
  - dia.maths
  - dia.observation

public_api:
  headers:
    - Dia/DiaScalarField/CFieldTopology.h
    - Dia/DiaScalarField/CellIndex.h
    - Dia/DiaScalarField/SquareFieldTopology.h
    - Dia/DiaScalarField/HexFieldTopology.h
    - Dia/DiaScalarField/UniformDecayPolicy.h
    - Dia/DiaScalarField/DiaScalarField.h
    - Dia/DiaScalarField/ScalarFieldLogChannel.h
  namespaces:
    - Dia::ScalarField

dependencies:
  required:
    - dia.core
    - dia.maths
    - dia.observation
  optional:
    - dia.rules (RulesPropagationPolicy adaptor only)
    - dia.visualdebugger (ScalarFieldOverlay adaptor only)
  forbidden:
    - dia.pathfinding
    - dia.flowfield
    - dia.geometry2d
    - dia.blackboard
    - dia.entity
    - dia.application
    - dia.graphics
---
