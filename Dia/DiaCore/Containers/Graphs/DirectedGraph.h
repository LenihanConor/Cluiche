#pragma once

#include "DiaCore/Containers/Arrays/DynamicArrayC.h"
#include "DiaCore/Containers/Graphs/DirectedGraphPolicy.h"
#include "DiaCore/Containers/Graphs/DirectedGraphNode.h"
#include "DiaCore/Containers/Graphs/DirectedGraphEdge.h"

namespace Dia
{
	namespace Core
	{
		namespace Containers
		{
			// -----------------------------------------------------------------------
			// DirectedGraph
			//
			// Fixed-capacity directed graph. kMaxNodes and kMaxEdges are compile-time
			// capacities — no heap allocation ever occurs.
			//
			// NodePayload / EdgePayload are user data stored per node/edge.
			// Both must be default-constructible.
			//
			// Policy controls optional behaviour — see DirectedGraphPolicy.h.
			// Default is GraphPolicy::None (pure container, no extras).
			//
			// kMaxOutEdgesPerNode caps the out-edge (and in-edge cache) list per node.
			// Set it to the real per-node fan-out to avoid over-allocating when kMaxEdges
			// is large. Defaults to kMaxEdges (star topology worst case).
			//
			// IDType is the type used for node and edge identifiers.
			// Default is StringCRC (68 bytes — stores a 64-char debug string + 4-byte CRC).
			// Use Dia::Core::CRC for compact 4-byte IDs when debug strings are not
			// needed (e.g. large internal caches). IDType must support operator==,
			// Value(), and default construction.
			// -----------------------------------------------------------------------
			template <
				class        NodePayload,
				unsigned int kMaxNodes,
				class        EdgePayload,
				unsigned int kMaxEdges,
				class        Policy              = GraphPolicy::None,
				unsigned int kMaxOutEdgesPerNode = kMaxEdges,
				class        IDType              = Dia::Core::StringCRC
			>
			class DirectedGraph
			{
			public:
				typedef DirectedGraphNode<NodePayload, EdgePayload, kMaxOutEdgesPerNode, IDType> Node;
				typedef DirectedGraphEdge<EdgePayload, NodePayload, kMaxOutEdgesPerNode, IDType> Edge;

				typedef Dia::Core::Containers::DynamicArrayC<Node*, kMaxNodes> NodeResults;
				typedef Dia::Core::Containers::DynamicArrayC<Edge*, kMaxEdges> EdgeResults;

				DirectedGraph();

				// ------------------------------------------------------------------
				// Mutation
				// ------------------------------------------------------------------

				// Reset the graph to an empty state (remove all nodes and edges).
				void Clear();

				// Add a node. Returns false (and asserts) if ID already exists or
				// capacity is full.
				bool AddNode(const IDType& id, const NodePayload& payload);

				// Add a directed edge from → to. Returns false (and asserts) if:
				//   - the edge ID already exists
				//   - fromNodeId or toNodeId are not found
				//   - edge capacity is full
				//   - (AcyclicEnforced only) the edge would create a cycle
				// Invalidates the ReverseEdgeCache if that policy is active.
				bool AddEdge(const IDType& id,
				             const IDType& fromNodeId,
				             const IDType& toNodeId,
				             const EdgePayload& payload);

				// ------------------------------------------------------------------
				// Query
				// ------------------------------------------------------------------

				Node*       FindNode(const IDType& id);
				const Node* FindNode(const IDType& id) const;

				Edge*       FindEdge(const IDType& id);
				const Edge* FindEdge(const IDType& id) const;

				unsigned int GetNumberOfNodes() const;
				unsigned int GetNumberOfEdges() const;

				// Fill results with all edges leaving nodeId (out-edges).
				void GetOutEdges(const IDType& nodeId, EdgeResults& results) const;

				// Fill results with all edges entering nodeId (in-edges).
				// GraphPolicy::None            — linear scan over all edges.
				// GraphPolicy::ReverseEdgeCache — uses lazy cache (rebuilds if dirty).
				void GetInEdges(const IDType& nodeId, EdgeResults& results) const;

				// ------------------------------------------------------------------
				// Traversal
				// ------------------------------------------------------------------

				// BFS from startNodeId. Visits each reachable node once in BFS order.
				// visitedBuffer: caller-provided scratch, capacity >= kMaxNodes recommended.
				template <class Visitor>
				void BFS(const IDType& startNodeId,
				         const Visitor& visitor,
				         DynamicArrayC<const Node*, kMaxNodes>& visitedBuffer) const;

				// DFS from startNodeId. Visits each reachable node once in DFS order.
				// visitedBuffer: caller-provided scratch, capacity >= kMaxNodes recommended.
				template <class Visitor>
				void DFS(const IDType& startNodeId,
				         const Visitor& visitor,
				         DynamicArrayC<const Node*, kMaxNodes>& visitedBuffer) const;

				// Topological sort (Kahn's algorithm, sources first).
				// Returns false if a cycle is detected; results is in an undefined partial
				// state on a false return.
				// GraphPolicy::AcyclicEnforced — always returns true.
				// workBuffer: caller-provided scratch, capacity >= kMaxNodes recommended.
				bool TopoSort(NodeResults& results,
				              DynamicArrayC<const Node*, kMaxNodes>& workBuffer) const;

				// Returns true if the graph contains at least one cycle.
				// GraphPolicy::AcyclicEnforced — always returns false (O(1)).
				bool HasCycle() const;

			private:
				// ------------------------------------------------------------------
				// Internal helpers
				// ------------------------------------------------------------------

				int FindNodeIndex(const IDType& id) const;
				int FindEdgeIndex(const IDType& id) const;

				// DFS reachability: can we reach 'target' starting from 'start'?
				// Used by AcyclicEnforced to test whether an edge would create a cycle.
				bool CanReachNode(const Node* start, const IDType& targetId) const;

				// ------------------------------------------------------------------
				// Policy-specific helpers (compiled in only when Policy matches)
				// ------------------------------------------------------------------

				// ReverseEdgeCache: rebuild the per-node in-edge lists from mEdgeList.
				void RebuildReverseCache() const;

				// ReverseEdgeCache: mark the cache dirty (called after any mutation).
				void InvalidateReverseCache();

				// ------------------------------------------------------------------
				// Storage
				// ------------------------------------------------------------------

				typedef Dia::Core::Containers::DynamicArrayC<Node, kMaxNodes> NodeList;
				typedef Dia::Core::Containers::DynamicArrayC<Edge, kMaxEdges> EdgeList;

				NodeList mNodeList;
				EdgeList mEdgeList;

				// ReverseEdgeCache policy: per-node lists of in-edge pointers + dirty flag.
				// kMaxOutEdgesPerNode bounds both out- and in-edge capacity per node.
				typedef Dia::Core::Containers::DynamicArrayC<Edge*, kMaxOutEdgesPerNode> InEdgeCacheEntry;
				typedef Dia::Core::Containers::DynamicArrayC<InEdgeCacheEntry, kMaxNodes> InEdgeCache;

				mutable InEdgeCache mInEdgeCache;
				mutable bool        mCacheDirty;
			};
		}
	}
}

#include "DiaCore/Containers/Graphs/DirectedGraph.inl"
