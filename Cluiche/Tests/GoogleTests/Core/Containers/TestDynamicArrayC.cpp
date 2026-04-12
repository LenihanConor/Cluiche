#include <gtest/gtest.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

using namespace Dia::Core::Containers;

TEST(DynamicArrayC, DefaultConstruction_HasZeroSize)
{
	using IntArray3 = DynamicArrayC<int, 3>;
	IntArray3 array;

	EXPECT_DEATH({ int temp = array.At(0); }, "");

	EXPECT_EQ(array.Capacity(), 3);
	EXPECT_EQ(array.Size(), 0);
}

TEST(DynamicArrayC, ConstructionFromPointer_CopiesData)
{
	using IntArray3 = DynamicArrayC<int, 3>;
	int cArray1[3] = {1, 2, 3};

	EXPECT_DEATH({ IntArray3 temp(nullptr, 3); }, "");
	EXPECT_DEATH({ IntArray3 temp(&cArray1[0], 10); }, "");

	IntArray3 array1(&cArray1[0], 3);

	EXPECT_EQ(array1.Capacity(), 3);
	EXPECT_TRUE(array1.IsFull());
	EXPECT_EQ(array1.Size(), 3);
	EXPECT_EQ(array1.At(0), 1);
	EXPECT_EQ(array1.At(1), 2);
	EXPECT_EQ(array1.At(2), 3);

	int cArray2[5] = {1, 2, 3, 4, 5};
	IntArray3 array2(&cArray2[0], 3);

	EXPECT_EQ(array2.Capacity(), 3);
	EXPECT_TRUE(array2.IsFull());
	EXPECT_EQ(array2.Size(), 3);
	EXPECT_EQ(array2.At(0), 1);
	EXPECT_EQ(array2.At(1), 2);
	EXPECT_EQ(array2.At(2), 3);

	int cArray3[2] = {1, 2};
	IntArray3 array3(&cArray3[0], 2);

	EXPECT_EQ(array3.Capacity(), 3);
	EXPECT_FALSE(array3.IsFull());
	EXPECT_EQ(array3.Size(), 2);
	EXPECT_EQ(array3.At(0), 1);
	EXPECT_EQ(array3.At(1), 2);

	EXPECT_DEATH({ int temp = array3.At(2); }, "");
}

TEST(DynamicArrayC, CopyConstruction_DuplicatesArray)
{
	using IntArray3 = DynamicArrayC<int, 3>;
	int cArray1[3] = {1, 2, 3};

	EXPECT_DEATH({ IntArray3 temp(cArray1, 10); }, "");

	DynamicArrayC<int, 3> tempArray1(cArray1, 3);
	DynamicArrayC<int, 3> array1(tempArray1);

	EXPECT_TRUE(array1.IsFull());
	EXPECT_EQ(array1.Capacity(), 3);
	EXPECT_EQ(array1.Size(), 3);
	EXPECT_EQ(array1.At(0), 1);
	EXPECT_EQ(array1.At(1), 2);
	EXPECT_EQ(array1.At(2), 3);

	int cArray2[2] = {1, 2};
	DynamicArrayC<int, 2> tempArray2(cArray2, 2);
	DynamicArrayC<int, 3> array2(tempArray2);

	EXPECT_FALSE(array2.IsFull());
	EXPECT_EQ(array2.Capacity(), 3);
	EXPECT_EQ(array2.Size(), 2);
	EXPECT_EQ(array2.At(0), 1);
	EXPECT_EQ(array2.At(1), 2);
}

TEST(DynamicArrayC, ConstructionWithRange_ExtractsSubarray)
{
	int cArray1[5] = {1, 2, 3, 4, 5};
	DynamicArrayC<int, 5> tempArray1(cArray1, 5);
	DynamicArrayC<int, 3> array1(tempArray1, 1, 3);

	EXPECT_TRUE(array1.IsFull());
	EXPECT_EQ(array1.Capacity(), 3);
	EXPECT_EQ(array1.Size(), 3);
	EXPECT_EQ(array1.At(0), 2);
	EXPECT_EQ(array1.At(1), 3);
	EXPECT_EQ(array1.At(2), 4);

	DynamicArrayC<int, 3> array2(tempArray1, 1, 2);

	EXPECT_FALSE(array2.IsFull());
	EXPECT_EQ(array2.Capacity(), 3);
	EXPECT_EQ(array2.Size(), 2);
	EXPECT_EQ(array2.At(0), 2);
	EXPECT_EQ(array2.At(1), 3);
}

