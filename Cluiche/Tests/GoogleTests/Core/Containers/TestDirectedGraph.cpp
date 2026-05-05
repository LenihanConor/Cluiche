#include <gtest/gtest.h>
#include <DiaCore/Containers/Graphs/DirectedGraph.h>
#include <DiaCore/CRC/StringCRC.h>

using namespace Dia::Core::Containers;
using namespace Dia::Core;

// ============================================================
// Common type aliases shared across all suites
// ============================================================
typedef DirectedGraph<int, 8, float, 16>                                       TestGraph;
typedef DirectedGraph<int, 8, float, 16, GraphPolicy::ReverseEdgeCache>        TestGraphREC;
typedef DirectedGraph<int, 8, float, 16, GraphPolicy::AcyclicEnforced>         TestGraphAE;

typedef TestGraph::Node  TestNode;
typedef TestGraph::Edge  TestEdge;

// ============================================================
// Helpers
// ============================================================
namespace
{
	// Build a linear chain: a -> b -> c
	void BuildChain(TestGraph& g)
	{
		g.AddNode(StringCRC("a"), 1);
		g.AddNode(StringCRC("b"), 2);
		g.AddNode(StringCRC("c"), 3);
		g.AddEdge(StringCRC("ab"), StringCRC("a"), StringCRC("b"), 1.0f);
		g.AddEdge(StringCRC("bc"), StringCRC("b"), StringCRC("c"), 2.0f);
	}
}

// ============================================================
// Suite 1 — DirectedGraphNodeTest
// ============================================================

TEST(DirectedGraphNodeTest, DefaultConstruct_GetUniqueID_ReturnsDefaultCRC)
{
	TestNode node;
	EXPECT_EQ(node.GetUniqueID(), StringCRC());
}

TEST(DirectedGraphNodeTest, ConstructWithIdAndPayload_GetUniqueID_Correct)
{
	TestNode node(StringCRC("myNode"), 42);
	EXPECT_EQ(node.GetUniqueID(), StringCRC("myNode"));
}

TEST(DirectedGraphNodeTest, ConstructWithPayload_GetPayload_Correct)
{
	TestNode node(StringCRC("n"), 99);
	EXPECT_EQ(node.GetPayloadConst(), 99);
}

TEST(DirectedGraphNodeTest, GetPayload_MutableReference_CanModify)
{
	TestNode node(StringCRC("n"), 10);
	node.GetPayload() = 77;
	EXPECT_EQ(node.GetPayloadConst(), 77);
}

TEST(DirectedGraphNodeTest, AddOutEdge_OutEdgeListGrows)
{
	TestNode from(StringCRC("from"), 0);
	TestNode to(StringCRC("to"), 0);
	TestEdge edge(StringCRC("e"), 1.0f, &from, &to);
	from.AddOutEdge(&edge);
	EXPECT_EQ(from.GetOutEdgeList().Size(), 1u);
}

// ============================================================
// Suite 2 — DirectedGraphEdgeTest
// ============================================================

TEST(DirectedGraphEdgeTest, DefaultConstruct_GetFrom_ReturnsNull)
{
	TestEdge e;
	EXPECT_EQ(e.GetFrom(), nullptr);
}

TEST(DirectedGraphEdgeTest, ConstructWithAllParams_GetUniqueID_Correct)
{
	TestNode from(StringCRC("f"), 0);
	TestNode to(StringCRC("t"), 0);
	TestEdge e(StringCRC("myEdge"), 3.14f, &from, &to);
	EXPECT_EQ(e.GetUniqueID(), StringCRC("myEdge"));
}

TEST(DirectedGraphEdgeTest, Construct_GetFrom_ReturnsFromNode)
{
	TestNode from(StringCRC("f"), 1);
	TestNode to(StringCRC("t"), 2);
	TestEdge e(StringCRC("e"), 0.0f, &from, &to);
	EXPECT_EQ(e.GetFrom(), &from);
}

TEST(DirectedGraphEdgeTest, Construct_GetTo_ReturnsToNode)
{
	TestNode from(StringCRC("f"), 1);
	TestNode to(StringCRC("t"), 2);
	TestEdge e(StringCRC("e"), 0.0f, &from, &to);
	EXPECT_EQ(e.GetTo(), &to);
}

TEST(DirectedGraphEdgeTest, Construct_GetPayload_Correct)
{
	TestNode from(StringCRC("f"), 0);
	TestNode to(StringCRC("t"), 0);
	TestEdge e(StringCRC("e"), 9.5f, &from, &to);
	EXPECT_FLOAT_EQ(e.GetPayloadConst(), 9.5f);
}

// ============================================================
// Suite 3 — DirectedGraphCoreTest
// ============================================================

TEST(DirectedGraphCoreTest, DefaultConstruct_ZeroNodesAndEdges)
{
	TestGraph g;
	EXPECT_EQ(g.GetNumberOfNodes(), 0u);
	EXPECT_EQ(g.GetNumberOfEdges(), 0u);
}

TEST(DirectedGraphCoreTest, AddNode_Single_IncrementsCount)
{
	TestGraph g;
	EXPECT_TRUE(g.AddNode(StringCRC("n"), 1));
	EXPECT_EQ(g.GetNumberOfNodes(), 1u);
}

#ifdef DEBUG
TEST(DirectedGraphCoreTest, AddNode_DuplicateID_Asserts)
{
	TestGraph g;
	g.AddNode(StringCRC("dup"), 1);
	EXPECT_DEATH(g.AddNode(StringCRC("dup"), 2), "");
}
#endif

TEST(DirectedGraphCoreTest, AddEdge_Valid_IncrementsCount)
{
	TestGraph g;
	g.AddNode(StringCRC("a"), 1);
	g.AddNode(StringCRC("b"), 2);
	EXPECT_TRUE(g.AddEdge(StringCRC("e"), StringCRC("a"), StringCRC("b"), 1.0f));
	EXPECT_EQ(g.GetNumberOfEdges(), 1u);
}

