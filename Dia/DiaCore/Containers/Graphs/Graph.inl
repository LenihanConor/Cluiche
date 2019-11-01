#include "DiaCore/Memory/Memory.h"

namespace Dia
{
	namespace Core
	{
		namespace Containers
		{
			//------------------------------------------------------------------------------------
			template <class NodePayload, unsigned int kMaxNodes, class EdgePayload, unsigned int kMaxEdges>
			Graph<NodePayload, kMaxNodes, EdgePayload, kMaxEdges>::Graph()
			{}

			template <class NodePayload, unsigned int kMaxNodes, class EdgePayload, unsigned int kMaxEdges>
			void Graph<NodePayload, kMaxNodes, EdgePayload, kMaxEdges>::AddNode(const Node& node)
			{
				// Esnure the Edge name is unique
				int nodeUniqueID = FindNodeIndex(node.GetUniqueID());
				DIA_ASSERT(nodeUniqueID == -1, "Edge name [%s] already exists", node.GetUniqueID().AsChar());
				if (nodeUniqueID != -1)
					return;

				mNodeList.Add(node);
			}

			template <class NodePayload, unsigned int kMaxNodes, class EdgePayload, unsigned int kMaxEdges>
			void Graph<NodePayload, kMaxNodes, EdgePayload, kMaxEdges>::AddEdge(const Edge& edge)
			{
				// Esnure the Edge name is unique
				int edgeUniqueID = FindEdgeIndex(edge.GetUniqueID());
				DIA_ASSERT(edgeUniqueID == -1, "Edge name [%s] already exists", edge.GetUniqueID().AsChar());
					
				if (edgeUniqueID != -1)
					return;
				
				mEdgeList.Add(edge);
			}

			template <class NodePayload, unsigned int kMaxNodes, class EdgePayload, unsigned int kMaxEdges>
			unsigned int Graph<NodePayload, kMaxNodes, EdgePayload, kMaxEdges>::GetNumberOfNodes()const
			{
				return mNodeList.Size();
			}

			template <class NodePayload, unsigned int kMaxNodes, class EdgePayload, unsigned int kMaxEdges>
			unsigned int Graph<NodePayload, kMaxNodes, EdgePayload, kMaxEdges>::GetNumberOfEdges()const
			{
				return mEdgeList.Size();
			}

			template <class NodePayload, unsigned int kMaxNodes, class EdgePayload, unsigned int kMaxEdges>
			GraphNode<NodePayload, EdgePayload>* Graph<NodePayload, kMaxNodes, EdgePayload, kMaxEdges>::FindNode(const Dia::Core::StringCRC& name)
			{
				int nodeIndex = FindNodeIndex(name);
				if (nodeIndex == -1)
					return nullptr;

				return &mNodeList[nodeIndex];
			}

			template <class NodePayload, unsigned int kMaxNodes, class EdgePayload, unsigned int kMaxEdges>
			const GraphNode<NodePayload, EdgePayload>* Graph<NodePayload, kMaxNodes, EdgePayload, kMaxEdges>::FindNode(const Dia::Core::StringCRC& name)const
			{
				int nodeIndex = FindNodeIndex(name);
				if (nodeIndex == -1)
					return nullptr;

				return &mNodeList[nodeIndex];
			}

			template <class NodePayload, unsigned int kMaxNodes, class EdgePayload, unsigned int kMaxEdges>
			GraphEdge<EdgePayload, NodePayload>* Graph<NodePayload, kMaxNodes, EdgePayload, kMaxEdges>::FindEdge(const Dia::Core::StringCRC& name)
			{
				int edgeIndex = FindEdgeIndex(name);
				if (edgeIndex == -1)
					return nullptr;

				return &mEdgeList[edgeIndex];
			}

			template <class NodePayload, unsigned int kMaxNodes, class EdgePayload, unsigned int kMaxEdges>
			const GraphEdge<EdgePayload, NodePayload>* Graph<NodePayload, kMaxNodes, EdgePayload, kMaxEdges>::FindEdge(const Dia::Core::StringCRC& name)const
			{
				int edgeIndex = FindEdgeIndex(name);
				if (edgeIndex == -1)
					return nullptr;

				return &mEdgeList[edgeIndex];
			}

			template <class NodePayload, unsigned int kMaxNodes, class EdgePayload, unsigned int kMaxEdges>
			int Graph<NodePayload, kMaxNodes, EdgePayload, kMaxEdges>::FindNodeIndex(const StringCRC& name)const
			{
				int index = -1;
				for (unsigned int i = 0; i < mNodeList.Size(); i++)
				{
					if (mNodeList.At(i).GetUniqueID() == name)
					{
						index = i;
						break;
					}
				}
				return index;
			}

			template <class NodePayload, unsigned int kMaxNodes, class EdgePayload, unsigned int kMaxEdges>
			int Graph<NodePayload, kMaxNodes, EdgePayload, kMaxEdges>::FindEdgeIndex(const StringCRC& name)const
			{
				int index = -1;
				for (unsigned int i = 0; i < mEdgeList.Size(); i++)
				{
					if (mEdgeList.At(i).GetUniqueID() == name)
					{
						index = i;
						break;
					}
				}
				return index;
			}
		}
	}
}