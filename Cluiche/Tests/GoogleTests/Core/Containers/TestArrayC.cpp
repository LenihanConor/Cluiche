#include <gtest/gtest.h>
#include <DiaCore/Containers/Arrays/ArrayC.h>

using namespace Dia::Core::Containers;

TEST(ArrayC, DefaultConstruction_InitializesWithZeroValues)
{
	ArrayC<int, 3> array;

	EXPECT_EQ(array.Size(), 3);
	EXPECT_EQ(array.At(0), 0);
}

TEST(ArrayC, ConstructionFromPointer_CopiesData)
{
	using IntArray3 = ArrayC<int, 3>;
	int cArray1[3] = {1, 2, 3};

	EXPECT_DEATH({ IntArray3 temp(nullptr, 3); }, "");
	EXPECT_DEATH({ IntArray3 temp(&cArray1[0], 10); }, "");

	IntArray3 array1(&cArray1[0], 3);

	EXPECT_EQ(array1.Size(), 3);
	EXPECT_EQ(array1.At(0), 1);
	EXPECT_EQ(array1.At(1), 2);
	EXPECT_EQ(array1.At(2), 3);

	int cArray2[5] = {1, 2, 3, 4, 5};
	IntArray3 array2(&cArray2[0], 3);

	EXPECT_EQ(array2.Size(), 3);
	EXPECT_EQ(array2.At(0), 1);
	EXPECT_EQ(array2.At(1), 2);
	EXPECT_EQ(array2.At(2), 3);

	int cArray3[2] = {1, 2};
	IntArray3 array3(&cArray3[0], 2);

	EXPECT_EQ(array3.Size(), 3);
	EXPECT_EQ(array3.At(0), 1);
	EXPECT_EQ(array3.At(1), 2);
	EXPECT_EQ(array3.At(2), 0);
}

TEST(ArrayC, CopyConstruction_DuplicatesArray)
{
	using IntArray3 = ArrayC<int, 3>;
	int cArray1[3] = {1, 2, 3};

	EXPECT_DEATH({ IntArray3 temp(cArray1, 10); }, "");

	IntArray3 tempArray1(cArray1, 3);
	IntArray3 array1(tempArray1);

	EXPECT_EQ(array1.Size(), 3);
	EXPECT_EQ(array1.At(0), 1);
	EXPECT_EQ(array1.At(1), 2);
	EXPECT_EQ(array1.At(2), 3);

	int cArray2[5] = {1, 2, 3, 4, 5};
	ArrayC<int, 5> tempArray2(cArray2, 5);
	ArrayC<int, 3> array2(tempArray2);

	EXPECT_EQ(array2.Size(), 3);
	EXPECT_EQ(array2.At(0), 1);
	EXPECT_EQ(array2.At(1), 2);
	EXPECT_EQ(array2.At(2), 3);

	int cArray3[2] = {1, 2};
	ArrayC<int, 2> tempArray3(cArray3, 2);
	ArrayC<int, 3> array3(tempArray3);

	EXPECT_EQ(array3.Size(), 3);
	EXPECT_EQ(array3.At(0), 1);
	EXPECT_EQ(array3.At(1), 2);
	EXPECT_EQ(array3.At(2), 0);
}

TEST(ArrayC, ConstructionWithOtherArrayAndRange_ExtractsSubarray)
{
	int cArray1[5] = {1, 2, 3, 4, 5};
	ArrayC<int, 5> tempArray1(cArray1, 5);
	ArrayC<int, 3> array1(tempArray1, 1, 3);

	EXPECT_EQ(array1.Size(), 3);
	EXPECT_EQ(array1.At(0), 2);
	EXPECT_EQ(array1.At(1), 3);
	EXPECT_EQ(array1.At(2), 4);

	int cArray2[5] = {1, 2, 3, 4, 5};
	ArrayC<int, 5> tempArray2(cArray2, 5);
	ArrayC<int, 3> array2(tempArray2, 1, 2);

	EXPECT_EQ(array2.Size(), 3);
	EXPECT_EQ(array2.At(0), 2);
	EXPECT_EQ(array2.At(1), 3);
	EXPECT_EQ(array2.At(2), 0);
}

