#include <gtest/gtest.h>
#include <DiaCore/Containers/Arrays/Array.h>

using namespace Dia::Core::Containers;

TEST(Array, DefaultConstruction_InitializesWithZeroValues)
{
	Array<int> array1(3);
	Array<char> array2;

	EXPECT_EQ(array1.Size(), 3);
	EXPECT_EQ(array1.At(0), 0);
}

TEST(Array, ConstructionFromPointer_CopiesData)
{
	int cArray1[3] = {1, 2, 3};

	EXPECT_DEATH(Array<int>(nullptr, 3), "");

	Array<int> array1(&cArray1[0], 3);

	EXPECT_EQ(array1.Size(), 3);
	EXPECT_EQ(array1.At(0), 1);
	EXPECT_EQ(array1.At(1), 2);
	EXPECT_EQ(array1.At(2), 3);

	int cArray2[5] = {1, 2, 3, 4, 5};
	Array<int> array2(&cArray2[0], 3);

	EXPECT_EQ(array2.Size(), 3);
	EXPECT_EQ(array2.At(0), 1);
	EXPECT_EQ(array2.At(1), 2);
	EXPECT_EQ(array2.At(2), 3);
}

TEST(Array, CopyConstruction_DuplicatesArray)
{
	int cArray1[3] = {1, 2, 3};

	Array<int> tempArray1(cArray1, 3);
	Array<int> array1(tempArray1);

	EXPECT_EQ(array1.Size(), 3);
	EXPECT_EQ(array1.At(0), 1);
	EXPECT_EQ(array1.At(1), 2);
	EXPECT_EQ(array1.At(2), 3);

	int cArray2[5] = {1, 2, 3, 4, 5};
	Array<int> tempArray2(cArray2, 5);
	Array<int> array2(tempArray2);

	EXPECT_EQ(array2.Size(), 5);
	EXPECT_EQ(array2.At(0), 1);
	EXPECT_EQ(array2.At(1), 2);
	EXPECT_EQ(array2.At(2), 3);
}

TEST(Array, ConstructionWithRange_ExtractsSubarray)
{
	int cArray[5] = {1, 2, 3, 4, 5};
	Array<int> tempArray(cArray, 5);

	EXPECT_DEATH(Array<int>(tempArray, 2, 0), "");
	EXPECT_DEATH(Array<int>(tempArray, 5, 2), "");
	EXPECT_DEATH(Array<int>(tempArray, 2, 10), "");

	Array<int> array1(tempArray, 1, 3);

	EXPECT_EQ(array1.Size(), 3);
	EXPECT_EQ(array1.At(0), 2);
	EXPECT_EQ(array1.At(1), 3);
	EXPECT_EQ(array1.At(2), 4);

	Array<int> array2(tempArray, 1, 2);

	EXPECT_EQ(array2.Size(), 2);
	EXPECT_EQ(array2.At(0), 2);
	EXPECT_EQ(array2.At(1), 3);
}

TEST(Array, ConstructionFromIterator_CopiesFromBegin)
{
	int cArray1[3] = {1, 2, 3};
	Array<int> tempArray1(cArray1, 3);

	Array<int>::ConstIterator iter1(tempArray1.Begin());
	Array<int> array1(3, iter1);

	EXPECT_EQ(array1.Size(), 3);
	EXPECT_EQ(array1.At(0), 1);
	EXPECT_EQ(array1.At(1), 2);
	EXPECT_EQ(array1.At(2), 3);

	int cArray2[2] = {1, 2};
	Array<int> tempArray2(cArray2, 2);
	Array<int> array2(2, tempArray2.BeginConst());

	EXPECT_EQ(array2.Size(), 2);
	EXPECT_EQ(array2.At(0), 1);
	EXPECT_EQ(array2.At(1), 2);
}

