
#include "DiaCore/Core/Assert.h"
#include "DiaCore/Containers/Graphs/GraphEdge.h"

namespace Dia
{
	namespace Core
	{
		namespace Containers
		{
			//------------------------------------------------------------------------------------
			template <class EdgePayload, class VertexPayload>
			GraphVertex<EdgePayload, VertexPayload>::GraphVertex()
			{
				mEdges.RemoveAll();
			}

			//------------------------------------------------------------------------------------
			template <class EdgePayload, class VertexPayload>
			GraphVertex<EdgePayload, VertexPayload>::GraphVertex(unsigned int numberOfEdges)
				: mEdges(numberOfEdges)
			{}

			//------------------------------------------------------------------------------------
			template <class EdgePayload, class VertexPayload>
			GraphVertex<EdgePayload, VertexPayload>::~GraphVertex()
			{
				mEdges.RemoveAll();
			}

			//------------------------------------------------------------------------------------
			template <class EdgePayload, class VertexPayload>
			void GraphVertex<EdgePayload, VertexPayload>::ReserveNumberOfEdges(unsigned int numberOfEdges)
			{
				mEdges.Reserve(numberOfEdges);
			}

			//------------------------------------------------------------------------------------
			template <class EdgePayload, class VertexPayload>
			void GraphVertex<EdgePayload, VertexPayload>::AddEdge(GraphEdge* pEdge)
			{
				if (mEdges.IsFull())
				{
					DIA_ASSERT(1, "Cannot add any more edges to this vertex");
					return;
				}

				mEdges.Add(pEdge);
			}

			//------------------------------------------------------------------------------------
			template <class EdgePayload, class VertexPayload>
			void GraphVertex<EdgePayload, VertexPayload>::RemoveEdge(GraphEdge* pEdge)
			{
				if (mEdges.IsEmpty())
				{
					DIA_ASSERT(1, "Removing an edge that does not exist");
					return;
				}

				int index = mEdges.FindIndex(pEdge)
				
				if (index == -1)
				{
					DIA_ASSERT(1, "Could not find edge to remove");
					return;
				}

				mEdges.Remove(index);
			}
				
			//------------------------------------------------------------------------------------
			template <class EdgePayload, class VertexPayload>
			GraphVertex<EdgePayload, VertexPayload>& GraphVertex<EdgePayload, VertexPayload>::GetEdges()
			{
				return mEdges;
			}

			//------------------------------------------------------------------------------------
			template <class EdgePayload, class VertexPayload>
			const EdgeVector<EdgePayload, VertexPayload>& GraphVertex<EdgePayload, VertexPayload>::GetEdges()const
			{
				return mEdges;
			}

			//------------------------------------------------------------------------------------
			template <class EdgePayload, class VertexPayload>
			void GraphVertex<EdgePayload, VertexPayload>::SetPayload(VertexPayload* payload)
			{
				mPayload = payload;
			}

			//------------------------------------------------------------------------------------
			template <class EdgePayload, class VertexPayload>
			VertexPayload& GraphVertex<EdgePayload, VertexPayload>::GetPayload()
			{
				return mPayload;
			}

			//------------------------------------------------------------------------------------
			template <class EdgePayload, class VertexPayload>
			const VertexPayload& GraphVertex<EdgePayload, VertexPayload>::GetPayloadConst()const
			{
				return mPayload;
			}
		}
	}
}

