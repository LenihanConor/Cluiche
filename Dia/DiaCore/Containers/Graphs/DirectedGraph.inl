
#include <type_traits>
#include "DiaCore/Core/Assert.h"

namespace Dia
{
	namespace Core
	{
		namespace Containers
		{
			// Shorthand for the full template parameter list used throughout this file.
			#define DG_TPARAMS class NodePayload, unsigned int kMaxNodes, class EdgePayload, unsigned int kMaxEdges, class Policy, unsigned int kMaxOutEdgesPerNode, class IDType
			#define DG_TARGS   NodePayload, kMaxNodes, EdgePayload, kMaxEdges, Policy, kMaxOutEdgesPerNode, IDType

			// ======================================================================
			// Constructor
			// ======================================================================

			template <DG_TPARAMS>
			DirectedGraph<DG_TARGS>::DirectedGraph()
				: mCacheDirty(true)
			{}

			// ======================================================================
			// Mutation — Clear
			// ======================================================================

			template <DG_TPARAMS>
			void DirectedGraph<DG_TARGS>::Clear()
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

			template <DG_TPARAMS>
			bool DirectedGraph<DG_TARGS>::AddNode(const IDType& id, const NodePayload& payload)
			{
				DIA_ASSERT(FindNodeIndex(id) == -1, "AddNode: node ID 0x%08X already exists", id.Value());
				if (FindNodeIndex(id) != -1)
					return false;

				DIA_ASSERT(!mNodeList.IsFull(), "AddNode: node capacity (%u) is full", kMaxNodes);
				if (mNodeList.IsFull())
					return false;

				mNodeList.Add(Node(id, payload));

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

			template <DG_TPARAMS>
			bool DirectedGraph<DG_TARGS>::AddEdge(
				const IDType& id,
				const IDType& fromNodeId,
				const IDType& toNodeId,
				const EdgePayload& payload)
			{
				DIA_ASSERT(FindEdgeIndex(id) == -1, "AddEdge: edge ID 0x%08X already exists", id.Value());
				if (FindEdgeIndex(id) != -1)
					return false;

				int fromIdx = FindNodeIndex(fromNodeId);
				DIA_ASSERT(fromIdx != -1, "AddEdge: from node 0x%08X not found", fromNodeId.Value());
				if (fromIdx == -1)
					return false;

				int toIdx = FindNodeIndex(toNodeId);
				DIA_ASSERT(toIdx != -1, "AddEdge: to node 0x%08X not found", toNodeId.Value());
				if (toIdx == -1)
					return false;

				DIA_ASSERT(!mEdgeList.IsFull(), "AddEdge: edge capacity (%u) is full", kMaxEdges);
				if (mEdgeList.IsFull())
					return false;

				Node* fromNode = &mNodeList[fromIdx];
				Node* toNode   = &mNodeList[toIdx];

				if constexpr (std::is_same_v<Policy, GraphPolicy::AcyclicEnforced>)
				{
					if (CanReachNode(toNode, fromNodeId))
					{
						DIA_ASSERT(false, "AddEdge: edge 0x%08X would create a cycle (AcyclicEnforced policy)", id.Value());
						return false;
					}
				}

				mEdgeList.Add(Edge(id, payload, fromNode, toNode));
				fromNode->AddOutEdge(&mEdgeList.Back());

				InvalidateReverseCache();

				return true;
			}

			// ======================================================================
			// Query — FindNode / FindEdge
			// ======================================================================

			template <DG_TPARAMS>
			typename DirectedGraph<DG_TARGS>::Node*
			DirectedGraph<DG_TARGS>::FindNode(const IDType& id)
			{
				int index = FindNodeIndex(id);
				return (index == -1) ? nullptr : &mNodeList[index];
			}

			template <DG_TPARAMS>
			const typename DirectedGraph<DG_TARGS>::Node*
			DirectedGraph<DG_TARGS>::FindNode(const IDType& id) const
			{
				int index = FindNodeIndex(id);
				return (index == -1) ? nullptr : &mNodeList[index];
			}

			template <DG_TPARAMS>
			typename DirectedGraph<DG_TARGS>::Edge*
			DirectedGraph<DG_TARGS>::FindEdge(const IDType& id)
			{
				int index = FindEdgeIndex(id);
				return (index == -1) ? nullptr : &mEdgeList[index];
			}

			template <DG_TPARAMS>
			const typename DirectedGraph<DG_TARGS>::Edge*
			DirectedGraph<DG_TARGS>::FindEdge(const IDType& id) const
			{
				int index = FindEdgeIndex(id);
				return (index == -1) ? nullptr : &mEdgeList[index];
			}

			// ======================================================================
			// Query — counts
			// ======================================================================

			template <DG_TPARAMS>
			unsigned int DirectedGraph<DG_TARGS>::GetNumberOfNodes() const
			{
				return mNodeList.Size();
			}

			template <DG_TPARAMS>
			unsigned int DirectedGraph<DG_TARGS>::GetNumberOfEdges() const
			{
				return mEdgeList.Size();
			}

			// ======================================================================
			// Query — GetOutEdges
			// ======================================================================

			template <DG_TPARAMS>
			void DirectedGraph<DG_TARGS>::GetOutEdges(const IDType& nodeId, EdgeResults& results) const
			{
				results.RemoveAll();
				const Node* node = FindNode(nodeId);
				if (!node)
					return;

				const typename Node::OutEdgeList& outEdges = node->GetOutEdgeList();
				for (unsigned int i = 0; i < outEdges.Size(); i++)
					results.Add(outEdges.At(i));
			}

			// ======================================================================
			// Query — GetInEdges (policy-dispatched)
			// ======================================================================

			template <DG_TPARAMS>
			void DirectedGraph<DG_TARGS>::GetInEdges(const IDType& nodeId, EdgeResults& results) const
			{
				results.RemoveAll();

				if constexpr (std::is_same_v<Policy, GraphPolicy::ReverseEdgeCache>)
				{
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
					for (unsigned int i = 0; i < mEdgeList.Size(); i++)
					{
						const Edge& edge = mEdgeList.At(i);
						if (edge.GetTo()->GetUniqueID() == nodeId)
							results.Add(const_cast<Edge*>(&edge));
					}
				}
			}

			// ======================================================================
			// Traversal — BFS
			// ======================================================================

			template <DG_TPARAMS>
			template <class Visitor>
			void DirectedGraph<DG_TARGS>::BFS(
				const IDType& startNodeId,
				const Visitor& visitor,
				DynamicArrayC<const Node*, kMaxNodes>& visitedBuffer) const
			{
				visitedBuffer.RemoveAll();

				const Node* start = FindNode(startNodeId);
				if (!start)
					return;

				visitedBuffer.Add(start);

				for (unsigned int front = 0; front < visitedBuffer.Size(); front++)
				{
					const Node* current = visitedBuffer.At(front);
					visitor(*current);

					const typename Node::OutEdgeList& outEdges = current->GetOutEdgeList();
					for (unsigned int i = 0; i < outEdges.Size(); i++)
					{
						const Node* neighbor = outEdges.At(i)->GetTo();

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

			template <DG_TPARAMS>
			template <class Visitor>
			void DirectedGraph<DG_TARGS>::DFS(
				const IDType& startNodeId,
				const Visitor& visitor,
				DynamicArrayC<const Node*, kMaxNodes>& visitedBuffer) const
			{
				visitedBuffer.RemoveAll();

				const Node* start = FindNode(startNodeId);
				if (!start)
					return;

				DynamicArrayC<const Node*, kMaxNodes> stack;
				stack.Add(start);

				while (stack.Size() > 0)
				{
					const Node* current = stack.Back();
					stack.Remove();

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

					const typename Node::OutEdgeList& outEdges = current->GetOutEdgeList();
					for (int i = static_cast<int>(outEdges.Size()) - 1; i >= 0; i--)
						stack.Add(outEdges.At(i)->GetTo());
				}
			}

			// ======================================================================
			// Traversal — TopoSort (Kahn's algorithm)
			// ======================================================================

			template <DG_TPARAMS>
			bool DirectedGraph<DG_TARGS>::TopoSort(
				NodeResults& results,
				DynamicArrayC<const Node*, kMaxNodes>& workBuffer) const
			{
				results.RemoveAll();
				workBuffer.RemoveAll();

				DynamicArrayC<unsigned int, kMaxNodes> inDegree;
				for (unsigned int i = 0; i < mNodeList.Size(); i++)
					inDegree.Add(0u);

				for (unsigned int i = 0; i < mEdgeList.Size(); i++)
				{
					int toIdx = FindNodeIndex(mEdgeList.At(i).GetTo()->GetUniqueID());
					if (toIdx >= 0)
						inDegree[static_cast<unsigned int>(toIdx)]++;
				}

				for (unsigned int i = 0; i < mNodeList.Size(); i++)
				{
					if (inDegree.At(i) == 0)
						workBuffer.Add(&mNodeList.At(i));
				}

				for (unsigned int front = 0; front < workBuffer.Size(); front++)
				{
					const Node* current = workBuffer.At(front);
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

				return results.Size() == mNodeList.Size();
			}

			// ======================================================================
			// Traversal — HasCycle
			// ======================================================================

			template <DG_TPARAMS>
			bool DirectedGraph<DG_TARGS>::HasCycle() const
			{
				if constexpr (std::is_same_v<Policy, GraphPolicy::AcyclicEnforced>)
				{
					return false;
				}
				else
				{
					NodeResults results;
					DynamicArrayC<const Node*, kMaxNodes> workBuffer;
					return !TopoSort(results, workBuffer);
				}
			}

			// ======================================================================
			// Private — FindNodeIndex / FindEdgeIndex
			// ======================================================================

			template <DG_TPARAMS>
			int DirectedGraph<DG_TARGS>::FindNodeIndex(const IDType& id) const
			{
				for (unsigned int i = 0; i < mNodeList.Size(); i++)
				{
					if (mNodeList.At(i).GetUniqueID() == id)
						return static_cast<int>(i);
				}
				return -1;
			}

			template <DG_TPARAMS>
			int DirectedGraph<DG_TARGS>::FindEdgeIndex(const IDType& id) const
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

			template <DG_TPARAMS>
			bool DirectedGraph<DG_TARGS>::CanReachNode(const Node* start, const IDType& targetId) const
			{
				if (start->GetUniqueID() == targetId)
					return true;

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

			template <DG_TPARAMS>
			void DirectedGraph<DG_TARGS>::RebuildReverseCache() const
			{
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
			// Private — InvalidateReverseCache
			// ======================================================================

			template <DG_TPARAMS>
			void DirectedGraph<DG_TARGS>::InvalidateReverseCache()
			{
				if constexpr (std::is_same_v<Policy, GraphPolicy::ReverseEdgeCache>)
					mCacheDirty = true;
			}

			#undef DG_TPARAMS
			#undef DG_TARGS
		}
	}
}
