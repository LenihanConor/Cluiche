#pragma once

#include "DiaCore/CRC/StringCRC.h"

namespace Dia
{
	namespace Core
	{
		namespace Containers
		{
			template <class NodePayload, class EdgePayload, unsigned int kMaxOutEdges>
			class DirectedGraphNode;

			// -----------------------------------------------------------------------
			// DirectedGraphEdge
			//
			// A directed edge from one node (From) to another (To).
			// Stores a StringCRC ID, an EdgePayload, and typed pointers to the
			// source (From) and destination (To) nodes.
			//
			// Naming convention: From = tail, To = head (direction of the arrow).
			// -----------------------------------------------------------------------
			template <class EdgePayload, class NodePayload, unsigned int kMaxOutEdges>
			class DirectedGraphEdge
			{
			public:
				typedef DirectedGraphNode<NodePayload, EdgePayload, kMaxOutEdges> Node;

				DirectedGraphEdge();
				DirectedGraphEdge(const Dia::Core::StringCRC& uniqueId,
				                  const EdgePayload& payload,
				                  Node* from,
				                  Node* to);
				~DirectedGraphEdge();

				const Dia::Core::StringCRC& GetUniqueID() const { return mUniqueId; }

				Node*       GetFrom()       { return mFrom; }
				const Node* GetFrom() const { return mFrom; }

				Node*       GetTo()       { return mTo; }
				const Node* GetTo() const { return mTo; }

				EdgePayload&       GetPayload()            { return mPayload; }
				const EdgePayload& GetPayloadConst() const { return mPayload; }

			private:
				Dia::Core::StringCRC mUniqueId;
				EdgePayload          mPayload;
				Node*                mFrom;
				Node*                mTo;
			};
		}
	}
}

#include "DiaCore/Containers/Graphs/DirectedGraphEdge.inl"
