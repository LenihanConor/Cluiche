---
schema: dia.module.v1
module_id: dia.graphics.cellfield
name: CellField
owner_team: TBD
layer: domain/visual/core
status: active
maturity: dev

path: Dia/DiaGraphics/CellField
language: cpp
parent_module_id: dia.graphics

summary: >
  Thread-safe CellField layer registry and grid geometry building. Provides CellFieldRegistry
  (copy-on-submit command queue with in-place coalescing and 8 MB cap), topology-specific geometry builders
  (HexFlatTopOddQBuilder), and ramp/palette baking utilities (BakeRamp, BakePalette).

intent: >
  Provide a backend-agnostic, thread-safe API for submitting and tracking hex grid and cell grid
  layers, with efficient geometry building and colour baking for the rendering backend.

responsibilities:
  - Implement thread-safe CellFieldRegistry with max 32 layers and 128×128 max grid size
  - Manage copy-on-submit command queue with in-place Update coalescing and 8 MB byte cap
  - Track layer handles (index + generation) with stale handle counting (not asserted)
  - Enforce retry-after-drain contract for dropped commands
  - Provide ICellFieldCommandSink interface for render-thread draining
  - Support topology-specific geometry builders (HexFlatTopOddQBuilder for hex.flat.oddq)
  - Bake ramps (256 RGBA8 texels, linear interpolation) and palettes (64 RGBA8 texels) for colour mapping

non_responsibilities:
  - GPU rendering or geometry upload (owned by DiaBgfx::CellFieldRenderer)
  - Picking or interaction (application layer)
  - Additional topology types beyond HexFlatTopOddQBuilder

dependent_modules:
  - dia.core.core
  - dia.core.crc
  - dia.core.debugdraw

public_api:
  headers:
    - Dia/DiaGraphics/CellField/CellFieldRegistry.h
    - Dia/DiaGraphics/CellField/CellGridDesc.h
    - Dia/DiaGraphics/CellField/ICellGridGeometryBuilder.h
    - Dia/DiaGraphics/CellField/HexFlatTopOddQBuilder.h
  namespaces:
    - Dia::Graphics
  entry_points:
    - CellFieldRegistry
    - CellFieldResult
    - CellFieldStats
    - CellFieldRampStop
    - CellFieldPaletteData
    - CellFieldRampData
    - CellFieldValueKind
    - ICellFieldCommandSink
    - CellGridDesc
    - ICellGridGeometryBuilder
    - HexFlatTopOddQBuilder
    - BakeRamp
    - BakePalette

dependencies:
  required:
    - dia.core.core
    - dia.core.crc
    - dia.core.debugdraw
  optional: []
  forbidden:
    - dia.bgfx
    - dia.graphics.frame
    - dia.pathfinding
---

## Topology and Geometry

HexFlatTopOddQBuilder generates 18 vertices of 36 bytes each per hexagon cell for the `hex.flat.oddq` topology.
Geometry is cached by topology + grid size in the render backend (DiaBgfx).

## Command Queue and Coalescing

Commands submitted via Create/Update/Destroy are copied and queued on the submission thread.
Multiple Update commands for the same layer are coalesced in-place before drain.
If the queue exceeds 8 MB, new commands are dropped and counted in stats; callers must retry after Drain().

## Stale Handle Semantics

Destroyed layers have their handle stop resolving immediately, but the slot is not recycled until Drain processes
the Destroy command. The slot's generation remains unchanged until recycled. Stale handles (previously valid but now
destroyed) are counted in stats rather than asserted, allowing safe retry-after-drain semantics.