TEST(DynamicArrayC, ConstructionFromIterator_CopiesFromBegin)
{
	int cArray1[3] = {1, 2, 3};
	DynamicArrayC<int, 3> tempArray1(cArray1, 3);

	DynamicArrayC<int, 3>::ConstIterator iter1(tempArray1.Begin());
	DynamicArrayC<int, 3> array1(3, iter1);

	EXPECT_TRUE(array1.IsFull());
	EXPECT_EQ(array1.Size(), 3);
	EXPECT_EQ(array1.At(0), 1);
	EXPECT_EQ(array1.At(1), 2);
	EXPECT_EQ(array1.At(2), 3);

	int cArray2[2] = {1, 2};
	DynamicArrayC<int, 2> tempArray2(cArray2, 2);
	DynamicArrayC<int, 2> array2(2, tempArray2.BeginConst());

	EXPECT_TRUE(array2.IsFull());
	EXPECT_EQ(array2.Size(), 2);
	EXPECT_EQ(array2.At(0), 1);
	EXPECT_EQ(array2.At(1), 2);
}

TEST(DynamicArrayC, ConstructionFromReverseIterator_CopiesInReverse)
{
	int cArray1[3] = {1, 2, 3};
	DynamicArrayC<int, 3> tempArray1(cArray1, 3);

	DynamicArrayC<int, 3> array1(3, tempArray1.EndConst());

	EXPECT_TRUE(array1.IsFull());
	EXPECT_EQ(array1.Size(), 3);
	EXPECT_EQ(array1.At(0), 3);
	EXPECT_EQ(array1.At(1), 2);
	EXPECT_EQ(array1.At(2), 1);

	int cArray2[2] = {1, 2};
	DynamicArrayC<int, 2> tempArray2(cArray2, 2);
	DynamicArrayC<int, 2> array2(2, tempArray2.EndConst());

	EXPECT_TRUE(array2.IsFull());
	EXPECT_EQ(array2.Size(), 2);
	EXPECT_EQ(array2.At(0), 2);
	EXPECT_EQ(array2.At(1), 1);
}

TEST(DynamicArrayC, ConstructionWithFilter_FiltersElements)
{
	class AboveTwoFilter
	{
	public:
		bool Evaluate(const int& currentIter)const
		{
			return (currentIter > 2);
		};
	};

	AboveTwoFilter filter1;

	int cArray1[5] = {1, 2, 3, 4, 5};
	DynamicArrayC<int, 5> tempArray1(cArray1, 5);
	DynamicArrayC<int, 5> array1(5, tempArray1.BeginConst(), filter1);

	EXPECT_EQ(array1.Size(), 3);
	EXPECT_EQ(array1.At(0), 3);
	EXPECT_EQ(array1.At(1), 4);
	EXPECT_EQ(array1.At(2), 5);
}