#ifdef DEBUG
TEST(DirectedGraphCoreTest, AddEdge_MissingFromNode_Asserts)
{
	TestGraph g;
	g.AddNode(StringCRC("b"), 2);
	EXPECT_DEATH(g.AddEdge(StringCRC("e"), StringCRC("missing"), StringCRC("b"), 1.0f), "");
}

TEST(DirectedGraphCoreTest, AddEdge_MissingToNode_Asserts)
{
	TestGraph g;
	g.AddNode(StringCRC("a"), 1);
	EXPECT_DEATH(g.AddEdge(StringCRC("e"), StringCRC("a"), StringCRC("missing"), 1.0f), "");
}
#endif

#ifdef DEBUG
TEST(DirectedGraphCoreTest, AddEdge_DuplicateID_Asserts)
{
	TestGraph g;
	g.AddNode(StringCRC("a"), 1);
	g.AddNode(StringCRC("b"), 2);
	g.AddEdge(StringCRC("dup"), StringCRC("a"), StringCRC("b"), 1.0f);
	EXPECT_DEATH(g.AddEdge(StringCRC("dup"), StringCRC("a"), StringCRC("b"), 2.0f), "");
}
#endif

TEST(DirectedGraphCoreTest, FindNode_ExistingID_ReturnsPointer)
{
	TestGraph g;
	g.AddNode(StringCRC("n"), 55);
	TestNode* n = g.FindNode(StringCRC("n"));
	ASSERT_NE(n, nullptr);
	EXPECT_EQ(n->GetPayloadConst(), 55);
}

TEST(DirectedGraphCoreTest, FindNode_MissingID_ReturnsNull)
{
	TestGraph g;
	EXPECT_EQ(g.FindNode(StringCRC("ghost")), nullptr);
}

TEST(DirectedGraphCoreTest, FindNodeConst_ExistingID_ReturnsPointer)
{
	TestGraph g;
	g.AddNode(StringCRC("n"), 7);
	const TestGraph& cg = g;
	const TestNode* n = cg.FindNode(StringCRC("n"));
	ASSERT_NE(n, nullptr);
	EXPECT_EQ(n->GetPayloadConst(), 7);
}

TEST(DirectedGraphCoreTest, FindEdge_ExistingID_ReturnsPointer)
{
	TestGraph g;
	g.AddNode(StringCRC("a"), 1);
	g.AddNode(StringCRC("b"), 2);
	g.AddEdge(StringCRC("e"), StringCRC("a"), StringCRC("b"), 5.5f);
	TestEdge* e = g.FindEdge(StringCRC("e"));
	ASSERT_NE(e, nullptr);
	EXPECT_FLOAT_EQ(e->GetPayloadConst(), 5.5f);
}

TEST(DirectedGraphCoreTest, FindEdge_MissingID_ReturnsNull)
{
	TestGraph g;
	EXPECT_EQ(g.FindEdge(StringCRC("ghost")), nullptr);
}

TEST(DirectedGraphCoreTest, FindEdgeConst_ExistingID_ReturnsPointer)
{
	TestGraph g;
	g.AddNode(StringCRC("a"), 1);
	g.AddNode(StringCRC("b"), 2);
	g.AddEdge(StringCRC("e"), StringCRC("a"), StringCRC("b"), 2.5f);
	const TestGraph& cg = g;
	const TestEdge* e = cg.FindEdge(StringCRC("e"));
	ASSERT_NE(e, nullptr);
	EXPECT_FLOAT_EQ(e->GetPayloadConst(), 2.5f);
}

TEST(DirectedGraphCoreTest, GetNumberOfNodes_MultipleAdds_ReturnsCorrectCount)
{
	TestGraph g;
	g.AddNode(StringCRC("a"), 1);
	g.AddNode(StringCRC("b"), 2);
	g.AddNode(StringCRC("c"), 3);
	EXPECT_EQ(g.GetNumberOfNodes(), 3u);
}

TEST(DirectedGraphCoreTest, GetNumberOfEdges_MultipleAdds_ReturnsCorrectCount)
{
	TestGraph g;
	g.AddNode(StringCRC("a"), 1);
	g.AddNode(StringCRC("b"), 2);
	g.AddNode(StringCRC("c"), 3);
	g.AddEdge(StringCRC("ab"), StringCRC("a"), StringCRC("b"), 1.0f);
	g.AddEdge(StringCRC("bc"), StringCRC("b"), StringCRC("c"), 2.0f);
	g.AddEdge(StringCRC("ac"), StringCRC("a"), StringCRC("c"), 3.0f);
	EXPECT_EQ(g.GetNumberOfEdges(), 3u);
}

#ifdef DEBUG
TEST(DirectedGraphCoreTest, CapacityLimit_NodesFull_Asserts)
{
	typedef DirectedGraph<int, 2, float, 4> TinyGraph;
	TinyGraph g;
	g.AddNode(StringCRC("n1"), 1);
	g.AddNode(StringCRC("n2"), 2);
	EXPECT_DEATH(g.AddNode(StringCRC("n3"), 3), "");
}

TEST(DirectedGraphCoreTest, CapacityLimit_EdgesFull_Asserts)
{
	typedef DirectedGraph<int, 4, float, 2> TinyGraph;
	TinyGraph g;
	g.AddNode(StringCRC("a"), 1);
	g.AddNode(StringCRC("b"), 2);
	g.AddNode(StringCRC("c"), 3);
	g.AddEdge(StringCRC("e1"), StringCRC("a"), StringCRC("b"), 1.0f);
	g.AddEdge(StringCRC("e2"), StringCRC("b"), StringCRC("c"), 2.0f);
	EXPECT_DEATH(g.AddEdge(StringCRC("e3"), StringCRC("a"), StringCRC("c"), 3.0f), "");
}
#endif

// ============================================================
// Suite 4 — DirectedGraphOutEdgesTest
// ============================================================

TEST(DirectedGraphOutEdgesTest, NoEdges_GetOutEdges_ReturnsEmpty)
{
	TestGraph g;
	g.AddNode(StringCRC("a"), 1);
	TestGraph::EdgeResults results;
	g.GetOutEdges(StringCRC("a"), results);
	EXPECT_EQ(results.Size(), 0u);
}

