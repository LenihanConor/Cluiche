#pragma once

#include "DiaCore/Core/Assert.h"

namespace Dia
{
	namespace Core
	{
		namespace Containers
		{
			template <class NodePayload, class EdgePayload>
			class GraphNode;

			template <class EdgePayload, class NodePayload>
			class GraphEdge
			{
			public:
				GraphEdge();
				GraphEdge(const Dia::Core::StringCRC& uniqueId, const EdgePayload& payload, GraphNode<NodePayload, EdgePayload>* head, GraphNode<NodePayload, EdgePayload>* tail);
				~GraphEdge();

				const Dia::Core::StringCRC& GetUniqueID()const { return mUniqueId; }

				GraphNode<NodePayload, EdgePayload>* GetTail();
				const GraphNode<NodePayload, EdgePayload>* GetTail()const;

				GraphNode<NodePayload, EdgePayload>* GetHead();
				const GraphNode<NodePayload, EdgePayload>* GetHead()const;

				EdgePayload& GetPayload();
				const EdgePayload& GetPayloadConst()const;

			private:
				Dia::Core::StringCRC mUniqueId;
				EdgePayload mPayload;

				GraphNode<NodePayload, EdgePayload>* mHead;
				GraphNode<NodePayload, EdgePayload>* mTail;
			};
		}
	}
}

#include "DiaCore/Containers/Graphs/GraphEdge.inl"
