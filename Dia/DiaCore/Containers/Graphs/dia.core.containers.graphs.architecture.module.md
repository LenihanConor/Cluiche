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
  Defines Graph and DirectedGraph APIs. Includes fixed-capacity undirected (Graph, GraphEdge, GraphNode)
  and directed (DirectedGraph, DirectedGraphNode, DirectedGraphEdge, DirectedGraphPolicy) containers
  with BFS, DFS, topological sort, and compile-time policy specialisation.

intent: >
  Provide reusable graph building blocks with consistent semantics for higher-level systems.

responsibilities:
  - Expose undirected graph types: Graph, GraphEdge, GraphNode
  - Expose directed graph types: DirectedGraph, DirectedGraphNode, DirectedGraphEdge, DirectedGraphPolicy
  - Provide BFS, DFS, topological sort (Kahn's), and cycle detection on DirectedGraph
  - Provide compile-time policies (None, ReverseEdgeCache, AcyclicEnforced) for DirectedGraph
  - Define and maintain the public header surface for this module
  - Provide lightweight, zero-heap-allocation operations with predictable behaviour

non_responsibilities:
  - Domain-specific gameplay behaviour
  - Rendering or platform integration concerns
  - High-level orchestration (owned by higher-layer modules)

public_api:
  headers:
    - Dia/DiaCore/Containers/Graphs/DirectedGraph.h
    - Dia/DiaCore/Containers/Graphs/DirectedGraphEdge.h
    - Dia/DiaCore/Containers/Graphs/DirectedGraphNode.h
    - Dia/DiaCore/Containers/Graphs/DirectedGraphPolicy.h
    - Dia/DiaCore/Containers/Graphs/Graph.h
    - Dia/DiaCore/Containers/Graphs/GraphC.h
    - Dia/DiaCore/Containers/Graphs/GraphEdge.h
    - Dia/DiaCore/Containers/Graphs/GraphNode.h
  namespaces:
    - Dia::Core::Containers
    - Dia::Core::Containers::GraphPolicy
  entry_points:
    - DirectedGraph
    - DirectedGraphEdge
    - DirectedGraphNode
    - GraphPolicy::None
    - GraphPolicy::ReverseEdgeCache
    - GraphPolicy::AcyclicEnforced
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
