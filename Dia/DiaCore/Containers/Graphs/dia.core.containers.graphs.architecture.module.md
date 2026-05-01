---
schema: dia.module.v1
module_id: dia.core.containers.graphs
name: Graphs
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaCore/Containers/Graphs
language: cpp
parent_module_id: dia.core.containers

summary: >
  Defines Graphs APIs (Graph, GraphC, GraphEdge, GraphNode) including: DynamicArrayC, Graph, GraphEdge, GraphNode.

intent: >
  Provide reusable Graphs building blocks with consistent semantics for higher-level systems.

responsibilities:
  - Expose primary types: DynamicArrayC, Graph, GraphEdge, GraphNode
  - Define and maintain the public header surface for this module
  - Provide lightweight operations with predictable behavior

non_responsibilities:
  - Domain-specific gameplay behavior
  - Rendering or platform integration concerns (unless this module is explicitly an adapter)
  - High-level orchestration (owned by higher-layer modules)

public_api:
  headers:
    - Dia/DiaCore/Containers/Graphs/Graph.h
    - Dia/DiaCore/Containers/Graphs/GraphC.h
    - Dia/DiaCore/Containers/Graphs/GraphEdge.h
    - Dia/DiaCore/Containers/Graphs/GraphNode.h
  namespaces: []
  entry_points:
    - DynamicArrayC
    - Graph
    - GraphEdge
    - GraphNode

dependencies:
  required:
    - dia.core.containers.arrays
    - dia.core.core
    - dia.core.crc
    - dia.core.memory
  forbidden: []
---