TEST(DirectedGraphOutEdgesTest, OneEdge_GetOutEdges_ReturnsIt)
{
	TestGraph g;
	g.AddNode(StringCRC("a"), 1);
	g.AddNode(StringCRC("b"), 2);
	g.AddEdge(StringCRC("e"), StringCRC("a"), StringCRC("b"), 1.0f);
	TestGraph::EdgeResults results;
	g.GetOutEdges(StringCRC("a"), results);
	ASSERT_EQ(results.Size(), 1u);
	EXPECT_EQ(results.At(0)->GetUniqueID(), StringCRC("e"));
}

TEST(DirectedGraphOutEdgesTest, MultipleEdges_GetOutEdges_ReturnsAll)
{
	TestGraph g;
	g.AddNode(StringCRC("a"), 1);
	g.AddNode(StringCRC("b"), 2);
	g.AddNode(StringCRC("c"), 3);
	g.AddEdge(StringCRC("ab"), StringCRC("a"), StringCRC("b"), 1.0f);
	g.AddEdge(StringCRC("ac"), StringCRC("a"), StringCRC("c"), 2.0f);
	TestGraph::EdgeResults results;
	g.GetOutEdges(StringCRC("a"), results);
	EXPECT_EQ(results.Size(), 2u);
}

TEST(DirectedGraphOutEdgesTest, WrongNodeID_GetOutEdges_ReturnsEmpty)
{
	TestGraph g;
	g.AddNode(StringCRC("a"), 1);
	TestGraph::EdgeResults results;
	g.GetOutEdges(StringCRC("ghost"), results);
	EXPECT_EQ(results.Size(), 0u);
}

TEST(DirectedGraphOutEdgesTest, EdgePointer_HasCorrectFromAndTo)
{
	TestGraph g;
	g.AddNode(StringCRC("src"), 10);
	g.AddNode(StringCRC("dst"), 20);
	g.AddEdge(StringCRC("e"), StringCRC("src"), StringCRC("dst"), 7.0f);
	TestGraph::EdgeResults results;
	g.GetOutEdges(StringCRC("src"), results);
	ASSERT_EQ(results.Size(), 1u);
	const TestEdge* e = results.At(0);
	ASSERT_NE(e, nullptr);
	EXPECT_EQ(e->GetFrom()->GetUniqueID(), StringCRC("src"));
	EXPECT_EQ(e->GetTo()->GetUniqueID(), StringCRC("dst"));
	EXPECT_FLOAT_EQ(e->GetPayloadConst(), 7.0f);
}

// ============================================================
// Suite 5 — DirectedGraphInEdgesTest  (Policy::None)
// ============================================================

TEST(DirectedGraphInEdgesTest, NoInEdges_GetInEdges_ReturnsEmpty)
{
	TestGraph g;
	g.AddNode(StringCRC("a"), 1);
	TestGraph::EdgeResults results;
	g.GetInEdges(StringCRC("a"), results);
	EXPECT_EQ(results.Size(), 0u);
}

TEST(DirectedGraphInEdgesTest, OneInEdge_GetInEdges_ReturnsIt)
{
	TestGraph g;
	g.AddNode(StringCRC("a"), 1);
	g.AddNode(StringCRC("b"), 2);
	g.AddEdge(StringCRC("ab"), StringCRC("a"), StringCRC("b"), 1.0f);
	TestGraph::EdgeResults results;
	g.GetInEdges(StringCRC("b"), results);
	ASSERT_EQ(results.Size(), 1u);
	EXPECT_EQ(results.At(0)->GetUniqueID(), StringCRC("ab"));
}

TEST(DirectedGraphInEdgesTest, MultipleInEdges_GetInEdges_ReturnsAll)
{
	TestGraph g;
	g.AddNode(StringCRC("a"), 1);
	g.AddNode(StringCRC("b"), 2);
	g.AddNode(StringCRC("c"), 3);
	g.AddEdge(StringCRC("ab"), StringCRC("a"), StringCRC("c"), 1.0f);
	g.AddEdge(StringCRC("bc"), StringCRC("b"), StringCRC("c"), 2.0f);
	TestGraph::EdgeResults results;
	g.GetInEdges(StringCRC("c"), results);
	EXPECT_EQ(results.Size(), 2u);
}

TEST(DirectedGraphInEdgesTest, NodeNotInGraph_GetInEdges_ReturnsEmpty)
{
	TestGraph g;
	g.AddNode(StringCRC("a"), 1);
	TestGraph::EdgeResults results;
	g.GetInEdges(StringCRC("ghost"), results);
	EXPECT_EQ(results.Size(), 0u);
}

TEST(DirectedGraphInEdgesTest, EdgePointer_HasCorrectFromAndTo)
{
	TestGraph g;
	g.AddNode(StringCRC("src"), 10);
	g.AddNode(StringCRC("dst"), 20);
	g.AddEdge(StringCRC("e"), StringCRC("src"), StringCRC("dst"), 3.5f);
	TestGraph::EdgeResults results;
	g.GetInEdges(StringCRC("dst"), results);
	ASSERT_EQ(results.Size(), 1u);
	const TestEdge* e = results.At(0);
	ASSERT_NE(e, nullptr);
	EXPECT_EQ(e->GetFrom()->GetUniqueID(), StringCRC("src"));
	EXPECT_EQ(e->GetTo()->GetUniqueID(), StringCRC("dst"));
	EXPECT_FLOAT_EQ(e->GetPayloadConst(), 3.5f);
}

// ============================================================
// Suite 6 — DirectedGraphBFSTest
// ============================================================

TEST(DirectedGraphBFSTest, SingleNode_VisitorCalledOnce)
{
	TestGraph g;
	g.AddNode(StringCRC("a"), 1);
	DynamicArrayC<const TestNode*, 8> buf;
	int count = 0;
	g.BFS(StringCRC("a"), [&](const TestNode&) { count++; }, buf);
	EXPECT_EQ(count, 1);
}

