
#include <type_traits>
#include "DiaCore/Core/Assert.h"

namespace Dia
{
	namespace Core
	{
		namespace Containers
		{
			// ======================================================================
			// Constructor
			// ======================================================================

			template <class NodePayload, unsigned int kMaxNodes, class EdgePayload, unsigned int kMaxEdges, class Policy, unsigned int kMaxOutEdgesPerNode>
			DirectedGraph<NodePayload, kMaxNodes, EdgePayload, kMaxEdges, Policy, kMaxOutEdgesPerNode>::DirectedGraph()
				: mCacheDirty(true)
			{}

			// ======================================================================
			// Mutation — Clear
			// ======================================================================

			template <class NodePayload, unsigned int kMaxNodes, class EdgePayload, unsigned int kMaxEdges, class Policy, unsigned int kMaxOutEdgesPerNode>
			void DirectedGraph<NodePayload, kMaxNodes, EdgePayload, kMaxEdges, Policy, kMaxOutEdgesPerNode>::Clear()
			{
				mNodeList.RemoveAll();
				mEdgeList.RemoveAll();

				if constexpr (std::is_same_v<Policy, GraphPolicy::ReverseEdgeCache>)
				{
					mInEdgeCache.RemoveAll();
					mCacheDirty = true;
				}
			}

			// ======================================================================
			// Mutation — AddNode
			// ======================================================================

			template <class NodePayload, unsigned int kMaxNodes, class EdgePayload, unsigned int kMaxEdges, class Policy, unsigned int kMaxOutEdgesPerNode>
			bool DirectedGraph<NodePayload, kMaxNodes, EdgePayload, kMaxEdges, Policy, kMaxOutEdgesPerNode>::AddNode(
				const Dia::Core::StringCRC& id, const NodePayload& payload)
			{
				DIA_ASSERT(FindNodeIndex(id) == -1, "AddNode: node ID '%s' already exists", id.AsChar());
				if (FindNodeIndex(id) != -1)
					return false;

				DIA_ASSERT(!mNodeList.IsFull(), "AddNode: node capacity (%u) is full", kMaxNodes);
				if (mNodeList.IsFull())
					return false;

				mNodeList.Add(Node(id, payload));

				// ReverseEdgeCache: add a parallel empty cache entry and mark dirty
				if constexpr (std::is_same_v<Policy, GraphPolicy::ReverseEdgeCache>)
				{
					mInEdgeCache.Add(InEdgeCacheEntry());
					mCacheDirty = true;
				}

				return true;
			}

			// ======================================================================
			// Mutation — AddEdge
			// ======================================================================

			template <class NodePayload, unsigned int kMaxNodes, class EdgePayload, unsigned int kMaxEdges, class Policy, unsigned int kMaxOutEdgesPerNode>
			bool DirectedGraph<NodePayload, kMaxNodes, EdgePayload, kMaxEdges, Policy, kMaxOutEdgesPerNode>::AddEdge(
				const Dia::Core::StringCRC& id,
				const Dia::Core::StringCRC& fromNodeId,
				const Dia::Core::StringCRC& toNodeId,
				const EdgePayload& payload)
			{
				DIA_ASSERT(FindEdgeIndex(id) == -1, "AddEdge: edge ID '%s' already exists", id.AsChar());
				if (FindEdgeIndex(id) != -1)
					return false;

				int fromIdx = FindNodeIndex(fromNodeId);
				DIA_ASSERT(fromIdx != -1, "AddEdge: from node '%s' not found", fromNodeId.AsChar());
				if (fromIdx == -1)
					return false;

				int toIdx = FindNodeIndex(toNodeId);
				DIA_ASSERT(toIdx != -1, "AddEdge: to node '%s' not found", toNodeId.AsChar());
				if (toIdx == -1)
					return false;

				DIA_ASSERT(!mEdgeList.IsFull(), "AddEdge: edge capacity (%u) is full", kMaxEdges);
				if (mEdgeList.IsFull())
					return false;

				Node* fromNode = &mNodeList[fromIdx];
				Node* toNode   = &mNodeList[toIdx];

				// AcyclicEnforced: reject the edge if it would create a cycle.
				// A cycle exists when 'toNode' can already reach 'fromNode'.
				if constexpr (std::is_same_v<Policy, GraphPolicy::AcyclicEnforced>)
				{
					if (CanReachNode(toNode, fromNodeId))
					{
						DIA_ASSERT(false, "AddEdge: edge '%s' would create a cycle (AcyclicEnforced policy)", id.AsChar());
						return false;
					}
				}

				// Store the edge value, then register the stored pointer as an out-edge.
				// The pointer is stable: DynamicArrayC never moves existing elements.
				mEdgeList.Add(Edge(id, payload, fromNode, toNode));
				fromNode->AddOutEdge(&mEdgeList.Back());

				// ReverseEdgeCache: any mutation invalidates the reverse index
				InvalidateReverseCache();

				return true;
			}

