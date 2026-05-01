#include <gtest/gtest.h>
#include <DiaCore/Containers/LinkList/LinkListC.h>

using namespace Dia::Core::Containers;

class SameAsOperator
{
public:
	bool Equal(const int& object1, const int& object2)const
	{
		return (object1 == object2);
	}
};

class Equality
{
public:
	bool GreaterThen(const int& object1, const int& object2)const
	{
		return (object1 > object2);
	}
	bool LessThen(const int& object1, const int& object2)const
	{
		return (object1 < object2);
	}
	bool Equal(const int& object1, const int& object2)const
	{
		return (object1 == object2);
	}
};

class Eval
{
public:
	float Evaluate(const int& object1)const
	{
		return static_cast<float>(object1);
	};
};

TEST(LinkListC, AddNodeToTail_AddsNodesToEndOfList)
{
	LinkListC<int> list;

	EXPECT_EQ(list.Size(), 0);
	EXPECT_EQ(list.HighWaterMark(), 0);

	LinkListNode<int> node1(2);

	EXPECT_EQ(node1.GetPayload(), 2);
	EXPECT_EQ(node1.GetNext(), nullptr);
	EXPECT_EQ(node1.FindLast(), &node1);
	EXPECT_TRUE(node1.IsLast());

	list.AddNodeToTail(&node1);

	EXPECT_EQ(list.Size(), 1);
	EXPECT_EQ(list.HighWaterMark(), 1);

	LinkListNode<int> node2(1);

	list.AddNodeToTail(&node2);

	EXPECT_EQ(list.Size(), 2);
	EXPECT_EQ(list.HighWaterMark(), 2);

	LinkListNode<int> node3(4);
	LinkListNode<int> node4(5);

	node3.Add(&node4);

	list.AddNodeToTail(&node3);

	EXPECT_EQ(list.Size(), 4);
	EXPECT_EQ(list.HighWaterMark(), 4);

	EXPECT_EQ(list.Last(), 5);
	EXPECT_EQ(list.First(), 2);

	EXPECT_DEATH(list.AddNodeToTail(&node1), "");
}

TEST(LinkListC, AddNodeToHead_AddsNodesToStartOfList)
{
	LinkListC<int> list;

	EXPECT_EQ(list.Size(), 0);
	EXPECT_EQ(list.HighWaterMark(), 0);

	LinkListNode<int> node1;
	node1.SetPayload(2);

	EXPECT_EQ(node1.GetPayload(), 2);
	EXPECT_EQ(node1.GetNext(), nullptr);
	EXPECT_EQ(node1.FindLast(), &node1);
	EXPECT_TRUE(node1.IsLast());

	list.AddNodeToTail(&node1);

	EXPECT_EQ(list.Size(), 1);
	EXPECT_EQ(list.HighWaterMark(), 1);

	LinkListNode<int> node2(1);

	list.AddNodeToHead(&node2);

	EXPECT_EQ(list.Size(), 2);
	EXPECT_EQ(list.HighWaterMark(), 2);

	EXPECT_EQ(list.Last(), 2);
	EXPECT_EQ(list.First(), 1);

	LinkListNode<int> node3(4);
	LinkListNode<int> node4(5);

	node3.Add(&node4);

	list.AddNodeToHead(&node3);

	EXPECT_EQ(list.Size(), 4);
	EXPECT_EQ(list.HighWaterMark(), 4);

	EXPECT_EQ(list.Last(), 2);
	EXPECT_EQ(list.First(), 4);

	EXPECT_DEATH(list.AddNodeToHead(&node1), "");
}

TEST(LinkListC, InsertAsNextNodeTo_InsertsAfterSpecifiedNode)
{
	SameAsOperator sameAsOperator;

	LinkListC<int> list;

	LinkListNode<int> node1(2);
	LinkListNode<int> node2(1);
	LinkListNode<int> node3(4);

	list.AddNodeToTail(&node1);
	list.AddNodeToTail(&node2);
	list.AddNodeToTail(&node3);

	EXPECT_EQ(list.Size(), 3);
	EXPECT_EQ(list.HighWaterMark(), 3);

	LinkListNode<int> node4(5);

	list.InsertAsNextNodeTo(&node2, &node4);

	EXPECT_EQ(list.Size(), 4);
	EXPECT_EQ(list.HighWaterMark(), 4);
	EXPECT_EQ(list.NodeDepth(&node4), 2);
	EXPECT_TRUE(list.Contains(5));
	EXPECT_TRUE(list.Contains(&node4));
	EXPECT_TRUE(list.Contains(5, sameAsOperator));
	EXPECT_EQ(list.Head(), &node1);
	EXPECT_EQ(list.HeadConst(), &node1);
	EXPECT_EQ(list.Tail(), &node3);
	EXPECT_EQ(list.TailConst(), &node3);
}

