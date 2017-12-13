#pragma once

#include "DiaCore/Containers/Arrays/DynamicArray.h"

namespace Dia
{
	namespace Core
	{
		namespace Containers
		{
			template <class EdgePayload, class VertexPayload>
			class GraphEdge;

			template <class EdgePayload, class VertexPayload>
			class GraphVertex
			{
			public:
				typedef DynamicArray<GraphEdge<EdgePayload, VertexPayload>*> EdgeVector;

				GraphVertex();
				GraphVertex(unsigned int numberOfEdges);
				~GraphVertex();

				void ReserveNumberOfEdges(unsigned int numberOfEdges);

				void AddEdge(GraphEdge* pEdge);
				void RemoveEdge(GraphEdge* pEdge);
				
				EdgeVector& GetEdges();
				const EdgeVector& GetEdges()const;

				void SetPayload(VertexPayload* payload);

				VertexPayload& GetPayload();
				const VertexPayload& GetPayloadConst()const;

			private:
				EdgeVector mEdges;
				VertexPayload mPayload;
			};
		}
	}
}

#include "DiaCore/Containers/Graphs/GraphEdge.inl"