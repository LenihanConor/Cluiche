#include <gtest/gtest.h>
#include <DiaCore/Containers/Graphs/Graph.h>
#include <DiaCore/CRC/StringCRC.h>

using namespace Dia::Core::Containers;

typedef Graph<int, 8, float, 16> TestGraph;
typedef TestGraph::Node TestNode;
typedef TestGraph::Edge TestEdge;

TEST(Graph, DefaultConstruction_Empty_ZeroNodesAndEdges)
{
	TestGraph graph;

	EXPECT_EQ(graph.GetNumberOfNodes(), 0u);
	EXPECT_EQ(graph.GetNumberOfEdges(), 0u);
}

TEST(Graph, AddNode_SingleNode_IncrementsCount)
{
	TestGraph graph;
	TestNode node(Dia::Core::StringCRC("node1"), 42);

	graph.AddNode(node);

	EXPECT_EQ(graph.GetNumberOfNodes(), 1u);
}

#ifdef DEBUG
TEST(Graph, AddNode_DuplicateName_Asserts)
{
	TestGraph graph;
	TestNode node1(Dia::Core::StringCRC("duplicate"), 1);
	TestNode node2(Dia::Core::StringCRC("duplicate"), 2);

	graph.AddNode(node1);

	EXPECT_DEATH(graph.AddNode(node2), "");
}
#endif

TEST(Graph, AddEdge_SingleEdge_IncrementsCount)
{
	TestGraph graph;
	TestNode headNode(Dia::Core::StringCRC("head"), 10);
	TestNode tailNode(Dia::Core::StringCRC("tail"), 20);

	graph.AddNode(headNode);
	graph.AddNode(tailNode);

	TestNode* headPtr = graph.FindNode(Dia::Core::StringCRC("head"));
	TestNode* tailPtr = graph.FindNode(Dia::Core::StringCRC("tail"));

	TestEdge edge(Dia::Core::StringCRC("edge1"), 1.5f, headPtr, tailPtr);

	graph.AddEdge(edge);

	EXPECT_EQ(graph.GetNumberOfEdges(), 1u);
}

#ifdef DEBUG
TEST(Graph, AddEdge_DuplicateName_Asserts)
{
	TestGraph graph;
	TestNode headNode(Dia::Core::StringCRC("head"), 10);
	TestNode tailNode(Dia::Core::StringCRC("tail"), 20);

	graph.AddNode(headNode);
	graph.AddNode(tailNode);

	TestNode* headPtr = graph.FindNode(Dia::Core::StringCRC("head"));
	TestNode* tailPtr = graph.FindNode(Dia::Core::StringCRC("tail"));

	TestEdge edge1(Dia::Core::StringCRC("dup_edge"), 1.0f, headPtr, tailPtr);
	TestEdge edge2(Dia::Core::StringCRC("dup_edge"), 2.0f, headPtr, tailPtr);

	graph.AddEdge(edge1);

	EXPECT_DEATH(graph.AddEdge(edge2), "");
}
#endif

TEST(Graph, FindNode_ExistingName_ReturnsCorrectNode)
{
	TestGraph graph;
	TestNode node(Dia::Core::StringCRC("target"), 99);

	graph.AddNode(node);

	TestNode* found = graph.FindNode(Dia::Core::StringCRC("target"));

	ASSERT_NE(found, nullptr);
	EXPECT_EQ(found->GetPayloadConst(), 99);
	EXPECT_EQ(found->GetUniqueID(), Dia::Core::StringCRC("target"));
}

TEST(Graph, FindNode_UnknownName_ReturnsNullptr)
{
	TestGraph graph;
	TestNode node(Dia::Core::StringCRC("exists"), 1);

	graph.AddNode(node);

	TestNode* found = graph.FindNode(Dia::Core::StringCRC("missing"));

	EXPECT_EQ(found, nullptr);
}

TEST(Graph, FindNodeConst_ExistingName_ReturnsCorrectNode)
{
	TestGraph graph;
	TestNode node(Dia::Core::StringCRC("constTarget"), 77);

	graph.AddNode(node);

	const TestGraph& constGraph = graph;
	const TestNode* found = constGraph.FindNode(Dia::Core::StringCRC("constTarget"));

	ASSERT_NE(found, nullptr);
	EXPECT_EQ(found->GetPayloadConst(), 77);
	EXPECT_EQ(found->GetUniqueID(), Dia::Core::StringCRC("constTarget"));
}