TEST(DirectedGraphBFSTest, LinearChain_VisitsAllInOrder)
{
	TestGraph g;
	BuildChain(g);
	DynamicArrayC<const TestNode*, 8> buf;
	DynamicArrayC<int, 8> order;
	g.BFS(StringCRC("a"), [&](const TestNode& n) { order.Add(n.GetPayloadConst()); }, buf);
	ASSERT_EQ(order.Size(), 3u);
	EXPECT_EQ(order.At(0), 1); // a
	EXPECT_EQ(order.At(1), 2); // b
	EXPECT_EQ(order.At(2), 3); // c
}

TEST(DirectedGraphBFSTest, BranchingGraph_VisitsAllNodes)
{
	TestGraph g;
	g.AddNode(StringCRC("root"), 0);
	g.AddNode(StringCRC("l"), 1);
	g.AddNode(StringCRC("r"), 2);
	g.AddEdge(StringCRC("rl"), StringCRC("root"), StringCRC("l"), 0.0f);
	g.AddEdge(StringCRC("rr"), StringCRC("root"), StringCRC("r"), 0.0f);
	DynamicArrayC<const TestNode*, 8> buf;
	int count = 0;
	g.BFS(StringCRC("root"), [&](const TestNode&) { count++; }, buf);
	EXPECT_EQ(count, 3);
}

TEST(DirectedGraphBFSTest, DisconnectedGraph_UnreachableNotVisited)
{
	TestGraph g;
	g.AddNode(StringCRC("a"), 1);
	g.AddNode(StringCRC("b"), 2);
	g.AddNode(StringCRC("isolated"), 99);
	g.AddEdge(StringCRC("ab"), StringCRC("a"), StringCRC("b"), 0.0f);
	DynamicArrayC<const TestNode*, 8> buf;
	int count = 0;
	g.BFS(StringCRC("a"), [&](const TestNode&) { count++; }, buf);
	EXPECT_EQ(count, 2); // isolated not reached
}

TEST(DirectedGraphBFSTest, MissingStart_VisitorNotCalled)
{
	TestGraph g;
	g.AddNode(StringCRC("a"), 1);
	DynamicArrayC<const TestNode*, 8> buf;
	int count = 0;
	g.BFS(StringCRC("ghost"), [&](const TestNode&) { count++; }, buf);
	EXPECT_EQ(count, 0);
}

TEST(DirectedGraphBFSTest, BranchingGraph_VisitorOrderBreadthFirst)
{
	// root -> l, root -> r, l -> ll
	// BFS order from root: root, l, r, ll  (level by level)
	TestGraph g;
	g.AddNode(StringCRC("root"), 0);
	g.AddNode(StringCRC("l"), 1);
	g.AddNode(StringCRC("r"), 2);
	g.AddNode(StringCRC("ll"), 3);
	g.AddEdge(StringCRC("rl"),  StringCRC("root"), StringCRC("l"),  0.0f);
	g.AddEdge(StringCRC("rr"),  StringCRC("root"), StringCRC("r"),  0.0f);
	g.AddEdge(StringCRC("lll"), StringCRC("l"),    StringCRC("ll"), 0.0f);
	DynamicArrayC<const TestNode*, 8> buf;
	DynamicArrayC<int, 8> order;
	g.BFS(StringCRC("root"), [&](const TestNode& n) { order.Add(n.GetPayloadConst()); }, buf);
	// root (0) must be first; ll (3) must be after both l (1) and r (2)
	ASSERT_EQ(order.Size(), 4u);
	EXPECT_EQ(order.At(0), 0); // root
	unsigned int llPos = 99, lPos = 99, rPos = 99;
	for (unsigned int i = 0; i < order.Size(); i++)
	{
		if (order.At(i) == 1) lPos = i;
		if (order.At(i) == 2) rPos = i;
		if (order.At(i) == 3) llPos = i;
	}
	EXPECT_LT(lPos,  llPos); // l before ll
	EXPECT_LT(rPos,  llPos); // r before ll (breadth-first)
}

TEST(DirectedGraphBFSTest, CyclicGraph_EachNodeVisitedAtMostOnce)
{
	// a -> b -> a  (cycle) plus b -> c
	TestGraph g;
	g.AddNode(StringCRC("a"), 1);
	g.AddNode(StringCRC("b"), 2);
	g.AddNode(StringCRC("c"), 3);
	g.AddEdge(StringCRC("ab"), StringCRC("a"), StringCRC("b"), 0.0f);
	g.AddEdge(StringCRC("ba"), StringCRC("b"), StringCRC("a"), 0.0f);
	g.AddEdge(StringCRC("bc"), StringCRC("b"), StringCRC("c"), 0.0f);
	DynamicArrayC<const TestNode*, 8> buf;
	int count = 0;
	g.BFS(StringCRC("a"), [&](const TestNode&) { count++; }, buf);
	EXPECT_EQ(count, 3); // a, b, c — each visited once despite cycle
}

// ============================================================
// Suite 7 — DirectedGraphDFSTest
// ============================================================

TEST(DirectedGraphDFSTest, SingleNode_VisitorCalledOnce)
{
	TestGraph g;
	g.AddNode(StringCRC("a"), 1);
	DynamicArrayC<const TestNode*, 8> buf;
	int count = 0;
	g.DFS(StringCRC("a"), [&](const TestNode&) { count++; }, buf);
	EXPECT_EQ(count, 1);
}

TEST(DirectedGraphDFSTest, LinearChain_VisitsAllNodes)
{
	TestGraph g;
	BuildChain(g);
	DynamicArrayC<const TestNode*, 8> buf;
	int count = 0;
	g.DFS(StringCRC("a"), [&](const TestNode&) { count++; }, buf);
	EXPECT_EQ(count, 3);
}

TEST(DirectedGraphDFSTest, BranchingGraph_VisitsAllNodes)
{
	TestGraph g;
	g.AddNode(StringCRC("root"), 0);
	g.AddNode(StringCRC("l"), 1);
	g.AddNode(StringCRC("r"), 2);
	g.AddEdge(StringCRC("rl"), StringCRC("root"), StringCRC("l"), 0.0f);
	g.AddEdge(StringCRC("rr"), StringCRC("root"), StringCRC("r"), 0.0f);
	DynamicArrayC<const TestNode*, 8> buf;
	int count = 0;
	g.DFS(StringCRC("root"), [&](const TestNode&) { count++; }, buf);
	EXPECT_EQ(count, 3);
}