TEST(ArrayC, ConstructionFromIterator_CopiesFromBegin)
{
	int cArray1[3] = {1, 2, 3};
	ArrayC<int, 3> tempArray1(cArray1, 3);

	ArrayC<int, 3>::ConstIterator iter1(tempArray1.Begin());
	ArrayC<int, 3> array1(iter1);

	EXPECT_EQ(array1.Size(), 3);
	EXPECT_EQ(array1.At(0), 1);
	EXPECT_EQ(array1.At(1), 2);
	EXPECT_EQ(array1.At(2), 3);

	int cArray2[2] = {1, 2};
	ArrayC<int, 2> tempArray2(cArray2, 2);
	ArrayC<int, 2> array2(tempArray2.BeginConst());

	EXPECT_EQ(array2.Size(), 2);
	EXPECT_EQ(array2.At(0), 1);
	EXPECT_EQ(array2.At(1), 2);
}

TEST(ArrayC, ConstructionFromReverseIterator_CopiesInReverse)
{
	int cArray1[3] = {1, 2, 3};
	ArrayC<int, 3> tempArray1(cArray1, 3);

	ArrayC<int, 3> array1(tempArray1.EndConst());

	EXPECT_EQ(array1.Size(), 3);
	EXPECT_EQ(array1.At(0), 3);
	EXPECT_EQ(array1.At(1), 2);
	EXPECT_EQ(array1.At(2), 1);

	int cArray2[2] = {1, 2};
	ArrayC<int, 2> tempArray2(cArray2, 2);
	ArrayC<int, 2> array2(tempArray2.EndConst());

	EXPECT_EQ(array2.Size(), 2);
	EXPECT_EQ(array2.At(0), 2);
	EXPECT_EQ(array2.At(1), 1);
}

TEST(ArrayC, ConstructionWithFilter_FiltersElements)
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
	ArrayC<int, 5> tempArray1(cArray1, 5);
	ArrayC<int, 5> array1(tempArray1.BeginConst(), filter1);

	EXPECT_EQ(array1.Size(), 5);
	EXPECT_EQ(array1.At(0), 3);
	EXPECT_EQ(array1.At(1), 4);
	EXPECT_EQ(array1.At(2), 5);
	EXPECT_EQ(array1.At(3), 0);
	EXPECT_EQ(array1.At(4), 0);
}

TEST(ArrayC, AssignFromPointer_OverwritesData)
{
	using IntArray3 = ArrayC<int, 3>;
	int cArray1[3] = {1, 2, 3};

	EXPECT_DEATH({ IntArray3 array0; array0.Assign(nullptr, 3); }, "");
	EXPECT_DEATH({ IntArray3 array1; array1.Assign(&cArray1[0], 10); }, "");

	IntArray3 array1;
	array1.Assign(&cArray1[0], 3);

	EXPECT_EQ(array1.Size(), 3);
	EXPECT_EQ(array1.At(0), 1);
	EXPECT_EQ(array1.At(1), 2);
	EXPECT_EQ(array1.At(2), 3);

	int cArray2[5] = {1, 2, 3, 4, 5};
	ArrayC<int, 3> array2;
	array2.Assign(&cArray2[0], 3);

	EXPECT_EQ(array2.Size(), 3);
	EXPECT_EQ(array2.At(0), 1);
	EXPECT_EQ(array2.At(1), 2);
	EXPECT_EQ(array2.At(2), 3);

	int cArray3[2] = {1, 2};
	ArrayC<int, 3> array3;
	array3.Assign(&cArray3[0], 2);

	EXPECT_EQ(array3.Size(), 3);
	EXPECT_EQ(array3.At(0), 1);
	EXPECT_EQ(array3.At(1), 2);
	EXPECT_EQ(array3.At(2), 0);
}

