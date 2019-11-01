
#include "DiaCore/Containers/Graphs/GraphNode.h"

namespace Dia
{
	namespace Core
	{
		namespace Containers
		{
			template <class EdgePayload, class NodePayload> inline
			GraphEdge<EdgePayload, NodePayload>::GraphEdge()
				: mTail(NULL)
				, mHead(NULL)
			{}

			template <class EdgePayload, class NodePayload> inline
			GraphEdge<EdgePayload, NodePayload>::GraphEdge(const Dia::Core::StringCRC& uniqueId, const EdgePayload& payload, GraphNode<NodePayload, EdgePayload>* head, GraphNode<NodePayload, EdgePayload>* tail)
				: mUniqueId(uniqueId)
				, mPayload(payload)
				, mHead(head)
				, mTail(tail)
			{
				DIA_ASSERT(mHead != nullptr, "Head being inserted into node is null");
				DIA_ASSERT(mTail != nullptr, "Tail being inserted into node is null");

				mHead->AddEdgeAwayFromNode(this);
			}

			template <class EdgePayload, class NodePayload> inline
			GraphEdge<EdgePayload, NodePayload>::~GraphEdge()
			{
				mTail = NULL;
				mHead = NULL;
			}

			template <class EdgePayload, class NodePayload>
			GraphNode<NodePayload, EdgePayload>* GraphEdge<EdgePayload, NodePayload>::GetTail()
			{
				return mTail;
			}

			template <class EdgePayload, class NodePayload>
			const GraphNode<NodePayload, EdgePayload>* GraphEdge<EdgePayload, NodePayload>::GetTail()const
			{
				return mTail;
			}

			template <class EdgePayload, class NodePayload>
			GraphNode<NodePayload, EdgePayload>* GraphEdge<EdgePayload, NodePayload>::GetHead()
			{
				return mHead;
			}

			template <class EdgePayload, class NodePayload>
			const GraphNode<NodePayload, EdgePayload>* GraphEdge<EdgePayload, NodePayload>::GetHead()const
			{
				return mHead;
			}

			template <class EdgePayload, class NodePayload>
			EdgePayload& GraphEdge<EdgePayload, NodePayload>::GetPayload()
			{
				return mPayload;
			}

			template <class EdgePayload, class NodePayload>
			const EdgePayload& GraphEdge<EdgePayload, NodePayload>::GetPayloadConst()const
			{
				return mPayload;
			}
		}
	}
}

