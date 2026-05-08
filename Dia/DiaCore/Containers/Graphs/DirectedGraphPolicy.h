#pragma once

namespace Dia
{
	namespace Core
	{
		namespace Containers
		{
			namespace GraphPolicy
			{
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
				// and cached. Subsequent GetInEdges() calls use the cache directly.
				//
				// Cache is invalidated (marked dirty) by AddNode() or AddEdge(). The next
				// GetInEdges() call rebuilds it from all current edges.
				//
				// Use when:
				//   - GetInEdges() is called frequently (e.g. "what uses X?" queries)
				//   - The graph is built once (or rarely mutated) and queried many times
				//   - Edge count is large enough that a linear scan is measurable
				//
				// Trade-off:
				//   - Adds kMaxEdges * 2 storage per entry in the cache
				//   - One full rebuild scan on the first GetInEdges() after any mutation
				// -----------------------------------------------------------------------
				struct ReverseEdgeCache {};

				// -----------------------------------------------------------------------
				// GraphPolicy::AcyclicEnforced
				//
				// Enforces the DAG (Directed Acyclic Graph) invariant at mutation time.
				// AddEdge() runs a reachability check before inserting; if the new edge
				// would create a cycle it asserts and returns false — the edge is NOT added.
				//
				// Because cycles are impossible by construction:
				//   - HasCycle() always returns false (O(1))
				//   - TopoSort() never returns false
				//
				// Use when:
				//   - Cycles are always a programming error (module dependencies, task graphs)
				//   - You want the container to catch the mistake at the insertion site
				//     rather than discovering it later during a topo sort
				//
				// Trade-off:
				//   - AddEdge() is O(V + E) in the worst case (runs DFS to detect cycle)
				//   - Not suitable for graphs assembled in hot loops
				// -----------------------------------------------------------------------
				struct AcyclicEnforced {};
			}
		}
	}
}