			// ======================================================================
			// Query — FindNode / FindEdge
			// ======================================================================

			template <class NodePayload, unsigned int kMaxNodes, class EdgePayload, unsigned int kMaxEdges, class Policy, unsigned int kMaxOutEdgesPerNode>
			typename DirectedGraph<NodePayload, kMaxNodes, EdgePayload, kMaxEdges, Policy, kMaxOutEdgesPerNode>::Node*
			DirectedGraph<NodePayload, kMaxNodes, EdgePayload, kMaxEdges, Policy, kMaxOutEdgesPerNode>::FindNode(const Dia::Core::StringCRC& id)
			{
				int index = FindNodeIndex(id);
				return (index == -1) ? nullptr : &mNodeList[index];
			}

			template <class NodePayload, unsigned int kMaxNodes, class EdgePayload, unsigned int kMaxEdges, class Policy, unsigned int kMaxOutEdgesPerNode>
			const typename DirectedGraph<NodePayload, kMaxNodes, EdgePayload, kMaxEdges, Policy, kMaxOutEdgesPerNode>::Node*
			DirectedGraph<NodePayload, kMaxNodes, EdgePayload, kMaxEdges, Policy, kMaxOutEdgesPerNode>::FindNode(const Dia::Core::StringCRC& id) const
			{
				int index = FindNodeIndex(id);
				return (index == -1) ? nullptr : &mNodeList[index];
			}

			template <class NodePayload, unsigned int kMaxNodes, class EdgePayload, unsigned int kMaxEdges, class Policy, unsigned int kMaxOutEdgesPerNode>
			typename DirectedGraph<NodePayload, kMaxNodes, EdgePayload, kMaxEdges, Policy, kMaxOutEdgesPerNode>::Edge*
			DirectedGraph<NodePayload, kMaxNodes, EdgePayload, kMaxEdges, Policy, kMaxOutEdgesPerNode>::FindEdge(const Dia::Core::StringCRC& id)
			{
				int index = FindEdgeIndex(id);
				return (index == -1) ? nullptr : &mEdgeList[index];
			}

			template <class NodePayload, unsigned int kMaxNodes, class EdgePayload, unsigned int kMaxEdges, class Policy, unsigned int kMaxOutEdgesPerNode>
			const typename DirectedGraph<NodePayload, kMaxNodes, EdgePayload, kMaxEdges, Policy, kMaxOutEdgesPerNode>::Edge*
			DirectedGraph<NodePayload, kMaxNodes, EdgePayload, kMaxEdges, Policy, kMaxOutEdgesPerNode>::FindEdge(const Dia::Core::StringCRC& id) const
			{
				int index = FindEdgeIndex(id);
				return (index == -1) ? nullptr : &mEdgeList[index];
			}

			// ======================================================================
			// Query — counts
			// ======================================================================

			template <class NodePayload, unsigned int kMaxNodes, class EdgePayload, unsigned int kMaxEdges, class Policy, unsigned int kMaxOutEdgesPerNode>
			unsigned int DirectedGraph<NodePayload, kMaxNodes, EdgePayload, kMaxEdges, Policy, kMaxOutEdgesPerNode>::GetNumberOfNodes() const
			{
				return mNodeList.Size();
			}

			template <class NodePayload, unsigned int kMaxNodes, class EdgePayload, unsigned int kMaxEdges, class Policy, unsigned int kMaxOutEdgesPerNode>
			unsigned int DirectedGraph<NodePayload, kMaxNodes, EdgePayload, kMaxEdges, Policy, kMaxOutEdgesPerNode>::GetNumberOfEdges() const
			{
				return mEdgeList.Size();
			}

			// ======================================================================
			// Query — GetOutEdges
			// ======================================================================

