
#include "UnitTests/Tests/Collections/UnitTestLinkListC.h"

#include <DiaCore/Containers/LinkList/LinkListC.h>

#include "UnitTests/Infrastructure/UnitTestMacros.h"

using namespace Dia::Core::Containers;

namespace UnitTests
{	
	UnitTestLinkLIstC::UnitTestLinkLIstC(const Dia::Core::Containers::String32& name)
		: UnitTestCoreContainers(name)
	{}

	UnitTestLinkLIstC::UnitTestLinkLIstC(void)
		: UnitTestCoreContainers()
	{}

	void UnitTestLinkLIstC::DoTest()
	{
		UNIT_TEST_BLOCK_START();

			LinkListC<int> list;
							
			UNIT_TEST_POSITIVE(list.Size() == 0, "LinkListC()");
			UNIT_TEST_POSITIVE(list.HighWaterMark() == 0, "LinkListC()");
	
			LinkListNode<int> node1(2);

			UNIT_TEST_POSITIVE(node1.GetPayload() == 2, "LinkListC()");
			UNIT_TEST_POSITIVE(node1.GetNext() == NULL, "LinkListC()");
			UNIT_TEST_POSITIVE(node1.FindLast() == &node1, "LinkListC()");
			UNIT_TEST_POSITIVE(node1.IsLast(), "LinkListC()");
			
			list.AddNodeToTail(&node1);
			
			UNIT_TEST_POSITIVE(list.Size() == 1, "LinkListC()");
			UNIT_TEST_POSITIVE(list.HighWaterMark() == 1, "LinkListC()");
			
			LinkListNode<int> node2(1);

			list.AddNodeToTail(&node2);

			UNIT_TEST_POSITIVE(list.Size() == 2, "LinkListC()");
			UNIT_TEST_POSITIVE(list.HighWaterMark() == 2, "LinkListC()");
			
			LinkListNode<int> node3(4);
			LinkListNode<int> node4(5);
			
			node3.Add(&node4);
						
			list.AddNodeToTail(&node3);

			UNIT_TEST_POSITIVE(list.Size() == 4, "LinkListC()");
			UNIT_TEST_POSITIVE(list.HighWaterMark() == 4, "LinkListC()");
			
			UNIT_TEST_POSITIVE(list.Last() == 5, "LinkListC()");
			UNIT_TEST_POSITIVE(list.First() == 2, "LinkListC()");

			UNIT_TEST_ASSERT_EXPECTED_START();
			list.AddNodeToTail(&node1);
			UNIT_TEST_ASSERT_EXPECTED_END();

		UNIT_TEST_BLOCK_END();

		UNIT_TEST_BLOCK_START();

			LinkListC<int> list;

			UNIT_TEST_POSITIVE(list.Size() == 0, "LinkListC()");
			UNIT_TEST_POSITIVE(list.HighWaterMark() == 0, "LinkListC()");

			LinkListNode<int> node1;
			node1.SetPayload(2);

			UNIT_TEST_POSITIVE(node1.GetPayload() == 2, "LinkListC()");
			UNIT_TEST_POSITIVE(node1.GetNext() == NULL, "LinkListC()");
			UNIT_TEST_POSITIVE(node1.FindLast() == &node1, "LinkListC()");
			UNIT_TEST_POSITIVE(node1.IsLast(), "LinkListC()");

			list.AddNodeToTail(&node1);

			UNIT_TEST_POSITIVE(list.Size() == 1, "LinkListC()");
			UNIT_TEST_POSITIVE(list.HighWaterMark() == 1, "LinkListC()");

			LinkListNode<int> node2(1);

			list.AddNodeToHead(&node2);

			UNIT_TEST_POSITIVE(list.Size() == 2, "LinkListC()");
			UNIT_TEST_POSITIVE(list.HighWaterMark() == 2, "LinkListC()");
			
			UNIT_TEST_POSITIVE(list.Last() == 2, "LinkListC()");
			UNIT_TEST_POSITIVE(list.First() == 1, "LinkListC()");

			LinkListNode<int> node3(4);
			LinkListNode<int> node4(5);

			node3.Add(&node4);

			list.AddNodeToHead(&node3);

			UNIT_TEST_POSITIVE(list.Size() == 4, "LinkListC()");
			UNIT_TEST_POSITIVE(list.HighWaterMark() == 4, "LinkListC()");

			UNIT_TEST_POSITIVE(list.Last() == 2, "LinkListC()");
			UNIT_TEST_POSITIVE(list.First() == 4, "LinkListC()");

			UNIT_TEST_ASSERT_EXPECTED_START();
			list.AddNodeToHead(&node1);
			UNIT_TEST_ASSERT_EXPECTED_END();

		UNIT_TEST_BLOCK_END();
		
		UNIT_TEST_BLOCK_START();

			class SameAsOperator
			{
			public:
				bool Equal(const int& object1, const int& object2)const
				{
					return (object1 == (object2));
				}
			};

			SameAsOperator sameAsOperator;

			LinkListC<int> list;

			LinkListNode<int> node1(2);
			LinkListNode<int> node2(1);
			LinkListNode<int> node3(4);
			
			list.AddNodeToTail(&node1);
			list.AddNodeToTail(&node2);
			list.AddNodeToTail(&node3);

			UNIT_TEST_POSITIVE(list.Size() == 3, "LinkListC()");
			UNIT_TEST_POSITIVE(list.HighWaterMark() == 3, "LinkListC()");
		
			LinkListNode<int> node4(5);

			list.InsertAsNextNodeTo(&node2, &node4);

			UNIT_TEST_POSITIVE(list.Size() == 4, "LinkListC()");
			UNIT_TEST_POSITIVE(list.HighWaterMark() == 4, "LinkListC()");
			UNIT_TEST_POSITIVE(list.NodeDepth(&node4) == 2, "LinkListC()");
			UNIT_TEST_POSITIVE(list.Contains(5), "LinkListC()");
			UNIT_TEST_POSITIVE(list.Contains(&node4), "LinkListC()");
			UNIT_TEST_POSITIVE(list.Contains(5, sameAsOperator), "LinkListC()");
			UNIT_TEST_POSITIVE(list.Head() == &node1, "LinkListC()");
			UNIT_TEST_POSITIVE(list.HeadConst() == &node1, "LinkListC()");
			UNIT_TEST_POSITIVE(list.Tail() == &node3, "LinkListC()");
			UNIT_TEST_POSITIVE(list.TailConst() == &node3, "LinkListC()");

		UNIT_TEST_BLOCK_END();

		UNIT_TEST_BLOCK_START();

			class SameAsOperator
			{
			public:
				bool Equal(const int& object1, const int& object2)const
				{
					return (object1 == (object2));
				}
			};

			SameAsOperator sameAsOperator;

			LinkListC<int> list;

			LinkListNode<int> node1(2);
			LinkListNode<int> node2(1);
			LinkListNode<int> node3(4);

			list.AddNodeToTail(&node1);
			list.AddNodeToTail(&node2);
			list.AddNodeToTail(&node3);

			UNIT_TEST_POSITIVE(list.Size() == 3, "LinkListC()");
			UNIT_TEST_POSITIVE(list.HighWaterMark() == 3, "LinkListC()");

			LinkListNode<int> node4(5);

			list.InsertAsNextPreviousTo(&node2, &node4);

			UNIT_TEST_POSITIVE(list.Size() == 4, "LinkListC()");
			UNIT_TEST_POSITIVE(list.HighWaterMark() == 4, "LinkListC()");
			UNIT_TEST_POSITIVE(list.NodeDepth(&node4) == 1, "LinkListC()");
			UNIT_TEST_POSITIVE(list.Contains(5), "LinkListC()");
			UNIT_TEST_POSITIVE(list.Contains(&node4), "LinkListC()");
			UNIT_TEST_POSITIVE(list.Contains(5, sameAsOperator), "LinkListC()");

		UNIT_TEST_BLOCK_END();

		UNIT_TEST_BLOCK_START();

			LinkListC<int> list1;
			LinkListNode<int> node1(2);
			LinkListNode<int> node2(1);
			
			list1.AddNodeToTail(&node1);
			list1.AddNodeToTail(&node2);

			LinkListC<int> list2;

			list2.AddNodeToTail(&node1);

			UNIT_TEST_POSITIVE(list1 == list2, "LinkListC()");
			
			LinkListC<int> list3;
			LinkListNode<int> node3(2);
			
			list3.AddNodeToTail(&node3);

			UNIT_TEST_POSITIVE(list1 != list3, "LinkListC()");
			
			LinkListC<int> list4 (list1);

			UNIT_TEST_POSITIVE(list4 == list1, "LinkListC()");
			
			LinkListC<int> list5;
			list5 = list1;

			UNIT_TEST_POSITIVE(list5 == list1, "LinkListC()");

		UNIT_TEST_BLOCK_END();
		
		UNIT_TEST_BLOCK_START();

			LinkListC<int> list1;
			LinkListNode<int> node1(2);
			LinkListNode<int> node2(1);

			list1.AddNodeToTail(&node1);
			list1.AddNodeToTail(&node2);

			list1.RemoveAll();

			UNIT_TEST_POSITIVE(list1.Size() == 0, "LinkListC()");
			UNIT_TEST_POSITIVE(list1.HighWaterMark() == 2, "LinkListC()");

		UNIT_TEST_BLOCK_END();

		UNIT_TEST_BLOCK_START();

			LinkListC<int> list1;
			LinkListNode<int> node1(2);
			LinkListNode<int> node2(1);
			LinkListNode<int> node3(5);

			list1.AddNodeToTail(&node1);
			list1.AddNodeToTail(&node2);
			list1.AddNodeToTail(&node3);

			UNIT_TEST_POSITIVE(list1.Last() == 5, "LinkListC()");

			list1.RemoveTail();
			
			UNIT_TEST_POSITIVE(list1.Last() == 1, "LinkListC()");
			UNIT_TEST_POSITIVE(list1.Size() == 2, "LinkListC()");
			UNIT_TEST_POSITIVE(list1.HighWaterMark() == 3, "LinkListC()");

			list1.RemoveHead();

			UNIT_TEST_POSITIVE(list1.Last() == 1, "LinkListC()");
			UNIT_TEST_POSITIVE(list1.Size() == 1, "LinkListC()");
			UNIT_TEST_POSITIVE(list1.HighWaterMark() == 3, "LinkListC()");

		UNIT_TEST_BLOCK_END();

		UNIT_TEST_BLOCK_START();

			LinkListC<int> list1;
			LinkListNode<int> node1(2);
			LinkListNode<int> node2(1);
			LinkListNode<int> node3(5);

			list1.AddNodeToTail(&node1);
			list1.AddNodeToTail(&node2);
			list1.AddNodeToTail(&node3);

			UNIT_TEST_POSITIVE(list1.Last() == 5, "LinkListC()");

			list1.Remove(&node2);

			UNIT_TEST_POSITIVE(list1.Last() == 5, "LinkListC()");
			UNIT_TEST_POSITIVE(list1.Size() == 2, "LinkListC()");
			UNIT_TEST_POSITIVE(list1.HighWaterMark() == 3, "LinkListC()");
			UNIT_TEST_POSITIVE(!list1.Contains(1), "LinkListC()");

		UNIT_TEST_BLOCK_END();
		
		UNIT_TEST_BLOCK_START();

			LinkListC<int> list1;
			LinkListNode<int> node1(2);
			LinkListNode<int> node2(1);
			LinkListNode<int> node3(5);

			list1.AddNodeToTail(&node1);
			list1.AddNodeToTail(&node2);
			list1.AddNodeToTail(&node3);

			UNIT_TEST_POSITIVE(list1.Last() == 5, "LinkListC()");

			list1.RemoveNext(&node1);

			UNIT_TEST_POSITIVE(list1.Last() == 5, "LinkListC()");
			UNIT_TEST_POSITIVE(list1.Size() == 2, "LinkListC()");
			UNIT_TEST_POSITIVE(list1.HighWaterMark() == 3, "LinkListC()");
			UNIT_TEST_POSITIVE(!list1.Contains(1), "LinkListC()");

		UNIT_TEST_BLOCK_END();

		UNIT_TEST_BLOCK_START();

			class SameAsOperator
			{
			public:
				bool Equal(const int& object1, const int& object2)const
				{
					return (object1 == (object2));
				}
			};

			SameAsOperator sameAsOperator;

			LinkListC<int> list1;
			LinkListNode<int> node1(2);
			LinkListNode<int> node2(1);
			LinkListNode<int> node3(5);

			list1.AddNodeToTail(&node1);
			list1.AddNodeToTail(&node2);
			list1.AddNodeToTail(&node3);

			UNIT_TEST_POSITIVE(list1.Find(1) == &node2, "LinkListC()");
			UNIT_TEST_POSITIVE(list1.FindConst(1) == &node2, "LinkListC()");

			UNIT_TEST_POSITIVE(list1.Find(1, sameAsOperator) == &node2, "LinkListC()");
			UNIT_TEST_POSITIVE(list1.FindConst(1, sameAsOperator) == &node2, "LinkListC()");

		UNIT_TEST_BLOCK_END();

		UNIT_TEST_BLOCK_START();

			LinkListC<int> list1;
			LinkListNode<int> node1(2);
			LinkListNode<int> node2(1);
			LinkListNode<int> node3(5);
			LinkListNode<int> node4(7);

			list1.AddNodeToTail(&node1);
			list1.AddNodeToTail(&node2);
			list1.AddNodeToTail(&node3);

			UNIT_TEST_POSITIVE(list1.Last() == 5, "LinkListC()");
			UNIT_TEST_POSITIVE(list1.First() == 2, "LinkListC()");

			list1.SwapNodes(&node1, &node3);
			
			UNIT_TEST_POSITIVE(list1.Last() == 2, "LinkListC()");
			UNIT_TEST_POSITIVE(list1.First() == 5, "LinkListC()");

			UNIT_TEST_ASSERT_EXPECTED_START();
			list1.SwapNodes(NULL, NULL);
			UNIT_TEST_ASSERT_EXPECTED_END();

			UNIT_TEST_ASSERT_EXPECTED_START();
			list1.SwapNodes(&node4, &node3);
			UNIT_TEST_ASSERT_EXPECTED_END();

		UNIT_TEST_BLOCK_END();
		
		UNIT_TEST_BLOCK_START();

			LinkListC<int> list1;
			LinkListNode<int> node1(2);
			LinkListNode<int> node2(1);
			LinkListNode<int> node3(5);
			LinkListNode<int> node4(7);

			list1.AddNodeToTail(&node1);
			list1.AddNodeToTail(&node2);
			list1.AddNodeToTail(&node3);

			UNIT_TEST_POSITIVE(list1.Last() == 5, "LinkListC()");
			UNIT_TEST_POSITIVE(list1.First() == 2, "LinkListC()");

			list1.MoveNodeForward(&node1);

			UNIT_TEST_POSITIVE(list1.Last() == 5, "LinkListC()");
			UNIT_TEST_POSITIVE(list1.First() == 1, "LinkListC()");
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			list1.MoveNodeForward(NULL);
			UNIT_TEST_ASSERT_EXPECTED_END();

			UNIT_TEST_ASSERT_EXPECTED_START();
			list1.MoveNodeForward(&node4);
			UNIT_TEST_ASSERT_EXPECTED_END();
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			list1.MoveNodeForward(&node3);
			UNIT_TEST_ASSERT_EXPECTED_END();

		UNIT_TEST_BLOCK_END();

		UNIT_TEST_BLOCK_START();

			LinkListC<int> list1;
			LinkListNode<int> node1(2);
			LinkListNode<int> node2(1);
			LinkListNode<int> node3(5);
			LinkListNode<int> node4(7);

			list1.AddNodeToTail(&node1);
			list1.AddNodeToTail(&node2);
			list1.AddNodeToTail(&node3);

			UNIT_TEST_POSITIVE(list1.Last() == 5, "LinkListC()");
			UNIT_TEST_POSITIVE(list1.First() == 2, "LinkListC()");

			list1.MoveNodeBackwards(&node3);

			UNIT_TEST_POSITIVE(list1.Last() == 1, "LinkListC()");
			UNIT_TEST_POSITIVE(list1.First() == 2, "LinkListC()");

			UNIT_TEST_ASSERT_EXPECTED_START();
			list1.MoveNodeBackwards(NULL);
			UNIT_TEST_ASSERT_EXPECTED_END();

			UNIT_TEST_ASSERT_EXPECTED_START();
			list1.MoveNodeBackwards(&node4);
			UNIT_TEST_ASSERT_EXPECTED_END();

			UNIT_TEST_ASSERT_EXPECTED_START();
			list1.MoveNodeBackwards(&node1);
			UNIT_TEST_ASSERT_EXPECTED_END();

		UNIT_TEST_BLOCK_END();

		UNIT_TEST_BLOCK_START();

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

			Equality equality;

			LinkListC<int> list1;
			LinkListNode<int> node1(2);
			LinkListNode<int> node2(1);
			LinkListNode<int> node3(5);

			list1.AddNodeToTail(&node1);
			list1.AddNodeToTail(&node2);
			list1.AddNodeToTail(&node3);

			UNIT_TEST_POSITIVE(!list1.IsSorted(), "LinkListC()");
			UNIT_TEST_POSITIVE(!list1.IsSorted(equality), "LinkListC()");
			
			list1.Sort();

 			UNIT_TEST_POSITIVE(list1.IsSorted(), "LinkListC()");
			UNIT_TEST_POSITIVE(list1.Size() == 3, "LinkListC()");
			UNIT_TEST_POSITIVE(list1.First() == 1, "LinkListC()");
			UNIT_TEST_POSITIVE(list1.Last() == 5, "LinkListC()");

			LinkListC<int> list2;
			LinkListNode<int> node4(2);
			LinkListNode<int> node5(1);
			LinkListNode<int> node6(5);

			list2.AddNodeToTail(&node4);
			list2.AddNodeToTail(&node5);
			list2.AddNodeToTail(&node6);

			list2.Sort(equality);

			UNIT_TEST_POSITIVE(list2.IsSorted(), "LinkListC()");
			UNIT_TEST_POSITIVE(list2.Size() == 3, "LinkListC()");
			UNIT_TEST_POSITIVE(list2.First() == 1, "LinkListC()");
			UNIT_TEST_POSITIVE(list2.Last() == 5, "LinkListC()");

		UNIT_TEST_BLOCK_END();

		UNIT_TEST_BLOCK_START();

			class Eval
			{
			public:
				float Evaluate(const int& object1)const
				{
					return static_cast<float>(object1);
				};
			};

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

			UNIT_TEST_POSITIVE(list1.HighestEvalution(eval)->GetPayload() == 18, "LinkListC");
			UNIT_TEST_POSITIVE(list1.HighestEvalutionConst(eval)->GetPayloadConst() == 18, "LinkListC");

		UNIT_TEST_BLOCK_END();
		
		mState = kFinished;
	}
}