TEST(DynamicArrayC, AssignFromPointer_OverwritesData)
{
	using IntArray3 = DynamicArrayC<int, 3>;
	int cArray1[3] = {1, 2, 3};

	EXPECT_DEATH({ IntArray3 array0; array0.Assign(nullptr, 3); }, "");
	EXPECT_DEATH({ IntArray3 array1; array1.Assign(&cArray1[0], 10); }, "");

	IntArray3 array1;
	array1.Assign(&cArray1[0], 3);

	EXPECT_TRUE(array1.IsFull());
	EXPECT_EQ(array1.Size(), 3);
	EXPECT_EQ(array1.At(0), 1);
	EXPECT_EQ(array1.At(1), 2);
	EXPECT_EQ(array1.At(2), 3);

	int cArray2[5] = {1, 2, 3, 4, 5};
	DynamicArrayC<int, 3> array2;
	array2.Assign(&cArray2[0], 3);

	EXPECT_TRUE(array2.IsFull());
	EXPECT_EQ(array2.Size(), 3);
	EXPECT_EQ(array2.At(0), 1);
	EXPECT_EQ(array2.At(1), 2);
	EXPECT_EQ(array2.At(2), 3);

	int cArray3[2] = {1, 2};
	DynamicArrayC<int, 3> array3;
	array3.Assign(&cArray3[0], 2);

	EXPECT_FALSE(array3.IsFull());
	EXPECT_EQ(array3.Size(), 2);
	EXPECT_EQ(array3.At(0), 1);
	EXPECT_EQ(array3.At(1), 2);
}

TEST(DynamicArrayC, AssignFromArray_CopiesData)
{
	int cArray1[5] = {1, 2, 3, 4, 5};
	DynamicArrayC<int, 5> tempArray1(cArray1, 5);
	DynamicArrayC<int, 5> array1;
	array1.Assign(tempArray1);

	EXPECT_TRUE(array1.IsFull());
	EXPECT_EQ(array1.Size(), 5);
	EXPECT_EQ(array1.At(0), 1);
	EXPECT_EQ(array1.At(1), 2);
	EXPECT_EQ(array1.At(2), 3);
	EXPECT_EQ(array1.At(3), 4);
	EXPECT_EQ(array1.At(4), 5);

	DynamicArrayC<int, 3> array2;
	array2.Assign(tempArray1);

	EXPECT_TRUE(array2.IsFull());
	EXPECT_EQ(array2.Size(), 3);
	EXPECT_EQ(array2.At(0), 1);
	EXPECT_EQ(array2.At(1), 2);
	EXPECT_EQ(array2.At(2), 3);
}

TEST(DynamicArrayC, AssignWithRange_CopiesSubarray)
{
	int cArray1[5] = {1, 2, 3, 4, 5};
	DynamicArrayC<int, 5> tempArray1(cArray1, 5);
	DynamicArrayC<int, 3> array1;
	array1.Assign(tempArray1, 1, 3);

	EXPECT_TRUE(array1.IsFull());
	EXPECT_EQ(array1.Size(), 3);
	EXPECT_EQ(array1.At(0), 2);
	EXPECT_EQ(array1.At(1), 3);
	EXPECT_EQ(array1.At(2), 4);

	DynamicArrayC<int, 3> array2;
	array2.Assign(tempArray1, 1, 2);

	EXPECT_FALSE(array2.IsFull());
	EXPECT_EQ(array2.Size(), 2);
	EXPECT_EQ(array2.At(0), 2);
	EXPECT_EQ(array2.At(1), 3);
}

TEST(DynamicArrayC, AssignFromIterator_CopiesFromBegin)
{
	int cArray1[3] = {1, 2, 3};
	DynamicArrayC<int, 3> tempArray1(cArray1, 3);

	DynamicArrayC<int, 3> array1;
	array1.Assign(3, tempArray1.BeginConst());

	EXPECT_TRUE(array1.IsFull());
	EXPECT_EQ(array1.Size(), 3);
	EXPECT_EQ(array1.At(0), 1);
	EXPECT_EQ(array1.At(1), 2);
	EXPECT_EQ(array1.At(2), 3);

	int cArray2[2] = {1, 2};
	DynamicArrayC<int, 2> tempArray2(cArray2, 2);
	DynamicArrayC<int, 2> array2;
	array2.Assign(2, tempArray2.BeginConst());

	EXPECT_TRUE(array2.IsFull());
	EXPECT_EQ(array2.Size(), 2);
	EXPECT_EQ(array2.At(0), 1);
	EXPECT_EQ(array2.At(1), 2);
}