			template <class NodePayload, unsigned int kMaxNodes, class EdgePayload, unsigned int kMaxEdges, class Policy, unsigned int kMaxOutEdgesPerNode>
			void DirectedGraph<NodePayload, kMaxNodes, EdgePayload, kMaxEdges, Policy, kMaxOutEdgesPerNode>::GetOutEdges(
				const Dia::Core::StringCRC& nodeId, EdgeResults& results) const
			{
				results.RemoveAll();
				const Node* node = FindNode(nodeId);
				if (!node)
					return;

				const typename Node::OutEdgeList& outEdges = node->GetOutEdgeList();
				for (unsigned int i = 0; i < outEdges.Size(); i++)
				{
					// outEdges stores Edge* values; accessing through const gives Edge* const&,
					// but the pointer value itself is Edge* (non-const) — safe copy.
					results.Add(outEdges.At(i));
				}
			}

			// ======================================================================
			// Query — GetInEdges (policy-dispatched)
			// ======================================================================

			template <class NodePayload, unsigned int kMaxNodes, class EdgePayload, unsigned int kMaxEdges, class Policy, unsigned int kMaxOutEdgesPerNode>
			void DirectedGraph<NodePayload, kMaxNodes, EdgePayload, kMaxEdges, Policy, kMaxOutEdgesPerNode>::GetInEdges(
				const Dia::Core::StringCRC& nodeId, EdgeResults& results) const
			{
				results.RemoveAll();

				if constexpr (std::is_same_v<Policy, GraphPolicy::ReverseEdgeCache>)
				{
					// ReverseEdgeCache: lazily rebuild the index if dirty, then do O(1) lookup
					if (mCacheDirty)
						RebuildReverseCache();

					int nodeIdx = FindNodeIndex(nodeId);
					if (nodeIdx < 0)
						return;

					const InEdgeCacheEntry& entry = mInEdgeCache.At(nodeIdx);
					for (unsigned int i = 0; i < entry.Size(); i++)
						results.Add(entry.At(i));
				}
				else
				{
					// GraphPolicy::None (and AcyclicEnforced): linear scan over all edges
					for (unsigned int i = 0; i < mEdgeList.Size(); i++)
					{
						const Edge& edge = mEdgeList.At(i);
						if (edge.GetTo()->GetUniqueID() == nodeId)
						{
							// mEdgeList stores Edge values; underlying data is non-const.
							results.Add(const_cast<Edge*>(&edge));
						}
					}
				}
			}

			// ======================================================================
			// Traversal — BFS
			// ======================================================================

			template <class NodePayload, unsigned int kMaxNodes, class EdgePayload, unsigned int kMaxEdges, class Policy, unsigned int kMaxOutEdgesPerNode>
			template <class Visitor>
			void DirectedGraph<NodePayload, kMaxNodes, EdgePayload, kMaxEdges, Policy, kMaxOutEdgesPerNode>::BFS(
				const Dia::Core::StringCRC& startNodeId,
				const Visitor& visitor,
				DynamicArrayC<const Node*, kMaxNodes>& visitedBuffer) const
			{
				visitedBuffer.RemoveAll();

				const Node* start = FindNode(startNodeId);
				if (!start)
					return;

				// visitedBuffer doubles as the BFS queue: process front→back, append new nodes to back.
				visitedBuffer.Add(start);

				for (unsigned int front = 0; front < visitedBuffer.Size(); front++)
				{
					const Node* current = visitedBuffer.At(front);
					visitor(*current);

					const typename Node::OutEdgeList& outEdges = current->GetOutEdgeList();
					for (unsigned int i = 0; i < outEdges.Size(); i++)
					{
						const Node* neighbor = outEdges.At(i)->GetTo();

						// Only enqueue if not already in visitedBuffer
						bool alreadyQueued = false;
						for (unsigned int j = 0; j < visitedBuffer.Size(); j++)
						{
							if (visitedBuffer.At(j) == neighbor)
							{
								alreadyQueued = true;
								break;
							}
						}
						if (!alreadyQueued && !visitedBuffer.IsFull())
							visitedBuffer.Add(neighbor);
					}
				}
			}

			// ======================================================================
			// Traversal — DFS
			// ======================================================================

