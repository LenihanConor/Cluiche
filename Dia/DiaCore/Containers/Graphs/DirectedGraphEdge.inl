
#include "DiaCore/Core/Assert.h"
#include "DiaCore/Containers/Graphs/DirectedGraphNode.h"

namespace Dia
{
	namespace Core
	{
		namespace Containers
		{
			template <class EdgePayload, class NodePayload, unsigned int kMaxOutEdges>
			DirectedGraphEdge<EdgePayload, NodePayload, kMaxOutEdges>::DirectedGraphEdge()
				: mPayload()
				, mFrom(nullptr)
				, mTo(nullptr)
			{}

			template <class EdgePayload, class NodePayload, unsigned int kMaxOutEdges>
			DirectedGraphEdge<EdgePayload, NodePayload, kMaxOutEdges>::DirectedGraphEdge(
				const Dia::Core::StringCRC& uniqueId,
				const EdgePayload& payload,
				Node* from,
				Node* to)
				: mUniqueId(uniqueId)
				, mPayload(payload)
				, mFrom(from)
				, mTo(to)
			{
				DIA_ASSERT(mFrom != nullptr, "DirectedGraphEdge: From node is null");
				DIA_ASSERT(mTo   != nullptr, "DirectedGraphEdge: To node is null");
			}

			template <class EdgePayload, class NodePayload, unsigned int kMaxOutEdges>
			DirectedGraphEdge<EdgePayload, NodePayload, kMaxOutEdges>::~DirectedGraphEdge()
			{
				mFrom = nullptr;
				mTo   = nullptr;
			}
		}
	}
}