TEST(DirectedGraphDFSTest, DisconnectedGraph_UnreachableNotVisited)
{
	TestGraph g;
	g.AddNode(StringCRC("a"), 1);
	g.AddNode(StringCRC("b"), 2);
	g.AddNode(StringCRC("isolated"), 99);
	g.AddEdge(StringCRC("ab"), StringCRC("a"), StringCRC("b"), 0.0f);
	DynamicArrayC<const TestNode*, 8> buf;
	int count = 0;
	g.DFS(StringCRC("a"), [&](const TestNode&) { count++; }, buf);
	EXPECT_EQ(count, 2);
}

TEST(DirectedGraphDFSTest, MissingStart_VisitorNotCalled)
{
	TestGraph g;
	DynamicArrayC<const TestNode*, 8> buf;
	int count = 0;
	g.DFS(StringCRC("ghost"), [&](const TestNode&) { count++; }, buf);
	EXPECT_EQ(count, 0);
}

TEST(DirectedGraphDFSTest, BranchingGraph_VisitorOrderDepthFirst)
{
	// root -> l -> ll (deep left)
	//      -> r       (shallow right)
	// DFS should go deep before wide: root, l, ll, r (or root, r, root, l, ll depending on push order)
	// Our impl pushes in reverse order so first out-edge is visited first: root, l, ll, r
	TestGraph g;
	g.AddNode(StringCRC("root"), 0);
	g.AddNode(StringCRC("l"), 1);
	g.AddNode(StringCRC("r"), 2);
	g.AddNode(StringCRC("ll"), 3);
	g.AddEdge(StringCRC("rl"),  StringCRC("root"), StringCRC("l"),  0.0f);
	g.AddEdge(StringCRC("rr"),  StringCRC("root"), StringCRC("r"),  0.0f);
	g.AddEdge(StringCRC("lll"), StringCRC("l"),    StringCRC("ll"), 0.0f);
	DynamicArrayC<const TestNode*, 8> buf;
	DynamicArrayC<int, 8> order;
	g.DFS(StringCRC("root"), [&](const TestNode& n) { order.Add(n.GetPayloadConst()); }, buf);
	ASSERT_EQ(order.Size(), 4u);
	// root (0) is first; ll (3) must come before r (2) — DFS goes deep before wide
	EXPECT_EQ(order.At(0), 0); // root always first
	unsigned int llPos = 99, rPos = 99;
	for (unsigned int i = 0; i < order.Size(); i++)
	{
		if (order.At(i) == 3) llPos = i;
		if (order.At(i) == 2) rPos  = i;
	}
	EXPECT_LT(llPos, rPos); // depth-first: ll reached before r
}

TEST(DirectedGraphDFSTest, LinearChain_VisitorOrderDepthFirst)
{
	// a -> b -> c: DFS from a should visit a, b, c in that order
	TestGraph g;
	BuildChain(g);
	DynamicArrayC<const TestNode*, 8> buf;
	DynamicArrayC<int, 8> order;
	g.DFS(StringCRC("a"), [&](const TestNode& n) { order.Add(n.GetPayloadConst()); }, buf);
	ASSERT_EQ(order.Size(), 3u);
	EXPECT_EQ(order.At(0), 1); // a
	EXPECT_EQ(order.At(1), 2); // b
	EXPECT_EQ(order.At(2), 3); // c
}

TEST(DirectedGraphDFSTest, CyclicGraph_EachNodeVisitedAtMostOnce)
{
	// a -> b -> a (cycle) plus b -> c
	TestGraph g;
	g.AddNode(StringCRC("a"), 1);
	g.AddNode(StringCRC("b"), 2);
	g.AddNode(StringCRC("c"), 3);
	g.AddEdge(StringCRC("ab"), StringCRC("a"), StringCRC("b"), 0.0f);
	g.AddEdge(StringCRC("ba"), StringCRC("b"), StringCRC("a"), 0.0f);
	g.AddEdge(StringCRC("bc"), StringCRC("b"), StringCRC("c"), 0.0f);
	DynamicArrayC<const TestNode*, 8> buf;
	int count = 0;
	g.DFS(StringCRC("a"), [&](const TestNode&) { count++; }, buf);
	EXPECT_EQ(count, 3); // a, b, c — each visited once despite cycle
}

// ============================================================
// Suite 8 — DirectedGraphTopoSortTest
// ============================================================

TEST(DirectedGraphTopoSortTest, EmptyGraph_ReturnsTrue_EmptyResults)
{
	TestGraph g;
	TestGraph::NodeResults results;
	DynamicArrayC<const TestNode*, 8> work;
	EXPECT_TRUE(g.TopoSort(results, work));
	EXPECT_EQ(results.Size(), 0u);
}

TEST(DirectedGraphTopoSortTest, SingleNode_ResultHasOneNode)
{
	TestGraph g;
	g.AddNode(StringCRC("a"), 1);
	TestGraph::NodeResults results;
	DynamicArrayC<const TestNode*, 8> work;
	EXPECT_TRUE(g.TopoSort(results, work));
	ASSERT_EQ(results.Size(), 1u);
	EXPECT_EQ(results.At(0)->GetUniqueID(), StringCRC("a"));
}

TEST(DirectedGraphTopoSortTest, LinearChain_OrderIsABC)
{
	TestGraph g;
	BuildChain(g);
	TestGraph::NodeResults results;
	DynamicArrayC<const TestNode*, 8> work;
	EXPECT_TRUE(g.TopoSort(results, work));
	ASSERT_EQ(results.Size(), 3u);
	EXPECT_EQ(results.At(0)->GetUniqueID(), StringCRC("a"));
	EXPECT_EQ(results.At(1)->GetUniqueID(), StringCRC("b"));
	EXPECT_EQ(results.At(2)->GetUniqueID(), StringCRC("c"));
}

