#pragma once

#include "DiaCore/Containers/Arrays/DynamicArrayC.h"
#include "DiaCore/Containers/Graphs/GraphNode.h"
#include "DiaCore/Containers/Graphs/GraphEdge.h"

namespace Dia
{
	namespace Core
	{
		namespace Containers
		{
			//------------------------------------------------------------------------------------
			//	Interface
			//------------------------------------------------------------------------------------
			template <class NodePayload, unsigned int kMaxNodes, class EdgePayload, unsigned int kMaxEdges>
			class Graph
			{
			public:
				typedef GraphNode<NodePayload, EdgePayload> Node;
				typedef GraphEdge<EdgePayload, NodePayload> Edge;

				typedef Dia::Core::Containers::DynamicArrayC<Node, kMaxNodes> NodeList;
				typedef Dia::Core::Containers::DynamicArrayC<Edge, kMaxEdges> EdgeList;
			
				Graph();																								
				
				void AddNode(const Node& node);
				void AddEdge(const Edge& edge);

				unsigned int GetNumberOfNodes()const;
				unsigned int GetNumberOfEdges()const;
				
				Node* FindNode(const Dia::Core::StringCRC& name);
				const Node* FindNode(const Dia::Core::StringCRC& name)const;
				
				Edge* FindEdge(const Dia::Core::StringCRC& name);
				const Edge* FindEdge(const Dia::Core::StringCRC& name)const;

			private:
				int FindNodeIndex(const Dia::Core::StringCRC& name)const;
				int FindEdgeIndex(const Dia::Core::StringCRC& name)const;

				NodeList mNodeList;
				EdgeList mEdgeList;
			};
		}
	}
}

#include "DiaCore/Containers/Graphs/Graph.inl"