TEST(LinkListC, InsertAsNextPreviousTo_InsertsBeforeSpecifiedNode)
{
	SameAsOperator sameAsOperator;

	LinkListC<int> list;

	LinkListNode<int> node1(2);
	LinkListNode<int> node2(1);
	LinkListNode<int> node3(4);

	list.AddNodeToTail(&node1);
	list.AddNodeToTail(&node2);
	list.AddNodeToTail(&node3);

	EXPECT_EQ(list.Size(), 3);
	EXPECT_EQ(list.HighWaterMark(), 3);

	LinkListNode<int> node4(5);

	list.InsertAsNextPreviousTo(&node2, &node4);

	EXPECT_EQ(list.Size(), 4);
	EXPECT_EQ(list.HighWaterMark(), 4);
	EXPECT_EQ(list.NodeDepth(&node4), 1);
	EXPECT_TRUE(list.Contains(5));
	EXPECT_TRUE(list.Contains(&node4));
	EXPECT_TRUE(list.Contains(5, sameAsOperator));
}

TEST(LinkListC, CopyConstructorAndAssignment_DuplicatesList)
{
	LinkListC<int> list1;
	LinkListNode<int> node1(2);
	LinkListNode<int> node2(1);

	list1.AddNodeToTail(&node1);
	list1.AddNodeToTail(&node2);

	LinkListC<int> list2;

	list2.AddNodeToTail(&node1);

	EXPECT_TRUE(list1 == list2);

	LinkListC<int> list3;
	LinkListNode<int> node3(2);

	list3.AddNodeToTail(&node3);

	EXPECT_TRUE(list1 != list3);

	LinkListC<int> list4(list1);

	EXPECT_TRUE(list4 == list1);

	LinkListC<int> list5;
	list5 = list1;

	EXPECT_TRUE(list5 == list1);
}

TEST(LinkListC, RemoveAll_ClearsAllNodes)
{
	LinkListC<int> list1;
	LinkListNode<int> node1(2);
	LinkListNode<int> node2(1);

	list1.AddNodeToTail(&node1);
	list1.AddNodeToTail(&node2);

	list1.RemoveAll();

	EXPECT_EQ(list1.Size(), 0);
	EXPECT_EQ(list1.HighWaterMark(), 2);
}

TEST(LinkListC, RemoveTailAndHead_RemovesEndNodes)
{
	LinkListC<int> list1;
	LinkListNode<int> node1(2);
	LinkListNode<int> node2(1);
	LinkListNode<int> node3(5);

	list1.AddNodeToTail(&node1);
	list1.AddNodeToTail(&node2);
	list1.AddNodeToTail(&node3);

	EXPECT_EQ(list1.Last(), 5);

	list1.RemoveTail();

	EXPECT_EQ(list1.Last(), 1);
	EXPECT_EQ(list1.Size(), 2);
	EXPECT_EQ(list1.HighWaterMark(), 3);

	list1.RemoveHead();

	EXPECT_EQ(list1.Last(), 1);
	EXPECT_EQ(list1.Size(), 1);
	EXPECT_EQ(list1.HighWaterMark(), 3);
}

TEST(LinkListC, Remove_RemovesSpecificNode)
{
	LinkListC<int> list1;
	LinkListNode<int> node1(2);
	LinkListNode<int> node2(1);
	LinkListNode<int> node3(5);

	list1.AddNodeToTail(&node1);
	list1.AddNodeToTail(&node2);
	list1.AddNodeToTail(&node3);

	EXPECT_EQ(list1.Last(), 5);

	list1.Remove(&node2);

	EXPECT_EQ(list1.Last(), 5);
	EXPECT_EQ(list1.Size(), 2);
	EXPECT_EQ(list1.HighWaterMark(), 3);
	EXPECT_FALSE(list1.Contains(1));
}

TEST(LinkListC, RemoveNext_RemovesNodeAfterSpecified)
{
	LinkListC<int> list1;
	LinkListNode<int> node1(2);
	LinkListNode<int> node2(1);
	LinkListNode<int> node3(5);

	list1.AddNodeToTail(&node1);
	list1.AddNodeToTail(&node2);
	list1.AddNodeToTail(&node3);

	EXPECT_EQ(list1.Last(), 5);

	list1.RemoveNext(&node1);

	EXPECT_EQ(list1.Last(), 5);
	EXPECT_EQ(list1.Size(), 2);
	EXPECT_EQ(list1.HighWaterMark(), 3);
	EXPECT_FALSE(list1.Contains(1));
}

TEST(LinkListC, Find_ReturnsNodeWithMatchingPayload)
{
	SameAsOperator sameAsOperator;

	LinkListC<int> list1;
	LinkListNode<int> node1(2);
	LinkListNode<int> node2(1);
	LinkListNode<int> node3(5);

	list1.AddNodeToTail(&node1);
	list1.AddNodeToTail(&node2);
	list1.AddNodeToTail(&node3);

	EXPECT_EQ(list1.Find(1), &node2);
	EXPECT_EQ(list1.FindConst(1), &node2);

	EXPECT_EQ(list1.Find(1, sameAsOperator), &node2);
	EXPECT_EQ(list1.FindConst(1, sameAsOperator), &node2);
}