TEST(Graph, FindEdge_ExistingName_ReturnsCorrectEdge)
{
	TestGraph graph;
	TestNode headNode(Dia::Core::StringCRC("head"), 10);
	TestNode tailNode(Dia::Core::StringCRC("tail"), 20);

	graph.AddNode(headNode);
	graph.AddNode(tailNode);

	TestNode* headPtr = graph.FindNode(Dia::Core::StringCRC("head"));
	TestNode* tailPtr = graph.FindNode(Dia::Core::StringCRC("tail"));

	TestEdge edge(Dia::Core::StringCRC("myEdge"), 3.14f, headPtr, tailPtr);

	graph.AddEdge(edge);

	TestEdge* found = graph.FindEdge(Dia::Core::StringCRC("myEdge"));

	ASSERT_NE(found, nullptr);
	EXPECT_FLOAT_EQ(found->GetPayloadConst(), 3.14f);
	EXPECT_EQ(found->GetUniqueID(), Dia::Core::StringCRC("myEdge"));
}

TEST(Graph, FindEdge_UnknownName_ReturnsNullptr)
{
	TestGraph graph;
	TestNode headNode(Dia::Core::StringCRC("head"), 10);
	TestNode tailNode(Dia::Core::StringCRC("tail"), 20);

	graph.AddNode(headNode);
	graph.AddNode(tailNode);

	TestNode* headPtr = graph.FindNode(Dia::Core::StringCRC("head"));
	TestNode* tailPtr = graph.FindNode(Dia::Core::StringCRC("tail"));

	TestEdge edge(Dia::Core::StringCRC("realEdge"), 1.0f, headPtr, tailPtr);

	graph.AddEdge(edge);

	TestEdge* found = graph.FindEdge(Dia::Core::StringCRC("ghostEdge"));

	EXPECT_EQ(found, nullptr);
}

TEST(Graph, GetNumberOfNodes_MultipleAdds_ReturnsCorrectCount)
{
	TestGraph graph;

	graph.AddNode(TestNode(Dia::Core::StringCRC("a"), 1));
	graph.AddNode(TestNode(Dia::Core::StringCRC("b"), 2));
	graph.AddNode(TestNode(Dia::Core::StringCRC("c"), 3));
	graph.AddNode(TestNode(Dia::Core::StringCRC("d"), 4));
	graph.AddNode(TestNode(Dia::Core::StringCRC("e"), 5));

	EXPECT_EQ(graph.GetNumberOfNodes(), 5u);
}

TEST(Graph, GetNumberOfEdges_MultipleAdds_ReturnsCorrectCount)
{
	TestGraph graph;
	TestNode nodeA(Dia::Core::StringCRC("a"), 1);
	TestNode nodeB(Dia::Core::StringCRC("b"), 2);

	graph.AddNode(nodeA);
	graph.AddNode(nodeB);

	TestNode* ptrA = graph.FindNode(Dia::Core::StringCRC("a"));
	TestNode* ptrB = graph.FindNode(Dia::Core::StringCRC("b"));

	TestEdge edge1(Dia::Core::StringCRC("e1"), 1.0f, ptrA, ptrB);
	TestEdge edge2(Dia::Core::StringCRC("e2"), 2.0f, ptrA, ptrB);
	TestEdge edge3(Dia::Core::StringCRC("e3"), 3.0f, ptrB, ptrA);

	graph.AddEdge(edge1);
	graph.AddEdge(edge2);
	graph.AddEdge(edge3);

	EXPECT_EQ(graph.GetNumberOfEdges(), 3u);
}

TEST(Graph, GraphNode_ConstructionWithIdAndPayload_StoresCorrectly)
{
	Dia::Core::StringCRC id("testNode");
	TestNode node(id, 123);

	EXPECT_EQ(node.GetUniqueID(), Dia::Core::StringCRC("testNode"));
	EXPECT_EQ(node.GetPayloadConst(), 123);
}