TEST(DynamicArrayC, AssignFromReverseIterator_CopiesInReverse)
{
	int cArray1[3] = {1, 2, 3};
	DynamicArrayC<int, 3> tempArray1(cArray1, 3);

	DynamicArrayC<int, 3> array1;
	array1.Assign(3, tempArray1.EndConst());

	EXPECT_TRUE(array1.IsFull());
	EXPECT_EQ(array1.Size(), 3);
	EXPECT_EQ(array1.At(0), 3);
	EXPECT_EQ(array1.At(1), 2);
	EXPECT_EQ(array1.At(2), 1);

	int cArray2[2] = {1, 2};
	DynamicArrayC<int, 2> tempArray2(cArray2, 2);
	DynamicArrayC<int, 3> array3;
	array3.Assign(2, tempArray2.EndConst());

	EXPECT_FALSE(array3.IsFull());
	EXPECT_EQ(array3.Size(), 2);
	EXPECT_EQ(array3.At(0), 2);
	EXPECT_EQ(array3.At(1), 1);
}

TEST(DynamicArrayC, AssignWithFilter_FiltersElements)
{
	class AboveTwoFilter
	{
	public:
		bool Evaluate(const int& currentIter)const
		{
			return (currentIter > 2);
		};
	};

	AboveTwoFilter filter1;

	int cArray1[5] = {1, 2, 3, 4, 5};
	DynamicArrayC<int, 5> tempArray1(cArray1, 5);

	DynamicArrayC<int, 5> array1;
	array1.Assign(5, tempArray1.BeginConst(), filter1);

	EXPECT_EQ(array1.Size(), 3);
	EXPECT_EQ(array1.At(0), 3);
	EXPECT_EQ(array1.At(1), 4);
	EXPECT_EQ(array1.At(2), 5);
}

TEST(DynamicArrayC, AssignmentOperator_CopiesArray)
{
	int cArray1[5] = {1, 2, 3, 4, 5};
	DynamicArrayC<int, 5> tempArray1(cArray1, 5);
	DynamicArrayC<int, 5> array1;
	array1 = tempArray1;

	EXPECT_TRUE(array1.IsFull());
	EXPECT_EQ(array1.Size(), 5);
	EXPECT_EQ(array1.At(0), 1);
	EXPECT_EQ(array1.At(1), 2);
	EXPECT_EQ(array1.At(2), 3);
	EXPECT_EQ(array1.At(3), 4);
	EXPECT_EQ(array1.At(4), 5);
}

TEST(DynamicArrayC, ComparisonOperators_CheckEquality)
{
	int cArray1[5] = {1, 2, 3, 4, 5};
	int cArray3[5] = {1, 2, 3, 4, 6};  // Different last element

	DynamicArrayC<int, 5> array1(cArray1, 5);
	DynamicArrayC<int, 5> array2(array1);
	DynamicArrayC<int, 5> array3(cArray3, 5);

	EXPECT_TRUE(array1 == array2);
	EXPECT_FALSE(array1 == array3);
	EXPECT_TRUE(array1 != array3);
	EXPECT_FALSE(array1 != array2);
}

TEST(DynamicArrayC, ElementAccess_WorksCorrectlyWithBoundsChecking)
{
	using IntArray5 = DynamicArrayC<int, 5>;
	int cArray1[5] = {1, 2, 3, 4, 5};

	IntArray5 array1(cArray1, 5);

	EXPECT_DEATH({ int temp = array1[-1]; }, "");
	EXPECT_DEATH({ int temp = array1[6]; }, "");
	EXPECT_DEATH({ int temp = array1.At(-1); }, "");
	EXPECT_DEATH({ int temp = array1.At(6); }, "");

	EXPECT_EQ(array1[0], array1.At(0));
	EXPECT_EQ(array1[1], array1.At(1));
	EXPECT_EQ(array1[2], array1.At(2));
	EXPECT_EQ(array1[3], array1.At(3));
	EXPECT_EQ(array1[4], array1.At(4));

	EXPECT_EQ(array1.Front(), array1.At(0));
	EXPECT_EQ(array1.Back(), array1.At(4));
}

