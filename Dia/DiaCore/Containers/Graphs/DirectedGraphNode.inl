
#include "DiaCore/Core/Assert.h"

namespace Dia
{
	namespace Core
	{
		namespace Containers
		{
			template <class NodePayload, class EdgePayload, unsigned int kMaxOutEdges, class IDType>
			DirectedGraphNode<NodePayload, EdgePayload, kMaxOutEdges, IDType>::DirectedGraphNode()
				: mPayload()
			{}

			template <class NodePayload, class EdgePayload, unsigned int kMaxOutEdges, class IDType>
			DirectedGraphNode<NodePayload, EdgePayload, kMaxOutEdges, IDType>::DirectedGraphNode(const IDType& uniqueId, const NodePayload& payload)
				: mUniqueId(uniqueId)
				, mPayload(payload)
			{}

			template <class NodePayload, class EdgePayload, unsigned int kMaxOutEdges, class IDType>
			DirectedGraphNode<NodePayload, EdgePayload, kMaxOutEdges, IDType>::~DirectedGraphNode()
			{}

			template <class NodePayload, class EdgePayload, unsigned int kMaxOutEdges, class IDType>
			void DirectedGraphNode<NodePayload, EdgePayload, kMaxOutEdges, IDType>::AddOutEdge(Edge* edge)
			{
				DIA_ASSERT(edge != nullptr, "Out-edge pointer is null");
				DIA_ASSERT(!mOutEdges.IsFull(), "Out-edge list is full for this node");
				mOutEdges.Add(edge);
			}
		}
	}
}
