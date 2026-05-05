# Feature Spec: DirectedGraph Container

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaCore | @docs/specs/systems/dia/diacore.md |
| Feature | DirectedGraph Container | (this document) |

## Summary

A fixed-capacity, stack-allocated directed graph container with BFS, DFS, topological sort, and
cycle detection. Extends the existing undirected `Graph<>` family in `DiaCore/Containers/Graphs/`
with proper directed semantics and compile-time **policies** that snap in alternative behaviours
(lazy reverse-edge caching, or acyclicity enforcement) without paying for what you don't need.

A **policy** is a compile-time template parameter — a type tag that specialises the graph's
internal behaviour. The default policy (`GraphPolicy::None`) is a pure container. Other policies
add targeted capabilities. Only one policy is active at a time.

## Problem

The existing `Graph<>` container has no direction or traversal semantics, and no concept of
in-edges vs out-edges. Systems that need to reason about dependency order (module validation,
`RelationshipIndex` in DiaAssetCatalogue, future AI/behaviour trees) must implement their own
ad-hoc directed graph logic, duplicating topology storage and re-inventing BFS/DFS each time.
A shared `DirectedGraph<>` in DiaCore eliminates this duplication and gives those systems a
tested, policy-configurable foundation to build on.

## Acceptance Criteria

1. `DirectedGraph<NodePayload, kMaxNodes, EdgePayload, kMaxEdges, Policy = GraphPolicy::None>` compiles as a fixed-capacity, zero-heap-allocation container under C++20.
2. `AddNode(id, payload)` — registers a node by `StringCRC` ID. Asserts and returns false if the ID is already present or capacity is full.
3. `AddEdge(id, fromNodeId, toNodeId, payload)` — adds a directed edge from `from → to`. Asserts and returns false if the edge ID already exists, either node is missing, or capacity is full. With `AcyclicEnforced` policy, also asserts and returns false if the edge would create a cycle.
4. `GetOutEdges(nodeId, results)` — fills a caller-provided `DynamicArrayC` with pointers to all edges whose tail is `nodeId`.
5. `GetInEdges(nodeId, results)` — fills a caller-provided `DynamicArrayC` with pointers to all edges whose head is `nodeId`. With `None` policy this scans all edges. With `ReverseEdgeCache` policy this queries the lazy cache.
6. `FindNode(id)` / `FindEdge(id)` — returns pointer or null; const and non-const overloads. Matches existing `Graph<>` API convention.
7. `GetNumberOfNodes()` / `GetNumberOfEdges()` — match existing `Graph<>` API convention.
8. `BFS(startNodeId, visitor)` — breadth-first traversal from a start node. Calls `visitor(node)` for each node reached in BFS order. Visits each node at most once.
9. `DFS(startNodeId, visitor)` — depth-first traversal from a start node. Calls `visitor(node)` for each node reached in DFS order. Visits each node at most once.
10. `TopoSort(results)` — fills a caller-provided `DynamicArrayC` with nodes in topological order. Returns `false` if a cycle is detected; the results array is left in an undefined partial state on cycle. With `AcyclicEnforced` policy, `TopoSort` never returns false (cycles cannot exist).
11. `HasCycle()` — returns `true` if the graph contains at least one cycle. With `AcyclicEnforced` policy, always returns `false` (O(1)).
12. BFS and DFS use a caller-provided working buffer (a `DynamicArrayC` passed in) to avoid heap allocation. The buffer requirement is documented per method.
13. All traversal and sort algorithms work correctly on disconnected graphs (graphs with multiple components).
14. All code lives in `Dia::Core::Containers::` namespace. Header-only via `.h` + `.inl` pattern, consistent with existing `Graph<>` files.
15. Policy comments: every `GraphPolicy` tag struct and every policy-conditioned code path carries an explanatory comment describing what the policy does, when to use it, and what trade-offs it makes. Reviewers unfamiliar with policy-based design should be able to understand the system from comments alone.

## Policies

Policies are **compile-time type tags** — empty structs — passed as the fifth template parameter.
They do not add virtual dispatch or runtime overhead beyond what the policy itself introduces
(e.g. `ReverseEdgeCache` adds a cache array; `None` adds nothing).