TEST(DynamicArrayC, Iterators_ProvideCorrectAccess)
{
	int cArray1[5] = {1, 2, 3, 4, 5};

	DynamicArrayC<int, 5> array1(cArray1, 5);

	EXPECT_EQ(&array1[0], array1.IteratorAt(0).Current());
	EXPECT_EQ(&array1[1], array1.IteratorAt(1).Current());
	EXPECT_EQ(&array1[2], array1.IteratorAt(2).Current());
	EXPECT_EQ(&array1[3], array1.IteratorAt(3).Current());
	EXPECT_EQ(&array1[4], array1.IteratorAt(4).Current());

	EXPECT_TRUE(array1.Begin() == array1.IteratorAt(0));
	EXPECT_TRUE(array1.BeginConst() == array1.IteratorAtConst(0));

	EXPECT_EQ(&array1[0], array1.ReverseIteratorAt(0).Current());
	EXPECT_EQ(&array1[1], array1.ReverseIteratorAt(1).Current());
	EXPECT_EQ(&array1[2], array1.ReverseIteratorAt(2).Current());
	EXPECT_EQ(&array1[3], array1.ReverseIteratorAt(3).Current());
	EXPECT_EQ(&array1[4], array1.ReverseIteratorAt(4).Current());

	EXPECT_TRUE(array1.End() == array1.ReverseIteratorAt(4));
	EXPECT_TRUE(array1.EndConst() == array1.ReverseIteratorAtConst(4));
}

TEST(DynamicArrayC, Add_IncreasesSize)
{
	using IntArray5 = DynamicArrayC<int, 5>;
	IntArray5 array1;

	EXPECT_TRUE(array1.IsEmpty());
	EXPECT_FALSE(array1.IsFull());

	array1.Add(1);

	EXPECT_FALSE(array1.IsEmpty());
	EXPECT_FALSE(array1.IsFull());
	EXPECT_EQ(array1.Size(), 1);
	EXPECT_EQ(array1.At(0), 1);

	array1.Add(2);
	array1.Add(3);

	EXPECT_EQ(array1.Size(), 3);
	EXPECT_EQ(array1.At(0), 1);
	EXPECT_EQ(array1.At(1), 2);
	EXPECT_EQ(array1.At(2), 3);

	array1.Add(4);
	array1.Add(5);

	EXPECT_TRUE(array1.IsFull());
	EXPECT_EQ(array1.Size(), 5);

	EXPECT_DEATH(array1.Add(6), "");
}

TEST(DynamicArrayC, Remove_DecreasesSize)
{
	int cArray1[5] = {1, 2, 3, 4, 5};
	DynamicArrayC<int, 5> array1(cArray1, 5);

	EXPECT_TRUE(array1.IsFull());

	array1.RemoveAt(2);

	EXPECT_FALSE(array1.IsFull());
	EXPECT_EQ(array1.Size(), 4);
	EXPECT_EQ(array1.At(0), 1);
	EXPECT_EQ(array1.At(1), 2);
	EXPECT_EQ(array1.At(2), 4);
	EXPECT_EQ(array1.At(3), 5);

	array1.RemoveAt(0);

	EXPECT_EQ(array1.Size(), 3);
	EXPECT_EQ(array1.At(0), 2);
	EXPECT_EQ(array1.At(1), 4);
	EXPECT_EQ(array1.At(2), 5);
}

TEST(DynamicArrayC, RemoveAll_ClearsArray)
{
	int cArray1[5] = {1, 2, 3, 4, 5};
	DynamicArrayC<int, 5> array1(cArray1, 5);

	EXPECT_TRUE(array1.IsFull());

	array1.RemoveAll();

	EXPECT_TRUE(array1.IsEmpty());
	EXPECT_EQ(array1.Size(), 0);
}

