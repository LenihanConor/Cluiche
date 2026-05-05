#pragma once

#include "DiaCore/Containers/Arrays/DynamicArrayC.h"
#include "DiaCore/CRC/StringCRC.h"

namespace Dia
{
	namespace Core
	{
		namespace Containers
		{
			template <class EdgePayload, class NodePayload, unsigned int kMaxOutEdges, class IDType>
			class DirectedGraphEdge;

			// -----------------------------------------------------------------------
			// DirectedGraphNode
			//
			// A node in a DirectedGraph. Stores an IDType ID (default: StringCRC),
			// a NodePayload, and a fixed-capacity list of pointers to outgoing edges.
			//
			// kMaxOutEdges caps how many out-edges a single node may have.
			// IDType defaults to StringCRC; use Dia::Core::CRC for a compact 4-byte key
			// when debug string storage is not needed (e.g., large internal graphs).
			// -----------------------------------------------------------------------
			template <class NodePayload, class EdgePayload, unsigned int kMaxOutEdges, class IDType = Dia::Core::StringCRC>
			class DirectedGraphNode
			{
			public:
				typedef DirectedGraphEdge<EdgePayload, NodePayload, kMaxOutEdges, IDType> Edge;
				typedef Dia::Core::Containers::DynamicArrayC<Edge*, kMaxOutEdges> OutEdgeList;

				DirectedGraphNode();
				DirectedGraphNode(const IDType& uniqueId, const NodePayload& payload);
				~DirectedGraphNode();

				const IDType& GetUniqueID() const { return mUniqueId; }

				NodePayload&       GetPayload()             { return mPayload; }
				const NodePayload& GetPayloadConst() const  { return mPayload; }

				void AddOutEdge(Edge* edge);

				const OutEdgeList& GetOutEdgeList() const { return mOutEdges; }
				OutEdgeList&       GetOutEdgeList()       { return mOutEdges; }

			private:
				IDType      mUniqueId;
				NodePayload mPayload;
				OutEdgeList mOutEdges;
			};
		}
	}
}

#include "DiaCore/Containers/Graphs/DirectedGraphNode.inl"