```cpp
// -----------------------------------------------------------------------
// GraphPolicy::None  (default)
//
// Pure topology container. No additional data members, no extra logic.
// GetInEdges() performs a linear scan over all edges to find incoming ones.
//
// Use when:
//   - You only query in-edges rarely, or the edge count is small
//   - You want the smallest possible memory footprint
//   - You are building a temporary graph (e.g. during a single algorithm pass)
// -----------------------------------------------------------------------
struct None {};

// -----------------------------------------------------------------------
// GraphPolicy::ReverseEdgeCache
//
// Adds a lazily-built reverse-edge index. On the first call to GetInEdges()
// after any mutation, the full reverse index is built from all current edges
// and cached. Subsequent GetInEdges() calls are O(1) hash lookups.
//
// Cache is invalidated (marked dirty) by AddNode(), AddEdge(), or any future
// Remove() operation. The next GetInEdges() call rebuilds it.
//
// Use when:
//   - GetInEdges() is called frequently (e.g. RelationshipIndex "what uses X?")
//   - The graph is built once (or rarely mutated) and queried many times
//   - Edge count is large enough that linear scan is measurable
//
// Trade-off:
//   - Adds kMaxNodes * kMaxEdges worst-case cache storage
//   - One full rebuild scan on first post-mutation GetInEdges()
// -----------------------------------------------------------------------
struct ReverseEdgeCache {};

// -----------------------------------------------------------------------
// GraphPolicy::AcyclicEnforced
//
// Enforces the DAG (Directed Acyclic Graph) invariant at mutation time.
// AddEdge() runs a cycle check before inserting; if the new edge would
// create a cycle it asserts and returns false — the edge is NOT added.
//
// Because cycles are impossible by construction:
//   - HasCycle() always returns false (O(1))
//   - TopoSort() never returns false
//
// Use when:
//   - Cycles are always a programming error (module dependency graphs,
//     build dependency graphs, task dependency graphs)
//   - You want the container to catch the mistake at the insertion site
//     rather than discovering it later during a topo sort
//
// Trade-off:
//   - AddEdge() is O(V + E) in the worst case (runs DFS to detect cycle)
//   - Cycle check on every edge insertion — not suitable for graphs that
//     are assembled in hot loops
// -----------------------------------------------------------------------
struct AcyclicEnforced {};
```

## API Design

```cpp
namespace Dia::Core::Containers
{
    // -------------------------------------------------------------------
    // Policy namespace — documented in full in DirectedGraphPolicy.h
    // -------------------------------------------------------------------
    namespace GraphPolicy
    {
        struct None {};
        struct ReverseEdgeCache {};
        struct AcyclicEnforced {};
    }

    // -------------------------------------------------------------------
    // DirectedGraph
    //
    // Fixed-capacity directed graph. kMaxNodes and kMaxEdges are compile-
    // time capacities — no heap allocation ever occurs.
    //
    // NodePayload and EdgePayload are the user data types stored per node
    // and per edge. They must be default-constructible.
    //
    // Policy controls optional behaviour — see GraphPolicy above.
    // Default is GraphPolicy::None (pure container, no extras).
    // -------------------------------------------------------------------
    template <
        class    NodePayload,
        unsigned int kMaxNodes,
        class    EdgePayload,
        unsigned int kMaxEdges,
        class    Policy = GraphPolicy::None
    >
    class DirectedGraph
    {
    public:
        using Node = DirectedGraphNode<NodePayload, EdgePayload>;
        using Edge = DirectedGraphEdge<EdgePayload, NodePayload>;

        using NodeResults = DynamicArrayC<Node*, kMaxNodes>;
        using EdgeResults = DynamicArrayC<Edge*, kMaxEdges>;

        DirectedGraph();

        // Mutation ---------------------------------------------------

        // Add a node with the given StringCRC ID and payload.
        // Returns false (and asserts) if the ID already exists or capacity is full.
        bool AddNode(const Dia::Core::StringCRC& id, const NodePayload& payload);

        // Add a directed edge from → to with the given ID and payload.
        // Returns false (and asserts) if:
        //   - the edge ID already exists
        //   - fromNodeId or toNodeId are not found
        //   - edge capacity is full
        //   - (AcyclicEnforced only) the edge would create a cycle
        // Invalidates the ReverseEdgeCache if that policy is active.
        bool AddEdge(const Dia::Core::StringCRC& id,
                     const Dia::Core::StringCRC& fromNodeId,
                     const Dia::Core::StringCRC& toNodeId,
                     const EdgePayload& payload);

        // Query -------------------------------------------------------

        Node*       FindNode(const Dia::Core::StringCRC& id);
        const Node* FindNode(const Dia::Core::StringCRC& id) const;

        Edge*       FindEdge(const Dia::Core::StringCRC& id);
        const Edge* FindEdge(const Dia::Core::StringCRC& id) const;

        unsigned int GetNumberOfNodes() const;
        unsigned int GetNumberOfEdges() const;

        // Fill results with all edges leaving this node (out-edges).
        void GetOutEdges(const Dia::Core::StringCRC& nodeId, EdgeResults& results) const;

        // Fill results with all edges entering this node (in-edges).
        // With GraphPolicy::None        — linear scan over all edges.
        // With GraphPolicy::ReverseEdgeCache — O(1) cache lookup (rebuilds lazily if dirty).
        void GetInEdges(const Dia::Core::StringCRC& nodeId, EdgeResults& results) const;

        // Traversal ---------------------------------------------------

        // BFS from startNodeId. Calls visitor(node) for each reached node in
        // breadth-first order. Each node is visited at most once.
        // visitedBuffer: caller-provided scratch space (capacity >= kMaxNodes recommended).
        template <class Visitor>
        void BFS(const Dia::Core::StringCRC& startNodeId,
                 const Visitor& visitor,
                 DynamicArrayC<const Node*, kMaxNodes>& visitedBuffer) const;

        // DFS from startNodeId. Calls visitor(node) for each reached node in
        // depth-first order. Each node is visited at most once.
        // visitedBuffer: caller-provided scratch space (capacity >= kMaxNodes recommended).
        template <class Visitor>
        void DFS(const Dia::Core::StringCRC& startNodeId,
                 const Visitor& visitor,
                 DynamicArrayC<const Node*, kMaxNodes>& visitedBuffer) const;

        // Topological sort. Fills results in topological order (sources first).
        // Returns false if a cycle is detected; results is in an undefined partial
        // state on false return.
        // With GraphPolicy::AcyclicEnforced — always returns true.
        // workBuffer: caller-provided scratch space (capacity >= kMaxNodes recommended).
        bool TopoSort(NodeResults& results,
                      DynamicArrayC<const Node*, kMaxNodes>& workBuffer) const;

        // Returns true if the graph contains at least one cycle.
        // With GraphPolicy::AcyclicEnforced — always returns false (O(1)).
        bool HasCycle() const;
    };
}
```