TEST(DynamicArrayC, FrequencyOperations_CalculateCorrectly)
{
	int cArray1[10] = {1, 2, 3, 4, 5, 1, 3, 3, 3, 4};

	DynamicArrayC<int, 10> array1(cArray1, 10);

	EXPECT_EQ(array1.FrequencyOfElement(1), 2);
	EXPECT_EQ(array1.FrequencyOfElement(2), 1);
	EXPECT_EQ(array1.FrequencyOfElement(3), 4);
	EXPECT_EQ(array1.FrequencyOfElement(4), 2);
	EXPECT_EQ(array1.FrequencyOfElement(5), 1);
	EXPECT_EQ(array1.FrequencyOfElement(6), 0);

	unsigned int numberOfUniqueElements = 0;
	DynamicArrayC<int, 10> arrayUnique;
	array1.UniqueElements(arrayUnique, numberOfUniqueElements);

	EXPECT_EQ(numberOfUniqueElements, 5);
	EXPECT_EQ(arrayUnique[0], 1);
	EXPECT_EQ(arrayUnique[1], 2);
	EXPECT_EQ(arrayUnique[2], 3);
	EXPECT_EQ(arrayUnique[3], 4);
	EXPECT_EQ(arrayUnique[4], 5);
}

TEST(DynamicArrayC, Sort_OrdersElementsCorrectly)
{
	class IncreasingOrder
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

	IncreasingOrder order;

	int cArray1[10] = {1, 2, 3, 4, 5, 1, 3, 3, 3, 4};

	DynamicArrayC<int, 10> array1(cArray1, 10);
	DynamicArrayC<int, 10> array2(cArray1, 10);

	array1.Sort(order);

	EXPECT_EQ(array1[0], 1);
	EXPECT_EQ(array1[1], 1);
	EXPECT_EQ(array1[2], 2);
	EXPECT_EQ(array1[3], 3);
	EXPECT_EQ(array1[4], 3);
	EXPECT_EQ(array1[5], 3);
	EXPECT_EQ(array1[6], 3);
	EXPECT_EQ(array1[7], 4);
	EXPECT_EQ(array1[8], 4);
	EXPECT_EQ(array1[9], 5);
	EXPECT_TRUE(array1.IsSorted(order));

	array2.Sort();

	EXPECT_EQ(array2[0], 1);
	EXPECT_EQ(array2[1], 1);
	EXPECT_EQ(array2[2], 2);
	EXPECT_TRUE(array2.IsSorted());
}

TEST(DynamicArrayC, Swap_ExchangesContents)
{
	int cArray1[5] = {1, 2, 3, 4, 5};
	int cArray2[5] = {4, 2, 5, 1, 3};

	DynamicArrayC<int, 5> array1(cArray1, 5);
	DynamicArrayC<int, 5> array2(cArray2, 5);

	array1.Swap(array2);

	EXPECT_EQ(array1[0], 4);
	EXPECT_EQ(array1[1], 2);
	EXPECT_EQ(array1[2], 5);
	EXPECT_EQ(array1[3], 1);
	EXPECT_EQ(array1[4], 3);

	EXPECT_EQ(array2[0], 1);
	EXPECT_EQ(array2[1], 2);
	EXPECT_EQ(array2[2], 3);
	EXPECT_EQ(array2[3], 4);
	EXPECT_EQ(array2[4], 5);
}

TEST(DynamicArrayC, FindOperations_LocateElements)
{
	int cArray1[10] = {1, 2, 3, 4, 5, 6, 3, 4, 5, 7};

	DynamicArrayC<int, 10> array1(cArray1, 10);

	class OneLess
	{
	public:
		bool Equal(const int& object1, const int& object2)const
		{
			return (object1 == (object2 - 1));
		}
	};

	OneLess oneLess;

	EXPECT_EQ(array1.FindIndex(5), 4);
	EXPECT_EQ(array1.FindIndex(7), 9);
	EXPECT_EQ(array1.FindIndex(10), -1);

	EXPECT_EQ(array1.FindBetweenIndex(5, 0, 7), 4);
	EXPECT_EQ(array1.FindBetweenIndex(5, 6, 9), 8);
	EXPECT_EQ(array1.FindBetweenIndex(1, 6, 9), -1);

	EXPECT_EQ(array1.FindIndex(5, oneLess), 3);
	EXPECT_EQ(array1.FindIndex(7, oneLess), 5);
	EXPECT_EQ(array1.FindIndex(10, oneLess), -1);
}