TEST(DirectedGraphTopoSortTest, DiamondDAG_AllFourNodesPresent)
{
	// a -> b -> d
	// a -> c -> d
	TestGraph g;
	g.AddNode(StringCRC("a"), 1);
	g.AddNode(StringCRC("b"), 2);
	g.AddNode(StringCRC("c"), 3);
	g.AddNode(StringCRC("d"), 4);
	g.AddEdge(StringCRC("ab"), StringCRC("a"), StringCRC("b"), 0.0f);
	g.AddEdge(StringCRC("ac"), StringCRC("a"), StringCRC("c"), 0.0f);
	g.AddEdge(StringCRC("bd"), StringCRC("b"), StringCRC("d"), 0.0f);
	g.AddEdge(StringCRC("cd"), StringCRC("c"), StringCRC("d"), 0.0f);
	TestGraph::NodeResults results;
	DynamicArrayC<const TestNode*, 8> work;
	EXPECT_TRUE(g.TopoSort(results, work));
	EXPECT_EQ(results.Size(), 4u);
	// 'a' must come before 'b', 'c'; 'd' must come last
	unsigned int aPos = 99, bPos = 99, cPos = 99, dPos = 99;
	for (unsigned int i = 0; i < results.Size(); i++)
	{
		const StringCRC id = results.At(i)->GetUniqueID();
		if (id == StringCRC("a")) aPos = i;
		else if (id == StringCRC("b")) bPos = i;
		else if (id == StringCRC("c")) cPos = i;
		else if (id == StringCRC("d")) dPos = i;
	}
	EXPECT_LT(aPos, bPos);
	EXPECT_LT(aPos, cPos);
	EXPECT_LT(bPos, dPos);
	EXPECT_LT(cPos, dPos);
}

TEST(DirectedGraphTopoSortTest, CyclePresent_ReturnsFalse)
{
	TestGraph g;
	g.AddNode(StringCRC("a"), 1);
	g.AddNode(StringCRC("b"), 2);
	g.AddEdge(StringCRC("ab"), StringCRC("a"), StringCRC("b"), 0.0f);
	g.AddEdge(StringCRC("ba"), StringCRC("b"), StringCRC("a"), 0.0f); // cycle
	TestGraph::NodeResults results;
	DynamicArrayC<const TestNode*, 8> work;
	EXPECT_FALSE(g.TopoSort(results, work));
}

TEST(DirectedGraphTopoSortTest, DisconnectedDAG_AllNodesPresent)
{
	// Two independent chains: a->b and c->d
	TestGraph g;
	g.AddNode(StringCRC("a"), 1);
	g.AddNode(StringCRC("b"), 2);
	g.AddNode(StringCRC("c"), 3);
	g.AddNode(StringCRC("d"), 4);
	g.AddEdge(StringCRC("ab"), StringCRC("a"), StringCRC("b"), 0.0f);
	g.AddEdge(StringCRC("cd"), StringCRC("c"), StringCRC("d"), 0.0f);
	TestGraph::NodeResults results;
	DynamicArrayC<const TestNode*, 8> work;
	EXPECT_TRUE(g.TopoSort(results, work));
	EXPECT_EQ(results.Size(), 4u);
	// a must come before b; c must come before d
	unsigned int aPos = 99, bPos = 99, cPos = 99, dPos = 99;
	for (unsigned int i = 0; i < results.Size(); i++)
	{
		const StringCRC id = results.At(i)->GetUniqueID();
		if (id == StringCRC("a")) aPos = i;
		else if (id == StringCRC("b")) bPos = i;
		else if (id == StringCRC("c")) cPos = i;
		else if (id == StringCRC("d")) dPos = i;
	}
	EXPECT_LT(aPos, bPos);
	EXPECT_LT(cPos, dPos);
}

TEST(DirectedGraphTopoSortTest, AlreadyTopologicalOrder_PreservedOrValid)
{
	// Insert nodes in topological order; result must still be valid topo order
	TestGraph g;
	g.AddNode(StringCRC("x"), 10);
	g.AddNode(StringCRC("y"), 20);
	g.AddNode(StringCRC("z"), 30);
	g.AddEdge(StringCRC("xy"), StringCRC("x"), StringCRC("y"), 0.0f);
	g.AddEdge(StringCRC("yz"), StringCRC("y"), StringCRC("z"), 0.0f);
	TestGraph::NodeResults results;
	DynamicArrayC<const TestNode*, 8> work;
	EXPECT_TRUE(g.TopoSort(results, work));
	ASSERT_EQ(results.Size(), 3u);
	// x before y before z
	unsigned int xPos = 99, yPos = 99, zPos = 99;
	for (unsigned int i = 0; i < results.Size(); i++)
	{
		const StringCRC id = results.At(i)->GetUniqueID();
		if (id == StringCRC("x")) xPos = i;
		else if (id == StringCRC("y")) yPos = i;
		else if (id == StringCRC("z")) zPos = i;
	}
	EXPECT_LT(xPos, yPos);
	EXPECT_LT(yPos, zPos);
}

// ============================================================
// Suite 9 — DirectedGraphHasCycleTest
// ============================================================

TEST(DirectedGraphHasCycleTest, EmptyGraph_NoCycle)
{
	TestGraph g;
	EXPECT_FALSE(g.HasCycle());
}

TEST(DirectedGraphHasCycleTest, DAG_NoCycle)
{
	TestGraph g;
	BuildChain(g);
	EXPECT_FALSE(g.HasCycle());
}

TEST(DirectedGraphHasCycleTest, SelfLoop_HasCycle)
{
	TestGraph g;
	g.AddNode(StringCRC("a"), 1);
	g.AddEdge(StringCRC("aa"), StringCRC("a"), StringCRC("a"), 0.0f);
	EXPECT_TRUE(g.HasCycle());
}

