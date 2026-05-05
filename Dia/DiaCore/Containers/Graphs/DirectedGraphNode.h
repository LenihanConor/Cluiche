#pragma once

#include "DiaCore/Containers/Arrays/DynamicArrayC.h"
#include "DiaCore/CRC/StringCRC.h"

namespace Dia
{
	namespace Core
	{
		namespace Containers
		{
			template <class EdgePayload, class NodePayload, unsigned int kMaxOutEdges>
			class DirectedGraphEdge;

			// -----------------------------------------------------------------------
			// DirectedGraphNode
			//
			// A node in a DirectedGraph. Stores a StringCRC ID, a NodePayload, and
			// a fixed-capacity list of pointers to outgoing edges.
			//
			// kMaxOutEdges caps how many out-edges a single node may have. It must
			// be >= the kMaxEdges of the owning DirectedGraph in the worst case
			// (a star topology), but callers can tune it lower for memory savings.
			// -----------------------------------------------------------------------
			template <class NodePayload, class EdgePayload, unsigned int kMaxOutEdges>
			class DirectedGraphNode
			{
			public:
				typedef DirectedGraphEdge<EdgePayload, NodePayload, kMaxOutEdges> Edge;
				typedef Dia::Core::Containers::DynamicArrayC<Edge*, kMaxOutEdges> OutEdgeList;

				DirectedGraphNode();
				DirectedGraphNode(const Dia::Core::StringCRC& uniqueId, const NodePayload& payload);
				~DirectedGraphNode();

				const Dia::Core::StringCRC& GetUniqueID() const { return mUniqueId; }

				NodePayload&       GetPayload()             { return mPayload; }
				const NodePayload& GetPayloadConst() const  { return mPayload; }

				void AddOutEdge(Edge* edge);

				const OutEdgeList& GetOutEdgeList() const { return mOutEdges; }
				OutEdgeList&       GetOutEdgeList()       { return mOutEdges; }

			private:
				Dia::Core::StringCRC mUniqueId;
				NodePayload          mPayload;
				OutEdgeList          mOutEdges;
			};
		}
	}
}

#include "DiaCore/Containers/Graphs/DirectedGraphNode.inl"