TEST(DynamicArrayC, FindLastOperations_LocateLastOccurrence)
{
	int cArray1[10] = {1, 2, 3, 4, 5, 6, 3, 4, 5, 7};

	DynamicArrayC<int, 10> array1(cArray1, 10);

	class OneLess
	{
	public:
		bool Equal(const int& object1, const int& object2)const
		{
			return (object1 == (object2 - 1));
		}
	};

	OneLess oneLess;

	EXPECT_EQ(array1.FindLastIndex(5), 8);
	EXPECT_EQ(array1.FindLastIndex(3), 6);
	EXPECT_EQ(array1.FindLastIndex(10), -1);

	EXPECT_EQ(array1.FindLastBetweenIndex(4, 0, 7), 7);
	EXPECT_EQ(array1.FindLastBetweenIndex(5, 6, 9), 8);
	EXPECT_EQ(array1.FindLastBetweenIndex(1, 6, 9), -1);

	EXPECT_EQ(array1.FindLastIndex(5, oneLess), 7);
	EXPECT_EQ(array1.FindLastIndex(7, oneLess), 5);
	EXPECT_EQ(array1.FindLastIndex(10, oneLess), -1);
}

TEST(DynamicArrayC, SortedFindOperations_LocateInSortedArray)
{
	int cArray1[10] = {1, 2, 3, 4, 5, 6, 3, 4, 5, 7};
	DynamicArrayC<int, 10> array1(cArray1, 10);

	array1.Sort();

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

	EXPECT_TRUE(array1.IsSorted());

	EXPECT_EQ(array1.FindSortedIndex(5), 6);
	EXPECT_EQ(array1.FindSortedIndex(3), 2);
	EXPECT_EQ(array1.FindSortedIndex(10), -1);

	EXPECT_EQ(array1.FindLastSortedIndex(4), 5);
	EXPECT_EQ(array1.FindLastSortedIndex(5), 7);
	EXPECT_EQ(array1.FindLastSortedIndex(10), -1);

	EXPECT_EQ(array1.FindSortedIndex(5, equality), 6);
	EXPECT_EQ(array1.FindSortedIndex(3, equality), 2);
	EXPECT_EQ(array1.FindSortedIndex(10, equality), -1);

	EXPECT_EQ(array1.FindLastSortedIndex(4, equality), 5);
	EXPECT_EQ(array1.FindLastSortedIndex(5, equality), 7);
	EXPECT_EQ(array1.FindLastSortedIndex(10, equality), -1);
}

TEST(DynamicArrayC, HighestEvaluationIndex_FindsMaximum)
{
	class Eval
	{
	public:
		float Evaluate(const int& object1)const
		{
			return static_cast<float>(object1);
		};
	};

	Eval eval;

	int cArray1[10] = {1, 2, 3, 9, 5, 6, 3, 4, 5, 7};

	DynamicArrayC<int, 10> array1(cArray1, 10);

	EXPECT_EQ(array1.HighestEvalutionIndex(eval), 3);
}

TEST(DynamicArrayC, NonPrimitiveTypes_WorkCorrectly)
{
	class Foo
	{
	public:
		Foo() : mSomeData(true), mMoreData(21.0f) {}

		bool SomeData() { return mSomeData; }
		float MoreData() { return mMoreData; }

		bool mSomeData;
		float mMoreData;
	};

	Foo cArray1[3];

	DynamicArrayC<Foo, 3> array1(cArray1, 3);

	cArray1[0].mSomeData = false;
	cArray1[0].mMoreData = 11.0f;

	EXPECT_EQ(array1.Size(), 3);
	EXPECT_TRUE(array1.At(0).SomeData());
	EXPECT_TRUE(array1.At(1).SomeData());
	EXPECT_TRUE(array1.At(2).SomeData());

	EXPECT_EQ(array1.At(0).MoreData(), 21.0f);
	EXPECT_EQ(array1.At(1).MoreData(), 21.0f);
	EXPECT_EQ(array1.At(2).MoreData(), 21.0f);
}