TEST(LinkListC, SwapNodes_SwapsNodePositions)
{
	LinkListC<int> list1;
	LinkListNode<int> node1(2);
	LinkListNode<int> node2(1);
	LinkListNode<int> node3(5);
	LinkListNode<int> node4(7);

	list1.AddNodeToTail(&node1);
	list1.AddNodeToTail(&node2);
	list1.AddNodeToTail(&node3);

	EXPECT_EQ(list1.Last(), 5);
	EXPECT_EQ(list1.First(), 2);

	list1.SwapNodes(&node1, &node3);

	EXPECT_EQ(list1.Last(), 2);
	EXPECT_EQ(list1.First(), 5);

	EXPECT_DEATH(list1.SwapNodes(nullptr, nullptr), "");
	EXPECT_DEATH(list1.SwapNodes(&node4, &node3), "");
}

TEST(LinkListC, MoveNodeForward_MovesNodeTowardHead)
{
	LinkListC<int> list1;
	LinkListNode<int> node1(2);
	LinkListNode<int> node2(1);
	LinkListNode<int> node3(5);
	LinkListNode<int> node4(7);

	list1.AddNodeToTail(&node1);
	list1.AddNodeToTail(&node2);
	list1.AddNodeToTail(&node3);

	EXPECT_EQ(list1.Last(), 5);
	EXPECT_EQ(list1.First(), 2);

	list1.MoveNodeForward(&node1);

	EXPECT_EQ(list1.Last(), 5);
	EXPECT_EQ(list1.First(), 1);

	EXPECT_DEATH(list1.MoveNodeForward(nullptr), "");
	EXPECT_DEATH(list1.MoveNodeForward(&node4), "");
	EXPECT_DEATH(list1.MoveNodeForward(&node3), "");
}

TEST(LinkListC, MoveNodeBackwards_MovesNodeTowardTail)
{
	LinkListC<int> list1;
	LinkListNode<int> node1(2);
	LinkListNode<int> node2(1);
	LinkListNode<int> node3(5);
	LinkListNode<int> node4(7);

	list1.AddNodeToTail(&node1);
	list1.AddNodeToTail(&node2);
	list1.AddNodeToTail(&node3);

	EXPECT_EQ(list1.Last(), 5);
	EXPECT_EQ(list1.First(), 2);

	list1.MoveNodeBackwards(&node3);

	EXPECT_EQ(list1.Last(), 1);
	EXPECT_EQ(list1.First(), 2);

	EXPECT_DEATH(list1.MoveNodeBackwards(nullptr), "");
	EXPECT_DEATH(list1.MoveNodeBackwards(&node4), "");
	EXPECT_DEATH(list1.MoveNodeBackwards(&node1), "");
}

TEST(LinkListC, Sort_SortsNodesInAscendingOrder)
{
	Equality equality;

	LinkListC<int> list1;
	LinkListNode<int> node1(2);
	LinkListNode<int> node2(1);
	LinkListNode<int> node3(5);

	list1.AddNodeToTail(&node1);
	list1.AddNodeToTail(&node2);
	list1.AddNodeToTail(&node3);

	EXPECT_FALSE(list1.IsSorted());
	EXPECT_FALSE(list1.IsSorted(equality));

	list1.Sort();

	EXPECT_TRUE(list1.IsSorted());
	EXPECT_EQ(list1.Size(), 3);
	EXPECT_EQ(list1.First(), 1);
	EXPECT_EQ(list1.Last(), 5);

	LinkListC<int> list2;
	LinkListNode<int> node4(2);
	LinkListNode<int> node5(1);
	LinkListNode<int> node6(5);

	list2.AddNodeToTail(&node4);
	list2.AddNodeToTail(&node5);
	list2.AddNodeToTail(&node6);

	list2.Sort(equality);

	EXPECT_TRUE(list2.IsSorted());
	EXPECT_EQ(list2.Size(), 3);
	EXPECT_EQ(list2.First(), 1);
	EXPECT_EQ(list2.Last(), 5);
}

TEST(LinkListC, HighestEvaluation_ReturnsNodeWithHighestValue)
{
	Eval eval;

	LinkListC<int> list1;
	LinkListNode<int> node1(2);
	LinkListNode<int> node2(1);
	LinkListNode<int> node3(5);
	LinkListNode<int> node4(7);
	LinkListNode<int> node5(18);
	LinkListNode<int> node6(9);
	LinkListNode<int> node7(0);

	list1.AddNodeToTail(&node1);
	list1.AddNodeToTail(&node2);
	list1.AddNodeToTail(&node3);
	list1.AddNodeToTail(&node4);
	list1.AddNodeToTail(&node5);
	list1.AddNodeToTail(&node6);
	list1.AddNodeToTail(&node7);

	EXPECT_EQ(list1.HighestEvalution(eval)->GetPayload(), 18);
	EXPECT_EQ(list1.HighestEvalutionConst(eval)->GetPayloadConst(), 18);
}