TEST(ArrayC, AssignFromArray_CopiesData)
{
	int cArray1[5] = {1, 2, 3, 4, 5};
	ArrayC<int, 5> tempArray1(cArray1, 5);
	ArrayC<int, 5> array1;
	array1.Assign(tempArray1);

	EXPECT_EQ(array1.Size(), 5);
	EXPECT_EQ(array1.At(0), 1);
	EXPECT_EQ(array1.At(1), 2);
	EXPECT_EQ(array1.At(2), 3);
	EXPECT_EQ(array1.At(3), 4);
	EXPECT_EQ(array1.At(4), 5);

	int cArray2[5] = {1, 2, 3, 4, 5};
	ArrayC<int, 5> tempArray2(cArray2, 5);
	ArrayC<int, 3> array2;
	array2.Assign(tempArray2);

	EXPECT_EQ(array2.Size(), 3);
	EXPECT_EQ(array2.At(0), 1);
	EXPECT_EQ(array2.At(1), 2);
	EXPECT_EQ(array2.At(2), 3);
}

TEST(ArrayC, AssignWithRange_CopiesSubarray)
{
	int cArray1[5] = {1, 2, 3, 4, 5};
	ArrayC<int, 5> tempArray1(cArray1, 5);
	ArrayC<int, 3> array1;
	array1.Assign(tempArray1, 1, 3);

	EXPECT_EQ(array1.Size(), 3);
	EXPECT_EQ(array1.At(0), 2);
	EXPECT_EQ(array1.At(1), 3);
	EXPECT_EQ(array1.At(2), 4);

	int cArray2[5] = {1, 2, 3, 4, 5};
	ArrayC<int, 5> tempArray2(cArray2, 5);
	ArrayC<int, 3> array2;
	array2.Assign(tempArray2, 1, 2);

	EXPECT_EQ(array2.Size(), 3);
	EXPECT_EQ(array2.At(0), 2);
	EXPECT_EQ(array2.At(1), 3);
	EXPECT_EQ(array2.At(2), 0);
}

TEST(ArrayC, AssignFromIterator_CopiesFromBegin)
{
	int cArray1[3] = {1, 2, 3};
	ArrayC<int, 3> tempArray1(cArray1, 3);

	ArrayC<int, 3> array1;
	array1.Assign(tempArray1.BeginConst());

	EXPECT_EQ(array1.Size(), 3);
	EXPECT_EQ(array1.At(0), 1);
	EXPECT_EQ(array1.At(1), 2);
	EXPECT_EQ(array1.At(2), 3);

	int cArray2[2] = {1, 2};
	ArrayC<int, 2> tempArray2(cArray2, 2);
	ArrayC<int, 2> array2;
	array2.Assign(tempArray2.BeginConst());

	EXPECT_EQ(array2.Size(), 2);
	EXPECT_EQ(array2.At(0), 1);
	EXPECT_EQ(array2.At(1), 2);
}

TEST(ArrayC, AssignFromReverseIterator_CopiesInReverse)
{
	int cArray1[3] = {1, 2, 3};
	ArrayC<int, 3> tempArray1(cArray1, 3);

	ArrayC<int, 3> array1;
	array1.Assign(tempArray1.EndConst());

	EXPECT_EQ(array1.Size(), 3);
	EXPECT_EQ(array1.At(0), 3);
	EXPECT_EQ(array1.At(1), 2);
	EXPECT_EQ(array1.At(2), 1);

	int cArray2[2] = {1, 2};
	ArrayC<int, 2> tempArray2(cArray2, 2);
	ArrayC<int, 3> array3;
	array3.Assign(tempArray2.EndConst());

	EXPECT_EQ(array3.Size(), 3);
	EXPECT_EQ(array3.At(0), 2);
	EXPECT_EQ(array3.At(1), 1);
	EXPECT_EQ(array3.At(2), 0);
}

