
#include "DiaCore/Containers/Graphs/GraphVertex.h"

namespace Dia
{
	namespace Core
	{
		namespace Containers
		{
			template <class EdgePayload, class VertexPayload> inline
			GraphEdge<EdgePayload, VertexPayload>::GraphEdge()
				: mTail(NULL)
				, mHead(NULL)
			{}

			template <class EdgePayload, class VertexPayload> inline
			GraphEdge<EdgePayload, VertexPayload>::~GraphEdge()
			{
				mTail = NULL;
				mHead = NULL;
			}

			template <class EdgePayload, class VertexPayload>
			GraphVertex<EdgePayload, VertexPayload>* GraphEdge<EdgePayload, VertexPayload>::GetTail()
			{
				return mTail;
			}

			template <class EdgePayload, class VertexPayload>
			const GraphVertex<EdgePayload, VertexPayload>* GraphEdge<EdgePayload, VertexPayload>::GetTail()const
			{
				return mTail;
			}

			template <class EdgePayload, class VertexPayload>
			GraphVertex<EdgePayload, VertexPayload>* GraphEdge<EdgePayload, VertexPayload>::GetHead()
			{
				return mHead;
			}

			template <class EdgePayload, class VertexPayload>
			const GraphVertex<EdgePayload, VertexPayload>* GraphEdge<EdgePayload, VertexPayload>::GetHead()const
			{
				return mHead;
			}

			template <class EdgePayload, class VertexPayload>
			void GraphEdge<EdgePayload, VertexPayload>::SetTail(GraphVertex* vert)
			{
				mTail = vert;
			}

			template <class EdgePayload, class VertexPayload>
			void GraphEdge<EdgePayload, VertexPayload>::SetHead(GraphVertex* vert)
			{
				mHead = vert;
			}

			template <class EdgePayload, class VertexPayload>
			void GraphEdge<EdgePayload, VertexPayload>::SetPayload(EdgePayload* payload)
			{

			}

			template <class EdgePayload, class VertexPayload>
			EdgePayload& GraphEdge<EdgePayload, VertexPayload>::GetPayload()
			{
				return mPayload;
			}

			template <class EdgePayload, class VertexPayload>
			const EdgePayload& GraphEdge<EdgePayload, VertexPayload>::GetPayloadConst()const
			{
				return mPayload;
			}
		}
	}
}

