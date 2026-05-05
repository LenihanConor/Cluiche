# System Spec: DiaCore

## Parent Application
@docs/specs/applications/dia.md

## Summary

DiaCore is the foundation library that every other Dia module depends on. It provides the building
blocks for all engine systems: custom fixed-capacity containers (arrays, hash tables, graphs, strings,
link lists), the type system (runtime reflection and serialization), memory utilities, compile-time
CRC string hashing, logging/assertions, time, file paths, and JSON parsing. No other engine module
may undercut DiaCore — it has no internal Dia dependencies.

DiaCore is not a system in the ProcessingUnit sense. It is a compiled static library. Its public
headers define the shared vocabulary of the entire engine.

## Responsibilities

**Owns:**
- Custom fixed-capacity containers: `DynamicArrayC`, `HashTableC`, `Graph`, `DirectedGraph`, `LinkList`, strings, bit flags
- Type system: runtime RTTI, field reflection, `TypeJsonSerializer`, `TypeVariableAttributes`
- Memory: allocation helpers, pooling primitives
- CRC / StringCRC: compile-time and runtime CRC32 hashing with debug string storage
- Core: assertions (`DIA_ASSERT`), call stack capture, logging primitives
- Time / Timer: frame-limited timers, high-resolution clock
- FilePath / PathStore: path representation, aliasing, deferred resolution
- JSON: jsoncpp wrapper (`DiaCore/Json/`)

**Does NOT own:**
- Application lifecycle (DiaApplication)
- Rendering, windowing, input (DiaGraphics, DiaWindow, DiaInput)
- Build tooling (DiaAPI, DiaCLI)
- High-level gameplay or physics systems

## Features

| # | Feature | Size | Description | Spec |
|---|---------|------|-------------|------|
| 1 | DirectedGraph Container | M | `DirectedGraph<NodePayload, kMaxNodes, EdgePayload, kMaxEdges, Policy>` — fixed-capacity directed graph with BFS, DFS, topo sort, cycle detection, and compile-time policies (None, ReverseEdgeCache, AcyclicEnforced) | [directed-graph.md](../../features/dia/diacore/directed-graph.md) |

## Dependencies

| Dependency | What DiaCore uses from it |
|------------|--------------------------|
| External: jsoncpp | JSON parsing (wrapped, not re-exported in public API) |
| External: GoogleTest | Unit testing only |

No dependencies on any other Dia module.

## Decisions

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| SD-CORE-001 | All containers are fixed-capacity (no heap allocation in public container API) | Predictable memory layout; zero fragmentation; compatible with object pools and stack allocation | All DiaCore containers | Accepted | Yes |
| SD-CORE-002 | Header-only containers via `.h` + `.inl` pattern | Template definitions must be visible at the call site; keeps implementation co-located with declaration | All DiaCore containers | Accepted | Yes |
| SD-CORE-003 | `StringCRC` is the canonical ID type throughout the engine | Compile-time hashing gives zero-cost comparisons; debug builds store the original string; prevents typos | All DiaCore identifiers | Accepted | Yes |

**Status values:** `Proposed` · `Accepted` · `Rejected` · `Superseded`
**Binding:** `Yes` = enforced constraint on all child features · `No` = guidance only

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Scope | Should DiaMaths be a child of DiaCore or a sibling? | Sibling — DiaMaths depends on DiaCore but is its own compiled module and spec. DiaCore has no math dependency. |
| 2 | Features | Are there other DiaCore subsystems (Type, Memory, FilePath) that need feature specs? | Yes — those can be specced when changes are needed. This spec is intentionally stub-level until features are planned. |

## Status

`Active` — stub spec; feature specs added as work is planned.