			template <class NodePayload, unsigned int kMaxNodes, class EdgePayload, unsigned int kMaxEdges, class Policy, unsigned int kMaxOutEdgesPerNode>
			template <class Visitor>
			void DirectedGraph<NodePayload, kMaxNodes, EdgePayload, kMaxEdges, Policy, kMaxOutEdgesPerNode>::DFS(
				const Dia::Core::StringCRC& startNodeId,
				const Visitor& visitor,
				DynamicArrayC<const Node*, kMaxNodes>& visitedBuffer) const
			{
				visitedBuffer.RemoveAll();

				const Node* start = FindNode(startNodeId);
				if (!start)
					return;

				// Use a local stack-allocated array as the DFS frontier (no heap allocation).
				DynamicArrayC<const Node*, kMaxNodes> stack;
				stack.Add(start);

				while (stack.Size() > 0)
				{
					const Node* current = stack.Back();
					stack.Remove(); // removes last element (LIFO)

					// Skip if already visited
					bool alreadyVisited = false;
					for (unsigned int i = 0; i < visitedBuffer.Size(); i++)
					{
						if (visitedBuffer.At(i) == current)
						{
							alreadyVisited = true;
							break;
						}
					}
					if (alreadyVisited)
						continue;

					visitedBuffer.Add(current);
					visitor(*current);

					// Push neighbors — pushed in reverse order so first out-edge is visited first
					const typename Node::OutEdgeList& outEdges = current->GetOutEdgeList();
					for (int i = static_cast<int>(outEdges.Size()) - 1; i >= 0; i--)
						stack.Add(outEdges.At(i)->GetTo());
				}
			}

			// ======================================================================
			// Traversal — TopoSort (Kahn's algorithm)
			// ======================================================================

			template <class NodePayload, unsigned int kMaxNodes, class EdgePayload, unsigned int kMaxEdges, class Policy, unsigned int kMaxOutEdgesPerNode>
			bool DirectedGraph<NodePayload, kMaxNodes, EdgePayload, kMaxEdges, Policy, kMaxOutEdgesPerNode>::TopoSort(
				NodeResults& results,
				DynamicArrayC<const Node*, kMaxNodes>& workBuffer) const
			{
				results.RemoveAll();
				workBuffer.RemoveAll();

				// Compute in-degree for each node (indexed parallel to mNodeList)
				DynamicArrayC<unsigned int, kMaxNodes> inDegree;
				for (unsigned int i = 0; i < mNodeList.Size(); i++)
					inDegree.Add(0u);

				for (unsigned int i = 0; i < mEdgeList.Size(); i++)
				{
					int toIdx = FindNodeIndex(mEdgeList.At(i).GetTo()->GetUniqueID());
					if (toIdx >= 0)
						inDegree[static_cast<unsigned int>(toIdx)]++;
				}

				// Seed queue with all nodes that have no incoming edges
				for (unsigned int i = 0; i < mNodeList.Size(); i++)
				{
					if (inDegree.At(i) == 0)
						workBuffer.Add(&mNodeList.At(i));
				}

				// Kahn's main loop: process queue front-to-back
				for (unsigned int front = 0; front < workBuffer.Size(); front++)
				{
					const Node* current = workBuffer.At(front);

					// mNodeList stores Node values; the underlying data is non-const.
					results.Add(const_cast<Node*>(current));

					const typename Node::OutEdgeList& outEdges = current->GetOutEdgeList();
					for (unsigned int i = 0; i < outEdges.Size(); i++)
					{
						const Node* neighbor = outEdges.At(i)->GetTo();
						int neighborIdx = FindNodeIndex(neighbor->GetUniqueID());
						if (neighborIdx >= 0)
						{
							unsigned int ui = static_cast<unsigned int>(neighborIdx);
							inDegree[ui]--;
							if (inDegree.At(ui) == 0 && !workBuffer.IsFull())
								workBuffer.Add(neighbor);
						}
					}
				}

				// If not all nodes were added to results, a cycle exists
				return results.Size() == mNodeList.Size();
			}

			// ======================================================================
			// Traversal — HasCycle
			// ======================================================================

			template <class NodePayload, unsigned int kMaxNodes, class EdgePayload, unsigned int kMaxEdges, class Policy, unsigned int kMaxOutEdgesPerNode>
			bool DirectedGraph<NodePayload, kMaxNodes, EdgePayload, kMaxEdges, Policy, kMaxOutEdgesPerNode>::HasCycle() const
			{
				if constexpr (std::is_same_v<Policy, GraphPolicy::AcyclicEnforced>)
				{
					// AcyclicEnforced: cycles are impossible by construction — O(1) false
					return false;
				}
				else
				{
					// Run TopoSort: if it cannot process all nodes, a cycle is present
					NodeResults results;
					DynamicArrayC<const Node*, kMaxNodes> workBuffer;
					return !TopoSort(results, workBuffer);
				}
			}

			// ======================================================================
			// Private — FindNodeIndex / FindEdgeIndex
			// ======================================================================