TEST(DirectedGraphHasCycleTest, TwoNodeCycle_HasCycle)
{
	TestGraph g;
	g.AddNode(StringCRC("a"), 1);
	g.AddNode(StringCRC("b"), 2);
	g.AddEdge(StringCRC("ab"), StringCRC("a"), StringCRC("b"), 0.0f);
	g.AddEdge(StringCRC("ba"), StringCRC("b"), StringCRC("a"), 0.0f);
	EXPECT_TRUE(g.HasCycle());
}

TEST(DirectedGraphHasCycleTest, LongerCycle_HasCycle)
{
	TestGraph g;
	g.AddNode(StringCRC("a"), 1);
	g.AddNode(StringCRC("b"), 2);
	g.AddNode(StringCRC("c"), 3);
	g.AddEdge(StringCRC("ab"), StringCRC("a"), StringCRC("b"), 0.0f);
	g.AddEdge(StringCRC("bc"), StringCRC("b"), StringCRC("c"), 0.0f);
	g.AddEdge(StringCRC("ca"), StringCRC("c"), StringCRC("a"), 0.0f);
	EXPECT_TRUE(g.HasCycle());
}

TEST(DirectedGraphHasCycleTest, DisconnectedGraph_OneCyclicComponent_HasCycle)
{
	// Component 1: x -> y (acyclic); Component 2: a -> b -> a (cycle)
	TestGraph g;
	g.AddNode(StringCRC("x"), 1);
	g.AddNode(StringCRC("y"), 2);
	g.AddNode(StringCRC("a"), 3);
	g.AddNode(StringCRC("b"), 4);
	g.AddEdge(StringCRC("xy"), StringCRC("x"), StringCRC("y"), 0.0f);
	g.AddEdge(StringCRC("ab"), StringCRC("a"), StringCRC("b"), 0.0f);
	g.AddEdge(StringCRC("ba"), StringCRC("b"), StringCRC("a"), 0.0f);
	EXPECT_TRUE(g.HasCycle());
}

TEST(DirectedGraphHasCycleTest, DisconnectedGraph_NoCycleInAnyComponent_NoCycle)
{
	// Two independent acyclic components
	TestGraph g;
	g.AddNode(StringCRC("x"), 1);
	g.AddNode(StringCRC("y"), 2);
	g.AddNode(StringCRC("a"), 3);
	g.AddNode(StringCRC("b"), 4);
	g.AddEdge(StringCRC("xy"), StringCRC("x"), StringCRC("y"), 0.0f);
	g.AddEdge(StringCRC("ab"), StringCRC("a"), StringCRC("b"), 0.0f);
	EXPECT_FALSE(g.HasCycle());
}

// ============================================================
// Suite 10 — DirectedGraphPolicyReverseEdgeCacheTest
// ============================================================

TEST(DirectedGraphPolicyReverseEdgeCacheTest, BeforeAndAfterAddEdge_SameResults)
{
	TestGraphREC g;
	g.AddNode(StringCRC("a"), 1);
	g.AddNode(StringCRC("b"), 2);
	TestGraphREC::EdgeResults results;

	// Before any edges — b has no in-edges
	g.GetInEdges(StringCRC("b"), results);
	EXPECT_EQ(results.Size(), 0u);

	g.AddEdge(StringCRC("ab"), StringCRC("a"), StringCRC("b"), 1.0f);

	// After adding edge — cache is dirty, should rebuild automatically
	g.GetInEdges(StringCRC("b"), results);
	ASSERT_EQ(results.Size(), 1u);
	EXPECT_EQ(results.At(0)->GetUniqueID(), StringCRC("ab"));
}

TEST(DirectedGraphPolicyReverseEdgeCacheTest, MultipleMutationsThenQuery_OneRebuild)
{
	TestGraphREC g;
	g.AddNode(StringCRC("a"), 1);
	g.AddNode(StringCRC("b"), 2);
	g.AddNode(StringCRC("c"), 3);
	g.AddEdge(StringCRC("ab"), StringCRC("a"), StringCRC("b"), 1.0f);
	g.AddEdge(StringCRC("cb"), StringCRC("c"), StringCRC("b"), 2.0f);

	TestGraphREC::EdgeResults results;
	g.GetInEdges(StringCRC("b"), results);
	EXPECT_EQ(results.Size(), 2u);
}

TEST(DirectedGraphPolicyReverseEdgeCacheTest, SameResultsAsNoneScan)
{
	// Build identical graphs with None and ReverseEdgeCache policies
	TestGraph gNone;
	TestGraphREC gRec;

	for (auto* g2 : { (TestGraph*)&gNone })
	{
		g2->AddNode(StringCRC("x"), 1);
		g2->AddNode(StringCRC("y"), 2);
		g2->AddNode(StringCRC("z"), 3);
		g2->AddEdge(StringCRC("xy"), StringCRC("x"), StringCRC("y"), 1.0f);
		g2->AddEdge(StringCRC("zy"), StringCRC("z"), StringCRC("y"), 2.0f);
	}
	gRec.AddNode(StringCRC("x"), 1);
	gRec.AddNode(StringCRC("y"), 2);
	gRec.AddNode(StringCRC("z"), 3);
	gRec.AddEdge(StringCRC("xy"), StringCRC("x"), StringCRC("y"), 1.0f);
	gRec.AddEdge(StringCRC("zy"), StringCRC("z"), StringCRC("y"), 2.0f);

	TestGraph::EdgeResults   noneResults;
	TestGraphREC::EdgeResults recResults;
	gNone.GetInEdges(StringCRC("y"), noneResults);
	gRec.GetInEdges(StringCRC("y"), recResults);

	EXPECT_EQ(noneResults.Size(), recResults.Size());
}

TEST(DirectedGraphPolicyReverseEdgeCacheTest, QueryAfterAddNode_RebuildCorrect)
{
	TestGraphREC g;
	g.AddNode(StringCRC("a"), 1);
	g.AddNode(StringCRC("b"), 2);
	g.AddEdge(StringCRC("ab"), StringCRC("a"), StringCRC("b"), 1.0f);

	// Prime the cache
	TestGraphREC::EdgeResults r;
	g.GetInEdges(StringCRC("b"), r);
	EXPECT_EQ(r.Size(), 1u);

	// Mutate — add a new node (invalidates cache)
	g.AddNode(StringCRC("c"), 3);
	g.AddEdge(StringCRC("cb"), StringCRC("c"), StringCRC("b"), 2.0f);

	// Should rebuild; now two in-edges to b
	g.GetInEdges(StringCRC("b"), r);
	EXPECT_EQ(r.Size(), 2u);
}

