
#include "DiaCore/Core/Assert.h"

namespace Dia
{
	namespace Core
	{
		namespace Containers
		{
			template <class NodePayload, class EdgePayload, unsigned int kMaxOutEdges>
			DirectedGraphNode<NodePayload, EdgePayload, kMaxOutEdges>::DirectedGraphNode()
				: mPayload()
			{}

			template <class NodePayload, class EdgePayload, unsigned int kMaxOutEdges>
			DirectedGraphNode<NodePayload, EdgePayload, kMaxOutEdges>::DirectedGraphNode(const Dia::Core::StringCRC& uniqueId, const NodePayload& payload)
				: mUniqueId(uniqueId)
				, mPayload(payload)
			{}

			template <class NodePayload, class EdgePayload, unsigned int kMaxOutEdges>
			DirectedGraphNode<NodePayload, EdgePayload, kMaxOutEdges>::~DirectedGraphNode()
			{}

			template <class NodePayload, class EdgePayload, unsigned int kMaxOutEdges>
			void DirectedGraphNode<NodePayload, EdgePayload, kMaxOutEdges>::AddOutEdge(Edge* edge)
			{
				DIA_ASSERT(edge != nullptr, "Out-edge pointer is null");
				DIA_ASSERT(!mOutEdges.IsFull(), "Out-edge list is full for this node");
				mOutEdges.Add(edge);
			}
		}
	}
}