TEST(ArrayC, AssignWithFilter_FiltersElements)
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
	ArrayC<int, 5> tempArray1(cArray1, 5);

	ArrayC<int, 5> array1;
	array1.Assign(tempArray1.BeginConst(), filter1);

	EXPECT_EQ(array1.Size(), 5);
	EXPECT_EQ(array1.At(0), 3);
	EXPECT_EQ(array1.At(1), 4);
	EXPECT_EQ(array1.At(2), 5);
	EXPECT_EQ(array1.At(3), 0);
	EXPECT_EQ(array1.At(4), 0);
}

TEST(ArrayC, AssignmentOperator_CopiesArray)
{
	int cArray1[5] = {1, 2, 3, 4, 5};
	ArrayC<int, 5> tempArray1(cArray1, 5);
	ArrayC<int, 5> array1;
	array1 = tempArray1;

	EXPECT_EQ(array1.Size(), 5);
	EXPECT_EQ(array1.At(0), 1);
	EXPECT_EQ(array1.At(1), 2);
	EXPECT_EQ(array1.At(2), 3);
	EXPECT_EQ(array1.At(3), 4);
	EXPECT_EQ(array1.At(4), 5);
}

TEST(ArrayC, ComparisonOperators_CheckEquality)
{
	int cArray1[5] = {1, 2, 3, 4, 5};
	int cArray3[5] = {1, 2, 3, 4, 6};  // Different last element

	ArrayC<int, 5> array1(cArray1, 5);
	ArrayC<int, 5> array2(array1);
	ArrayC<int, 5> array3(cArray3, 5);

	EXPECT_TRUE(array1 == array2);
	EXPECT_FALSE(array1 == array3);
	EXPECT_TRUE(array1 != array3);
	EXPECT_FALSE(array1 != array2);
}