TEST(Array, ConstructionFromReverseIterator_CopiesInReverse)
{
	int cArray1[3] = {1, 2, 3};
	Array<int> tempArray1(cArray1, 3);

	Array<int> array1(3, tempArray1.EndConst());

	EXPECT_EQ(array1.Size(), 3);
	EXPECT_EQ(array1.At(0), 3);
	EXPECT_EQ(array1.At(1), 2);
	EXPECT_EQ(array1.At(2), 1);

	int cArray2[2] = {1, 2};
	Array<int> tempArray2(cArray2, 2);
	Array<int> array2(2, tempArray2.EndConst());

	EXPECT_EQ(array2.Size(), 2);
	EXPECT_EQ(array2.At(0), 2);
	EXPECT_EQ(array2.At(1), 1);
}

TEST(Array, ConstructionWithFilter_FiltersElements)
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
	Array<int> tempArray1(cArray1, 5);
	Array<int> array1(5, tempArray1.BeginConst(), filter1);

	EXPECT_EQ(array1.Size(), 5);
	EXPECT_EQ(array1.At(0), 3);
	EXPECT_EQ(array1.At(1), 4);
	EXPECT_EQ(array1.At(2), 5);
	EXPECT_EQ(array1.At(3), 0);
	EXPECT_EQ(array1.At(4), 0);
}

TEST(Array, AssignFromPointer_OverwritesData)
{
	int cArray1[3] = {1, 2, 3};

	EXPECT_DEATH({ Array<int> array0(3); array0.Assign(nullptr, 3); }, "");
	EXPECT_DEATH({ Array<int> array1(3); array1.Assign(&cArray1[0], 10); }, "");

	Array<int> array1(3);
	array1.Assign(&cArray1[0], 3);

	EXPECT_EQ(array1.Size(), 3);
	EXPECT_EQ(array1.At(0), 1);
	EXPECT_EQ(array1.At(1), 2);
	EXPECT_EQ(array1.At(2), 3);

	int cArray2[5] = {1, 2, 3, 4, 5};
	Array<int> array2(3);
	array2.Assign(&cArray2[0], 3);

	EXPECT_EQ(array2.Size(), 3);
	EXPECT_EQ(array2.At(0), 1);
	EXPECT_EQ(array2.At(1), 2);
	EXPECT_EQ(array2.At(2), 3);

	int cArray3[2] = {1, 2};
	Array<int> array3(3);
	array3.Assign(&cArray3[0], 2);

	EXPECT_EQ(array3.Size(), 3);
	EXPECT_EQ(array3.At(0), 1);
	EXPECT_EQ(array3.At(1), 2);
	EXPECT_EQ(array3.At(2), 0);
}

TEST(Array, AssignFromArray_CopiesData)
{
	int cArray1[5] = {1, 2, 3, 4, 5};
	Array<int> tempArray1(cArray1, 5);
	Array<int> array1(3);
	array1.Reserve(5);
	array1.Assign(tempArray1);

	EXPECT_EQ(array1.Size(), 5);
	EXPECT_EQ(array1.At(0), 1);
	EXPECT_EQ(array1.At(1), 2);
	EXPECT_EQ(array1.At(2), 3);
	EXPECT_EQ(array1.At(3), 4);
	EXPECT_EQ(array1.At(4), 5);

	int cArray2[5] = {1, 2, 3, 4, 5};
	Array<int> tempArray2(cArray2, 5);
	Array<int> array2;
	array2.Reserve(3);
	array2.Assign(tempArray2);

	EXPECT_EQ(array2.Size(), 3);
	EXPECT_EQ(array2.At(0), 1);
	EXPECT_EQ(array2.At(1), 2);
	EXPECT_EQ(array2.At(2), 3);
}

TEST(Array, AssignWithRange_CopiesSubarray)
{
	int cArray[5] = {1, 2, 3, 4, 5};
	Array<int> tempArray(cArray, 5);

	EXPECT_DEATH({ Array<int> array1(3); array1.Assign(tempArray, 2, 0); }, "");
	EXPECT_DEATH({ Array<int> array1(3); array1.Assign(tempArray, 5, 2); }, "");
	EXPECT_DEATH({ Array<int> array1(3); array1.Assign(tempArray, 2, 10); }, "");

	Array<int> array1;
	array1.Reserve(3);
	array1.Assign(tempArray, 1, 3);

	EXPECT_EQ(array1.Size(), 3);
	EXPECT_EQ(array1.At(0), 2);
	EXPECT_EQ(array1.At(1), 3);
	EXPECT_EQ(array1.At(2), 4);

	Array<int> array2;
	array2.Reserve(3);
	array2.Assign(tempArray, 1, 2);

	EXPECT_EQ(array2.Size(), 3);
	EXPECT_EQ(array2.At(0), 2);
	EXPECT_EQ(array2.At(1), 3);
	EXPECT_EQ(array2.At(2), 0);
}

