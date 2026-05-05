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

// ============================================================
// Suite 8 — DirectedGraphTopoSortTest
// ============================================================

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
