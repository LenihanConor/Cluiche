#pragma once

#include "DiaCore/Containers/Arrays/DynamicArrayC.h"
#include <DiaCore/CRC/StringCRC.h>

namespace Dia
{
	namespace Core
	{
		namespace Containers
		{
			template <class EdgePayload, class NodePayload>
			class GraphEdge;

			template <class NodePayload, class EdgePayload>
			class GraphNode
			{
			public:
				typedef GraphEdge<EdgePayload, NodePayload> Edge;

				typedef Dia::Core::Containers::DynamicArrayC<Edge*, 8> EdgeList;

				GraphNode();
				GraphNode(const StringCRC& uniqueId, const NodePayload& payload);
				~GraphNode();

				const Dia::Core::StringCRC& GetUniqueID()const { return mUniqueId; }

				NodePayload& GetPayload() { return mPayload; }
				const NodePayload& GetPayloadConst()const { return mPayload; }

				void AddEdgeAwayFromNode(Edge* edge);

			private:			
				Dia::Core::StringCRC mUniqueId;
				NodePayload mPayload;
				EdgeList mEdgesAwayFromNode;
			};
		}
	}
}

#include "DiaCore/Containers/Graphs/GraphNode.inl"