TEST(Array, AssignFromIterator_CopiesFromBegin)
{
	int cArray1[3] = {1, 2, 3};
	Array<int> tempArray1(cArray1, 3);

	Array<int> array1(3);
	array1.Assign(tempArray1.BeginConst());

	EXPECT_EQ(array1.Size(), 3);
	EXPECT_EQ(array1.At(0), 1);
	EXPECT_EQ(array1.At(1), 2);
	EXPECT_EQ(array1.At(2), 3);

	int cArray2[2] = {1, 2};
	Array<int> tempArray2(cArray2, 2);
	Array<int> array2(2);
	array2.Assign(tempArray2.BeginConst());

	EXPECT_EQ(array2.Size(), 2);
	EXPECT_EQ(array2.At(0), 1);
	EXPECT_EQ(array2.At(1), 2);
}

TEST(Array, AssignFromReverseIterator_CopiesInReverse)
{
	int cArray1[3] = {1, 2, 3};
	Array<int> tempArray1(cArray1, 3);

	Array<int> array1(3);
	array1.Assign(tempArray1.EndConst());

	EXPECT_EQ(array1.Size(), 3);
	EXPECT_EQ(array1.At(0), 3);
	EXPECT_EQ(array1.At(1), 2);
	EXPECT_EQ(array1.At(2), 1);

	int cArray2[2] = {1, 2};
	Array<int> tempArray2(cArray2, 2);
	Array<int> array3(3);
	array3.Assign(tempArray2.EndConst());

	EXPECT_EQ(array3.Size(), 3);
	EXPECT_EQ(array3.At(0), 2);
	EXPECT_EQ(array3.At(1), 1);
	EXPECT_EQ(array3.At(2), 0);
}

TEST(Array, AssignWithFilter_FiltersElements)
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
	Array<int> tempArray1(cArray1, 5);

	Array<int> array1(5);
	array1.Assign(tempArray1.BeginConst(), filter1);

	EXPECT_EQ(array1.Size(), 5);
	EXPECT_EQ(array1.At(0), 3);
	EXPECT_EQ(array1.At(1), 4);
	EXPECT_EQ(array1.At(2), 5);
	EXPECT_EQ(array1.At(3), 0);
	EXPECT_EQ(array1.At(4), 0);
}

TEST(Array, AssignmentOperator_CopiesArray)
{
	int cArray1[5] = {1, 2, 3, 4, 5};
	Array<int> tempArray1(cArray1, 5);
	Array<int> array1(5);
	array1 = tempArray1;

	EXPECT_EQ(array1.Size(), 5);
	EXPECT_EQ(array1.At(0), 1);
	EXPECT_EQ(array1.At(1), 2);
	EXPECT_EQ(array1.At(2), 3);
	EXPECT_EQ(array1.At(3), 4);
	EXPECT_EQ(array1.At(4), 5);
}

TEST(Array, ComparisonOperators_CheckEquality)
{
	int cArray1[5] = {1, 2, 3, 4, 5};

	Array<int> array1(cArray1, 5);
	Array<int> array2(array1);
	Array<int> array3(array1, 1, 3);

	EXPECT_TRUE(array1 == array2);
	EXPECT_FALSE(array1 == array3);
	EXPECT_TRUE(array1 != array3);
	EXPECT_FALSE(array1 != array2);
}

