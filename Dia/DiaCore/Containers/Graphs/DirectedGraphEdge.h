#pragma once

#include "DiaCore/CRC/StringCRC.h"

namespace Dia
{
	namespace Core
	{
		namespace Containers
		{
			template <class NodePayload, class EdgePayload, unsigned int kMaxOutEdges, class IDType>
			class DirectedGraphNode;

			// -----------------------------------------------------------------------
			// DirectedGraphEdge
			//
			// A directed edge from one node (From) to another (To).
			// Stores an IDType ID (default: StringCRC), an EdgePayload, and typed
			// pointers to the source (From) and destination (To) nodes.
			//
			// IDType defaults to StringCRC; use Dia::Core::CRC for compact storage.
			// Naming convention: From = tail, To = head (direction of the arrow).
			// -----------------------------------------------------------------------
			template <class EdgePayload, class NodePayload, unsigned int kMaxOutEdges, class IDType = Dia::Core::StringCRC>
			class DirectedGraphEdge
			{
			public:
				typedef DirectedGraphNode<NodePayload, EdgePayload, kMaxOutEdges, IDType> Node;

				DirectedGraphEdge();
				DirectedGraphEdge(const IDType& uniqueId,
				                  const EdgePayload& payload,
				                  Node* from,
				                  Node* to);
				~DirectedGraphEdge();

				const IDType& GetUniqueID() const { return mUniqueId; }

				Node*       GetFrom()       { return mFrom; }
				const Node* GetFrom() const { return mFrom; }

				Node*       GetTo()       { return mTo; }
				const Node* GetTo() const { return mTo; }

				EdgePayload&       GetPayload()            { return mPayload; }
				const EdgePayload& GetPayloadConst() const { return mPayload; }

			private:
				IDType      mUniqueId;
				EdgePayload mPayload;
				Node*       mFrom;
				Node*       mTo;
			};
		}
	}
}

#include "DiaCore/Containers/Graphs/DirectedGraphEdge.inl"