TEST(DirectedGraphPolicyReverseEdgeCacheTest, EdgePointerContent_MatchesNoneScan)
{
	// Verify that GetInEdges via ReverseEdgeCache returns the same edge (by ID and payload)
	// as the Policy::None linear scan — not just the same count.
	TestGraph    gNone;
	TestGraphREC gRec;

	auto setup = [](auto& g)
	{
		g.AddNode(StringCRC("src"), 1);
		g.AddNode(StringCRC("dst"), 2);
		g.AddEdge(StringCRC("myEdge"), StringCRC("src"), StringCRC("dst"), 9.9f);
	};
	setup(gNone);
	setup(gRec);

	TestGraph::EdgeResults    noneRes;
	TestGraphREC::EdgeResults recRes;
	gNone.GetInEdges(StringCRC("dst"), noneRes);
	gRec.GetInEdges(StringCRC("dst"), recRes);

	ASSERT_EQ(noneRes.Size(), 1u);
	ASSERT_EQ(recRes.Size(),  1u);
	EXPECT_EQ(recRes.At(0)->GetUniqueID(),   StringCRC("myEdge"));
	EXPECT_FLOAT_EQ(recRes.At(0)->GetPayloadConst(), 9.9f);
	EXPECT_EQ(recRes.At(0)->GetFrom()->GetUniqueID(), StringCRC("src"));
	EXPECT_EQ(recRes.At(0)->GetTo()->GetUniqueID(),   StringCRC("dst"));
}

TEST(DirectedGraphPolicyReverseEdgeCacheTest, DoubleQuery_NoCacheRebuildOnSecondCall)
{
	// Prime the cache with a first query, then mutate and rebuild, then query again
	// without mutating — verifies the cache is reused (no redundant work).
	TestGraphREC g;
	g.AddNode(StringCRC("a"), 1);
	g.AddNode(StringCRC("b"), 2);
	g.AddEdge(StringCRC("ab"), StringCRC("a"), StringCRC("b"), 1.0f);

	TestGraphREC::EdgeResults r1, r2;
	g.GetInEdges(StringCRC("b"), r1); // builds cache
	g.GetInEdges(StringCRC("b"), r2); // should hit cache — same result, no crash

	EXPECT_EQ(r1.Size(), r2.Size());
	ASSERT_EQ(r2.Size(), 1u);
	EXPECT_EQ(r2.At(0)->GetUniqueID(), StringCRC("ab"));
}

// ============================================================
// Suite 11 — DirectedGraphPolicyAcyclicEnforcedTest
// ============================================================

TEST(DirectedGraphPolicyAcyclicEnforcedTest, ValidDAG_SuccessfullyBuilt)
{
	TestGraphAE g;
	g.AddNode(StringCRC("a"), 1);
	g.AddNode(StringCRC("b"), 2);
	g.AddNode(StringCRC("c"), 3);
	EXPECT_TRUE(g.AddEdge(StringCRC("ab"), StringCRC("a"), StringCRC("b"), 0.0f));
	EXPECT_TRUE(g.AddEdge(StringCRC("bc"), StringCRC("b"), StringCRC("c"), 0.0f));
	EXPECT_EQ(g.GetNumberOfEdges(), 2u);
}

#ifdef DEBUG
TEST(DirectedGraphPolicyAcyclicEnforcedTest, CycleEdge_Asserts)
{
	TestGraphAE g;
	g.AddNode(StringCRC("a"), 1);
	g.AddNode(StringCRC("b"), 2);
	g.AddEdge(StringCRC("ab"), StringCRC("a"), StringCRC("b"), 0.0f);
	EXPECT_DEATH(g.AddEdge(StringCRC("ba"), StringCRC("b"), StringCRC("a"), 0.0f), "");
}
#endif

TEST(DirectedGraphPolicyAcyclicEnforcedTest, HasCycle_AlwaysFalse)
{
	TestGraphAE g;
	g.AddNode(StringCRC("a"), 1);
	g.AddNode(StringCRC("b"), 2);
	g.AddEdge(StringCRC("ab"), StringCRC("a"), StringCRC("b"), 0.0f);
	EXPECT_FALSE(g.HasCycle());
}

TEST(DirectedGraphPolicyAcyclicEnforcedTest, TopoSort_AlwaysReturnsTrue)
{
	TestGraphAE g;
	g.AddNode(StringCRC("a"), 1);
	g.AddNode(StringCRC("b"), 2);
	g.AddEdge(StringCRC("ab"), StringCRC("a"), StringCRC("b"), 0.0f);
	TestGraphAE::NodeResults results;
	DynamicArrayC<const TestGraphAE::Node*, 8> work;
	EXPECT_TRUE(g.TopoSort(results, work));
}

#ifdef DEBUG
TEST(DirectedGraphPolicyAcyclicEnforcedTest, SelfLoop_Asserts)
{
	TestGraphAE g;
	g.AddNode(StringCRC("a"), 1);
	EXPECT_DEATH(g.AddEdge(StringCRC("aa"), StringCRC("a"), StringCRC("a"), 0.0f), "");
}

TEST(DirectedGraphPolicyAcyclicEnforcedTest, LongerCycle_Asserts)
{
	TestGraphAE g;
	g.AddNode(StringCRC("a"), 1);
	g.AddNode(StringCRC("b"), 2);
	g.AddNode(StringCRC("c"), 3);
	g.AddEdge(StringCRC("ab"), StringCRC("a"), StringCRC("b"), 0.0f);
	g.AddEdge(StringCRC("bc"), StringCRC("b"), StringCRC("c"), 0.0f);
	EXPECT_DEATH(g.AddEdge(StringCRC("ca"), StringCRC("c"), StringCRC("a"), 0.0f), "");
}
#endif