TEST(Array, ElementAccess_WorksCorrectlyWithBoundsChecking)
{
	int cArray1[5] = {1, 2, 3, 4, 5};

	Array<int> array1(cArray1, 5);

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

TEST(Array, Iterators_ProvideCorrectAccess)
{
	int cArray1[5] = {1, 2, 3, 4, 5};

	Array<int> array1(cArray1, 5);

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

TEST(Array, FrequencyOperations_CalculateCorrectly)
{
	int cArray1[10] = {1, 2, 3, 4, 5, 1, 3, 3, 3, 4};

	Array<int> array1(cArray1, 10);

	EXPECT_EQ(array1.FrequencyOfElement(1), 2);
	EXPECT_EQ(array1.FrequencyOfElement(2), 1);
	EXPECT_EQ(array1.FrequencyOfElement(3), 4);
	EXPECT_EQ(array1.FrequencyOfElement(4), 2);
	EXPECT_EQ(array1.FrequencyOfElement(5), 1);
	EXPECT_EQ(array1.FrequencyOfElement(6), 0);

	unsigned int numberOfUniqueElements = 0;
	Array<int> arrayUnique(10);
	array1.UniqueElements(arrayUnique, numberOfUniqueElements);

	EXPECT_EQ(numberOfUniqueElements, 5);
	EXPECT_EQ(arrayUnique[0], 1);
	EXPECT_EQ(arrayUnique[1], 2);
	EXPECT_EQ(arrayUnique[2], 3);
	EXPECT_EQ(arrayUnique[3], 4);
	EXPECT_EQ(arrayUnique[4], 5);

	unsigned int numberOfFreqUniqueElements = 0;
	Array<int> arrayFreqUnique(10);
	Array<int> arrayFreq(10);
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

TEST(Array, Sort_OrdersElementsCorrectly)
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

	Array<int> array1(cArray1, 10);
	Array<int> array2(cArray2, 5);

	Array<int> array3(cArray1, 10);
	Array<int> array4(cArray2, 5);

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

TEST(Array, Swap_ExchangesContents)
{
	int cArray1[5] = {1, 2, 3, 4, 5};
	int cArray2[5] = {4, 2, 5, 1, 3};

	Array<int> array1(cArray1, 5);
	Array<int> array2(cArray2, 5);

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

TEST(Array, FindOperations_LocateElements)
{
	int cArray1[10] = {1, 2, 3, 4, 5, 6, 3, 4, 5, 7};

	Array<int> array1(cArray1, 10);

	class OneLess
	{
	public:
		bool Equal(const int& object1, const int& object2)const
		{
			return (object1 == (object2 - 1));
		}
	};

	OneLess oneLess;

	EXPECT_DEATH(array1.FindBetweenIndex(5, -2, 3), "");
	EXPECT_DEATH(array1.FindBetweenIndex(5, 3, 1), "");
	EXPECT_DEATH(array1.FindBetweenIndex(5, 0, 17), "");

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

TEST(Array, FindLastOperations_LocateLastOccurrence)
{
	int cArray1[10] = {1, 2, 3, 4, 5, 6, 3, 4, 5, 7};

	Array<int> array1(cArray1, 10);

	class OneLess
	{
	public:
		bool Equal(const int& object1, const int& object2)const
		{
			return (object1 == (object2 - 1));
		}
	};

	OneLess oneLess;

	EXPECT_DEATH(array1.FindLastBetweenIndex(5, -2, 3), "");
	EXPECT_DEATH(array1.FindLastBetweenIndex(5, 3, 1), "");
	EXPECT_DEATH(array1.FindLastBetweenIndex(5, 0, 17), "");

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

TEST(Array, SortedFindOperations_LocateInSortedArray)
{
	int cArray1[10] = {1, 2, 3, 4, 5, 6, 3, 4, 5, 7};
	Array<int> array1(cArray1, 10);

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

TEST(Array, HighestEvaluationIndex_FindsMaximum)
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

	Array<int> array1(cArray1, 10);

	EXPECT_EQ(array1.HighestEvalutionIndex(eval), 3);
}

TEST(Array, NonPrimitiveTypes_WorkCorrectly)
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

	Array<Foo> array1(cArray1, 3);

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