TEST(ArrayC, ElementAccess_WorksCorrectlyWithBoundsChecking)
{
	int cArray1[5] = {1, 2, 3, 4, 5};

	ArrayC<int, 5> array1(cArray1, 5);

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

TEST(ArrayC, Iterators_ProvideCorrectAccess)
{
	int cArray1[5] = {1, 2, 3, 4, 5};

	ArrayC<int, 5> array1(cArray1, 5);

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

TEST(ArrayC, FrequencyOperations_CalculateCorrectly)
{
	int cArray1[10] = {1, 2, 3, 4, 5, 1, 3, 3, 3, 4};

	ArrayC<int, 10> array1(cArray1, 10);

	EXPECT_EQ(array1.FrequencyOfElement(1), 2);
	EXPECT_EQ(array1.FrequencyOfElement(2), 1);
	EXPECT_EQ(array1.FrequencyOfElement(3), 4);
	EXPECT_EQ(array1.FrequencyOfElement(4), 2);
	EXPECT_EQ(array1.FrequencyOfElement(5), 1);
	EXPECT_EQ(array1.FrequencyOfElement(6), 0);

	unsigned int numberOfUniqueElements = 0;
	ArrayC<int, 10> arrayUnique;
	array1.UniqueElements(arrayUnique, numberOfUniqueElements);

	EXPECT_EQ(numberOfUniqueElements, 5);
	EXPECT_EQ(arrayUnique[0], 1);
	EXPECT_EQ(arrayUnique[1], 2);
	EXPECT_EQ(arrayUnique[2], 3);
	EXPECT_EQ(arrayUnique[3], 4);
	EXPECT_EQ(arrayUnique[4], 5);

	unsigned int numberOfFreqUniqueElements = 0;
	ArrayC<int, 10> arrayFreqUnique;
	ArrayC<int, 10> arrayFreq;
	array1.FrequencyUniqueElements(arrayFreqUnique, arrayFreq, numberOfFreqUniqueElements);

	EXPECT_EQ(numberOfFreqUniqueElements, 5);
	EXPECT_EQ(arrayFreqUnique[0], 1);
	EXPECT_EQ(arrayFreqUnique[1], 2);
	EXPECT_EQ(arrayFreqUnique[2], 3);
	EXPECT_EQ(arrayFreqUnique[3], 4);
	EXPECT_EQ(arrayFreqUnique[4], 5);

	EXPECT_EQ(arrayFreq[0], 2);
	EXPECT_EQ(arrayFreq[1], 1);
	EXPECT_EQ(arrayFreq[2], 4);
	EXPECT_EQ(arrayFreq[3], 2);
	EXPECT_EQ(arrayFreq[4], 1);
}

TEST(ArrayC, Sort_OrdersElementsCorrectly)
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
	int cArray2[5] = {4, 2, 5, 1, 3};

	ArrayC<int, 10> array1(cArray1, 10);
	ArrayC<int, 5> array2(cArray2, 5);

	ArrayC<int, 10> array3(cArray1, 10);
	ArrayC<int, 5> array4(cArray2, 5);

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

	array2.Sort(order);

	EXPECT_EQ(array2[0], 1);
	EXPECT_EQ(array2[1], 2);
	EXPECT_EQ(array2[2], 3);
	EXPECT_EQ(array2[3], 4);
	EXPECT_EQ(array2[4], 5);
	EXPECT_TRUE(array2.IsSorted(order));

	array3.Sort();

	EXPECT_EQ(array3[0], 1);
	EXPECT_EQ(array3[1], 1);
	EXPECT_EQ(array3[2], 2);
	EXPECT_EQ(array3[3], 3);
	EXPECT_EQ(array3[4], 3);
	EXPECT_EQ(array3[5], 3);
	EXPECT_EQ(array3[6], 3);
	EXPECT_EQ(array3[7], 4);
	EXPECT_EQ(array3[8], 4);
	EXPECT_EQ(array3[9], 5);
	EXPECT_TRUE(array3.IsSorted());

	array4.Sort();

	EXPECT_EQ(array4[0], 1);
	EXPECT_EQ(array4[1], 2);
	EXPECT_EQ(array4[2], 3);
	EXPECT_EQ(array4[3], 4);
	EXPECT_EQ(array4[4], 5);
	EXPECT_TRUE(array4.IsSorted());
}

TEST(ArrayC, Swap_ExchangesContents)
{
	int cArray1[5] = {1, 2, 3, 4, 5};
	int cArray2[5] = {4, 2, 5, 1, 3};

	ArrayC<int, 5> array1(cArray1, 5);
	ArrayC<int, 5> array2(cArray2, 5);

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

TEST(ArrayC, FindOperations_LocateElements)
{
	int cArray1[10] = {1, 2, 3, 4, 5, 6, 3, 4, 5, 7};

	ArrayC<int, 10> array1(cArray1, 10);

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

	EXPECT_EQ(array1.FindBetweenIndex(5, oneLess, 0, 7), 3);
	EXPECT_EQ(array1.FindBetweenIndex(5, oneLess, 6, 9), 7);
	EXPECT_EQ(array1.FindBetweenIndex(1, oneLess, 6, 9), -1);
}

TEST(ArrayC, FindLastOperations_LocateLastOccurrence)
{
	int cArray1[10] = {1, 2, 3, 4, 5, 6, 3, 4, 5, 7};

	ArrayC<int, 10> array1(cArray1, 10);

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

	EXPECT_EQ(array1.FindLastBetweenIndex(5, oneLess, 0, 7), 7);
	EXPECT_EQ(array1.FindLastBetweenIndex(5, oneLess, 6, 9), 7);
	EXPECT_EQ(array1.FindLastBetweenIndex(1, oneLess, 6, 9), -1);
}

TEST(ArrayC, SortedFindOperations_LocateInSortedArray)
{
	int cArray1[10] = {1, 2, 3, 4, 5, 6, 3, 4, 5, 7};
	ArrayC<int, 10> array1(cArray1, 10);

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

TEST(ArrayC, HighestEvaluationIndex_FindsMaximum)
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

	ArrayC<int, 10> array1(cArray1, 10);

	EXPECT_EQ(array1.HighestEvalutionIndex(eval), 3);
}

TEST(ArrayC, NonPrimitiveTypes_WorkCorrectly)
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

	ArrayC<Foo, 3> array1(cArray1, 3);

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