TEST(Graph, GraphNode_GetPayload_ReturnsMutableReference)
{
	TestNode node(Dia::Core::StringCRC("mutable"), 50);

	node.GetPayload() = 999;

	EXPECT_EQ(node.GetPayloadConst(), 999);
}

TEST(Graph, GraphEdge_ConstructionWithIdPayloadHeadTail_StoresCorrectly)
{
	TestNode headNode(Dia::Core::StringCRC("head"), 10);
	TestNode tailNode(Dia::Core::StringCRC("tail"), 20);

	TestEdge edge(Dia::Core::StringCRC("link"), 2.5f, &headNode, &tailNode);

	EXPECT_EQ(edge.GetUniqueID(), Dia::Core::StringCRC("link"));
	EXPECT_FLOAT_EQ(edge.GetPayloadConst(), 2.5f);
}

TEST(Graph, GraphEdge_GetHeadGetTail_ReturnCorrectNodes)
{
	TestNode headNode(Dia::Core::StringCRC("head"), 10);
	TestNode tailNode(Dia::Core::StringCRC("tail"), 20);

	TestEdge edge(Dia::Core::StringCRC("link"), 1.0f, &headNode, &tailNode);

	EXPECT_EQ(edge.GetHead(), &headNode);
	EXPECT_EQ(edge.GetTail(), &tailNode);
	EXPECT_EQ(edge.GetHead()->GetPayloadConst(), 10);
	EXPECT_EQ(edge.GetTail()->GetPayloadConst(), 20);
}

TEST(Graph, Stress_FillMaxNodes_AllFindable)
{
	TestGraph graph;
	const char* names[] = {"n0", "n1", "n2", "n3", "n4", "n5", "n6", "n7"};

	for (int i = 0; i < 8; i++)
	{
		graph.AddNode(TestNode(Dia::Core::StringCRC(names[i]), i * 10));
	}

	EXPECT_EQ(graph.GetNumberOfNodes(), 8u);

	for (int i = 0; i < 8; i++)
	{
		TestNode* found = graph.FindNode(Dia::Core::StringCRC(names[i]));
		ASSERT_NE(found, nullptr);
		EXPECT_EQ(found->GetPayloadConst(), i * 10);
	}
}

TEST(Graph, Stress_FillMaxEdges_AllFindable)
{
	TestGraph graph;
	TestNode nodeA(Dia::Core::StringCRC("src1"), 1);
	TestNode nodeB(Dia::Core::StringCRC("src2"), 2);
	TestNode nodeC(Dia::Core::StringCRC("dst"), 3);

	graph.AddNode(nodeA);
	graph.AddNode(nodeB);
	graph.AddNode(nodeC);

	TestNode* ptrA = graph.FindNode(Dia::Core::StringCRC("src1"));
	TestNode* ptrB = graph.FindNode(Dia::Core::StringCRC("src2"));
	TestNode* ptrC = graph.FindNode(Dia::Core::StringCRC("dst"));

	const char* edgeNames[] = {
		"e00", "e01", "e02", "e03", "e04", "e05", "e06", "e07",
		"e08", "e09", "e10", "e11", "e12", "e13", "e14", "e15"
	};

	// Split edges across two source nodes to stay within per-node edge limit (8)
	for (int i = 0; i < 8; i++)
	{
		TestEdge edge(Dia::Core::StringCRC(edgeNames[i]), static_cast<float>(i), ptrA, ptrC);
		graph.AddEdge(edge);
	}
	for (int i = 8; i < 16; i++)
	{
		TestEdge edge(Dia::Core::StringCRC(edgeNames[i]), static_cast<float>(i), ptrB, ptrC);
		graph.AddEdge(edge);
	}

	EXPECT_EQ(graph.GetNumberOfEdges(), 16u);

	for (int i = 0; i < 16; i++)
	{
		TestEdge* found = graph.FindEdge(Dia::Core::StringCRC(edgeNames[i]));
		ASSERT_NE(found, nullptr);
		EXPECT_FLOAT_EQ(found->GetPayloadConst(), static_cast<float>(i));
	}
}
