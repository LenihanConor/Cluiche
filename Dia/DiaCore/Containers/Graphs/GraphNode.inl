
#include "DiaCore/Core/Assert.h"

namespace Dia
{
	namespace Core
	{
		namespace Containers
		{
			//------------------------------------------------------------------------------------
			template <class NodePayload, class EdgePayload>
			GraphNode<NodePayload, EdgePayload>::GraphNode()
			{}

			template <class NodePayload, class EdgePayload >
			GraphNode<NodePayload, EdgePayload>::GraphNode(const Dia::Core::StringCRC& uniqueId, const NodePayload& payload)
				: mUniqueId(uniqueId)
				, mPayload(payload)
			{}

			template <class NodePayload, class EdgePayload>
			GraphNode<NodePayload, EdgePayload>::~GraphNode()
			{}

			template <class NodePayload, class EdgePayload>
			void GraphNode<NodePayload, EdgePayload>::AddEdgeAwayFromNode(Edge* edge)
			{
				DIA_ASSERT(edge != nullptr, "Edge being inserted into node is null");

				mEdgesAwayFromNode.Add(edge);
			}
		}
	}
}