## Tasks

| # | Task | Description |
|---|------|-------------|
| 1 | Implement `DirectedGraphPolicy.h` | Policy tag structs (`None`, `ReverseEdgeCache`, `AcyclicEnforced`) with full explanatory comments for each |
| 2 | Implement `DirectedGraphNode.h` + `.inl` | Node type storing `StringCRC` ID, `NodePayload`, and a `DynamicArrayC` of out-edge pointers. Mirrors `GraphNode.h` pattern. |
| 3 | Implement `DirectedGraphEdge.h` + `.inl` | Edge type storing `StringCRC` ID, `EdgePayload`, and typed pointers to tail (from) and head (to) nodes. Mirrors `GraphEdge.h` pattern. |
| 4 | Implement `DirectedGraph.h` + `.inl` — core container | `AddNode`, `AddEdge`, `FindNode`, `FindEdge`, `GetNumberOfNodes`, `GetNumberOfEdges`, `GetOutEdges`, `GetInEdges` (Policy::None scan path) |
| 5 | Implement traversal algorithms | `BFS`, `DFS`, `TopoSort` (Kahn's algorithm), `HasCycle` — all in `DirectedGraph.inl` |
| 6 | Implement `ReverseEdgeCache` policy specialisation | Lazy reverse index storage + `GetInEdges` override; cache invalidation in `AddNode` / `AddEdge` |
| 7 | Implement `AcyclicEnforced` policy specialisation | Cycle-check path in `AddEdge`; `HasCycle()` returns false; `TopoSort` never returns false |
| 8 | Update `DiaCore.vcxproj` + `.vcxproj.filters` | Add all new files to the Graphs filter |
| 9 | Update `dia.core.containers.graphs.architecture.module.md` | Add `DirectedGraph`, `DirectedGraphNode`, `DirectedGraphEdge`, `DirectedGraphPolicy` to public API entries |
| 10 | GoogleTest coverage | See test plan below |
| 11 | Phase 2 — `RelationshipIndex` wraps `DirectedGraph` | Refactor `RelationshipIndex` to use `DirectedGraph<AssetRecord*, kMaxAssets, RelationshipEdge, kMaxRelationships, GraphPolicy::ReverseEdgeCache>` as its backing store. This proves the container works under real-world load and removes duplicated topology logic from DiaAssetCatalogue. |

## Test Plan (Task 10)

| Suite | Tests |
|-------|-------|
| `DirectedGraphNodeTest` | Construct, get ID, get payload, add out-edge pointer |
| `DirectedGraphEdgeTest` | Construct, get ID, get tail/head, get payload |
| `DirectedGraphCoreTest` | AddNode (unique, duplicate assert), AddEdge (valid, missing nodes assert, duplicate ID assert), FindNode/FindEdge (hit/miss), GetNumberOfNodes/Edges, capacity limits |
| `DirectedGraphOutEdgesTest` | GetOutEdges: no edges, one edge, multiple edges, wrong node ID |
| `DirectedGraphInEdgesTest` | GetInEdges (Policy::None): no in-edges, one in-edge, multiple in-edges, node not in graph |
| `DirectedGraphBFSTest` | Single node, linear chain, branching graph, disconnected graph (unreachable nodes not visited), visitor call order |
| `DirectedGraphDFSTest` | Single node, linear chain, branching graph, disconnected graph, visitor call order |
| `DirectedGraphTopoSortTest` | Single node, linear chain, diamond DAG, cycle returns false and correct result, already-topological order not disturbed |
| `DirectedGraphHasCycleTest` | No cycle, self-loop, two-node cycle, longer cycle |
| `DirectedGraphPolicyReverseEdgeCacheTest` | GetInEdges before and after AddEdge (cache rebuild), multiple mutations then query (one rebuild), same results as Policy::None scan |
| `DirectedGraphPolicyAcyclicEnforcedTest` | AddEdge that would create cycle returns false (and asserts), HasCycle always false, TopoSort always returns true, valid DAG construction succeeds |

## Files

| File | Action |
|------|--------|
| `Dia/DiaCore/Containers/Graphs/DirectedGraphPolicy.h` | Create — policy tag structs with full comments |
| `Dia/DiaCore/Containers/Graphs/DirectedGraphNode.h` | Create — directed graph node header |
| `Dia/DiaCore/Containers/Graphs/DirectedGraphNode.inl` | Create — directed graph node implementation |
| `Dia/DiaCore/Containers/Graphs/DirectedGraphEdge.h` | Create — directed graph edge header |
| `Dia/DiaCore/Containers/Graphs/DirectedGraphEdge.inl` | Create — directed graph edge implementation |
| `Dia/DiaCore/Containers/Graphs/DirectedGraph.h` | Create — directed graph class template |
| `Dia/DiaCore/Containers/Graphs/DirectedGraph.inl` | Create — all method implementations |
| `Dia/DiaCore/DiaCore.vcxproj` | Modify — add all new Graph files |
| `Dia/DiaCore/DiaCore.vcxproj.filters` | Modify — add new files under Graphs filter |
| `Dia/DiaCore/Containers/Graphs/dia.core.containers.graphs.architecture.module.md` | Modify — extend public_api entries |
| `Cluiche/Tests/GoogleTests/` | Add — new test file(s) for all suites above |

## Dependencies

| Dependency | What this feature uses |
|------------|----------------------|
| DiaCore — `DynamicArrayC` | Node/edge storage, traversal buffers, query results |
| DiaCore — `StringCRC` | Node and edge IDs |
| DiaCore — `Assert` | `DIA_ASSERT` for precondition checking |

## Phase 2 — RelationshipIndex Integration

Phase 2 (Task 11) is explicitly deferred to a separate implementation session. It is tracked here
because it is the primary proof that `DirectedGraph` integrates correctly into a real system.

**Scope of Phase 2:**
- Refactor `Dia::AssetCatalogue::RelationshipIndex` to hold a
  `DirectedGraph<..., GraphPolicy::ReverseEdgeCache>` as its internal backing store.
- Remove the hand-rolled reverse cache and topology logic that is currently inside `RelationshipIndex`.
- All existing `RelationshipIndex` tests must continue to pass without modification.
- No change to `AssetRegistry` public API.

**Why it is Phase 2 and not Phase 1:**
`RelationshipIndex` is in a Done system (`DiaAssetCatalogue`). Changing it requires re-running that
system's 92 tests as the acceptance gate. It is cleaner to build and test `DirectedGraph` first (Phase 1)
and then migrate in a separate focused commit (Phase 2) where any regression is clearly attributable.

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for all entity/component IDs | **Compliant.** Node and edge IDs are `StringCRC`. All lookups are CRC-keyed. |
| PD-002 | ProcessingUnit/Phase/Module architecture | **Not applicable.** Pure data container, no lifecycle. |
| PD-003 | Component-based entities | **Not applicable.** Container layer below component system. |
| PD-004 | No STL containers in public APIs | **Compliant.** All public methods use `DynamicArrayC`. No STL in any signature. |
| PD-005 | x64 Windows only | **Compliant.** No platform-specific code. |
| PD-006 | Visual Studio project files are source of truth | **Compliant.** `DiaCore.vcxproj` updated manually; no generator used. |
| PD-007 | C++20 required | **Compliant.** Uses C++20 `if constexpr` for policy specialisation; no C++20 features required beyond what the platform already mandates. |
| PD-008 | `Directory.Build.props` owns build settings | **Compliant.** `DiaCore.vcxproj` inherits all settings from root props. |
| PD-009 | Generated output under `Cluiche/out/` | **Not applicable.** No output generation. |
| AD-001 | Module system with YAML frontmatter | **Compliant.** `dia.core.containers.graphs.architecture.module.md` updated to include `DirectedGraph` family. |
| AD-002 | No STL in public APIs | **Compliant.** Same as PD-004. |
| AD-003 | Namespace `Dia::<Module>::` | **Compliant.** All code under `Dia::Core::Containers::`. |
| AD-005 | Component-based entities | **Not applicable.** Container layer. |
| SD-CORE-001 | Fixed-capacity containers | **Compliant.** `kMaxNodes` and `kMaxEdges` are compile-time capacities; no heap allocation. |
| SD-CORE-002 | Header-only via `.h` + `.inl` | **Compliant.** All template implementations in `.inl` files included from `.h`. |
| SD-CORE-003 | `StringCRC` as canonical ID type | **Compliant.** Same as PD-001. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Policies | Should policies be composable (multiple active at once)? | No. Single policy per graph instance for v1. Composable policies require variadic template packs or CRTP mixins — useful long-term but not justified by current use cases. Can be widened later. |
| 2 | Traversal | BFS and DFS use a caller-provided visited buffer to avoid heap allocation. Is the burden on the caller acceptable? | Yes. The engine pattern (DynamicArrayC) already places sizing responsibility on the caller. Callers can size the buffer to kMaxNodes for a safe upper bound. Documented per method. |
| 3 | TopoSort | Kahn's algorithm vs DFS-based topo sort — which to use? | Kahn's algorithm. It naturally produces cycle detection (queue empties before all nodes are processed = cycle exists), is iterative (no recursion stack risk on large graphs), and the result order is intuitive (sources first). |
| 4 | Naming | `DirectedGraph` vs `DiGraph` vs `DAG` — does the name hold up over time? | `DirectedGraph` holds up. `DiGraph` is a mathematical abbreviation that is less readable. `DAG` is wrong (cycles are allowed; `AcyclicEnforced` enforces DAG only when selected). `DirectedGraph` is unambiguous and searchable. |
| 5 | Existing Graph | Should the undirected `Graph<>` be deprecated in favour of `DirectedGraph` with symmetric edges? | No. `Graph<>` is used in existing code. It is a separate concept. Undirected and directed graphs have different semantics. Both should coexist. |
| 6 | Node/Edge pointers | `GetOutEdges` and `GetInEdges` return `Edge*` pointers into internal arrays. Are these stable? | Yes — `DynamicArrayC` is a fixed-size array; pointers into it are stable as long as no Remove() is called. `DirectedGraph` v1 has no Remove() method; if Remove() is added later this must be revisited. |
| 7 | Phase 2 | Is the RelationshipIndex refactor (Phase 2) a separate spec or tracked here? | Tracked here as Task 11 and Phase 2 section. It is implemented as a separate commit, but does not need its own feature spec because the contract (all existing RelationshipIndex tests pass) is already covered by the DiaAssetCatalogue specs. |
| 8 | Policy comments | How much comment coverage is "enough" for the policy system? | Every policy tag struct gets a block comment explaining: what it does, when to use it, and the trade-off. Every policy-conditioned code branch (e.g. `if constexpr (std::is_same_v<Policy, GraphPolicy::ReverseEdgeCache>)`) gets a one-line comment naming the policy and the reason. A reader unfamiliar with policy-based design should be able to understand the system from comments alone — this is a hard acceptance criterion (Task 1 and AC-15). |

## Status

`Approved`
