#pragma once

#include "DiaCore/Core/Assert.h"


namespace Dia
{
	namespace Core
	{
		namespace Containers
		{
			template <class EdgePayload, class VertexPayload>
			class GraphVertex;

			template <class EdgePayload, class VertexPayload>
			class GraphEdge
			{
			public:
				GraphEdge();
				~GraphEdge();

				GraphVertex* GetTail();
				const GraphVertex* GetTail()const;

				GraphVertex* GetHead();
				const GraphVertex* GetHead()const;

				void SetTail(GraphVertex* vert);
				void SetHead(GraphVertex* head);

				void SetPayload(EdgePayload* payload);

				EdgePayload& GetPayload();
				const EdgePayload& GetPayloadConst()const;

			private:
				GraphVertex* mTail;
				GraphVertex* mHead;
				EdgePayload mPayload;
			};
		}
	}
}


#include "DiaCore/Containers/Graphs/GraphEdge.inl"