			template <class NodePayload, unsigned int kMaxNodes, class EdgePayload, unsigned int kMaxEdges, class Policy, unsigned int kMaxOutEdgesPerNode>
			int DirectedGraph<NodePayload, kMaxNodes, EdgePayload, kMaxEdges, Policy, kMaxOutEdgesPerNode>::FindNodeIndex(
				const Dia::Core::StringCRC& id) const
			{
				for (unsigned int i = 0; i < mNodeList.Size(); i++)
				{
					if (mNodeList.At(i).GetUniqueID() == id)
						return static_cast<int>(i);
				}
				return -1;
			}

			template <class NodePayload, unsigned int kMaxNodes, class EdgePayload, unsigned int kMaxEdges, class Policy, unsigned int kMaxOutEdgesPerNode>
			int DirectedGraph<NodePayload, kMaxNodes, EdgePayload, kMaxEdges, Policy, kMaxOutEdgesPerNode>::FindEdgeIndex(
				const Dia::Core::StringCRC& id) const
			{
				for (unsigned int i = 0; i < mEdgeList.Size(); i++)
				{
					if (mEdgeList.At(i).GetUniqueID() == id)
						return static_cast<int>(i);
				}
				return -1;
			}

			// ======================================================================
			// Private — CanReachNode (used by AcyclicEnforced cycle detection)
			// ======================================================================

			template <class NodePayload, unsigned int kMaxNodes, class EdgePayload, unsigned int kMaxEdges, class Policy, unsigned int kMaxOutEdgesPerNode>
			bool DirectedGraph<NodePayload, kMaxNodes, EdgePayload, kMaxEdges, Policy, kMaxOutEdgesPerNode>::CanReachNode(
				const Node* start, const Dia::Core::StringCRC& targetId) const
			{
				if (start->GetUniqueID() == targetId)
					return true;

				// Iterative DFS from start; no heap allocation
				DynamicArrayC<const Node*, kMaxNodes> visited;
				DynamicArrayC<const Node*, kMaxNodes> stack;
				stack.Add(start);

				while (stack.Size() > 0)
				{
					const Node* current = stack.Back();
					stack.Remove();

					if (current->GetUniqueID() == targetId)
						return true;

					bool alreadyVisited = false;
					for (unsigned int i = 0; i < visited.Size(); i++)
					{
						if (visited.At(i) == current) { alreadyVisited = true; break; }
					}
					if (alreadyVisited)
						continue;

					visited.Add(current);

					const typename Node::OutEdgeList& outEdges = current->GetOutEdgeList();
					for (unsigned int i = 0; i < outEdges.Size(); i++)
						stack.Add(outEdges.At(i)->GetTo());
				}
				return false;
			}

			// ======================================================================
			// Private — RebuildReverseCache (ReverseEdgeCache policy only)
			// ======================================================================

			template <class NodePayload, unsigned int kMaxNodes, class EdgePayload, unsigned int kMaxEdges, class Policy, unsigned int kMaxOutEdgesPerNode>
			void DirectedGraph<NodePayload, kMaxNodes, EdgePayload, kMaxEdges, Policy, kMaxOutEdgesPerNode>::RebuildReverseCache() const
			{
				// ReverseEdgeCache: clear all per-node in-edge lists, then re-populate
				// from the current edge list.  Called lazily on the first GetInEdges()
				// after any mutation (AddNode or AddEdge).
				for (unsigned int i = 0; i < mInEdgeCache.Size(); i++)
					mInEdgeCache[i].RemoveAll();

				for (unsigned int i = 0; i < mEdgeList.Size(); i++)
				{
					const Edge& edge = mEdgeList.At(i);
					int toIdx = FindNodeIndex(edge.GetTo()->GetUniqueID());
					if (toIdx >= 0)
						mInEdgeCache[static_cast<unsigned int>(toIdx)].Add(const_cast<Edge*>(&edge));
				}

				mCacheDirty = false;
			}

			// ======================================================================
			// Private — InvalidateReverseCache (no-op for non-ReverseEdgeCache policies)
			// ======================================================================

			template <class NodePayload, unsigned int kMaxNodes, class EdgePayload, unsigned int kMaxEdges, class Policy, unsigned int kMaxOutEdgesPerNode>
			void DirectedGraph<NodePayload, kMaxNodes, EdgePayload, kMaxEdges, Policy, kMaxOutEdgesPerNode>::InvalidateReverseCache()
			{
				// ReverseEdgeCache: mark the reverse index stale after any mutation
				if constexpr (std::is_same_v<Policy, GraphPolicy::ReverseEdgeCache>)
					mCacheDirty = true;
			}
		}
	}
}
