
#include "UnitTests/Tests/Collections/UnitTestDynamicArray.h"

#include <DiaCore/Containers/Arrays/DynamicArray.h>

#include "UnitTests/Infrastructure/UnitTestMacros.h"

namespace UnitTests
{	
	UnitTestDynamicArray::UnitTestDynamicArray(const Dia::Core::Containers::String32& name)
		: UnitTestCoreContainers(name)
	{}

	UnitTestDynamicArray::UnitTestDynamicArray(void)
		: UnitTestCoreContainers()
	{}

	void UnitTestDynamicArray::DoTest()
	{
		UNIT_TEST_BLOCK_START();

			Dia::Core::Containers::DynamicArray<int> array(3);
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			bool test = (array.At(0) == 0);
			UNIT_TEST_ASSERT_EXPECTED_END();

			UNIT_TEST_POSITIVE(array.Capacity() == 3, "DynamicArray()");
			UNIT_TEST_POSITIVE(array.Size() == 0, "DynamicArray()");

		UNIT_TEST_BLOCK_END();
		
		UNIT_TEST_BLOCK_START();

			int cArray1[3] = {1, 2, 3};

			UNIT_TEST_ASSERT_EXPECTED_START();
			int* ptr = NULL;
			Dia::Core::Containers::DynamicArray<int> array0(ptr, 3);
			UNIT_TEST_ASSERT_EXPECTED_END();			
			
			Dia::Core::Containers::DynamicArray<int> array1(&cArray1[0], 3);
				
			UNIT_TEST_POSITIVE(array1.Capacity() == 3, "DynamicArray(ConstPointer pData, unsigned int _size)");
			UNIT_TEST_POSITIVE(array1.IsFull(), "DynamicArray(ConstPointer pData, unsigned int _size)");
			UNIT_TEST_POSITIVE(array1.Size() == 3, "DynamicArray(ConstPointer pData, unsigned int _size)");
			UNIT_TEST_POSITIVE(array1.At(0) == 1, "DynamicArray(ConstPointer pData, unsigned int _size)");	
			UNIT_TEST_POSITIVE(array1.At(1) == 2, "DynamicArray(ConstPointer pData, unsigned int _size)");
			UNIT_TEST_POSITIVE(array1.At(2) == 3, "DynamicArray(ConstPointer pData, unsigned int _size)");

			int cArray2[5] = {1, 2, 3, 4, 5};
			Dia::Core::Containers::DynamicArray<int> array2(&cArray2[0], 3);
			
			UNIT_TEST_POSITIVE(array2.Capacity() == 3, "DynamicArray(ConstPointer pData, unsigned int _size)");
			UNIT_TEST_POSITIVE(array2.IsFull(), "DynamicArray(ConstPointer pData, unsigned int _size)");
			UNIT_TEST_POSITIVE(array2.Size() == 3, "DynamicArray(ConstPointer pData, unsigned int _size)");
			UNIT_TEST_POSITIVE(array2.At(0) == 1, "DynamicArray(ConstPointer pData, unsigned int _size)");	
			UNIT_TEST_POSITIVE(array2.At(1) == 2, "DynamicArray(ConstPointer pData, unsigned int _size)");
			UNIT_TEST_POSITIVE(array2.At(2) == 3, "DynamicArray(ConstPointer pData, unsigned int _size)");

			int cArray3[2] = {1, 2};
			Dia::Core::Containers::DynamicArray<int> array3(&cArray3[0], 2);
				
			UNIT_TEST_POSITIVE(array3.Capacity() == 2, "DynamicArray(ConstPointer pData, unsigned int _size)");
			UNIT_TEST_POSITIVE(array3.IsFull(), "DynamicArray(ConstPointer pData, unsigned int _size)");
			UNIT_TEST_POSITIVE(array3.Size() == 2, "DynamicArray(ConstPointer pData, unsigned int _size)");
			UNIT_TEST_POSITIVE(array3.At(0) == 1, "DynamicArray(ConstPointer pData, unsigned int _size)");	
			UNIT_TEST_POSITIVE(array3.At(1) == 2, "DynamicArray(ConstPointer pData, unsigned int _size)");
		
			UNIT_TEST_ASSERT_EXPECTED_START();
			bool test = (array3.At(2) == 0);
			UNIT_TEST_ASSERT_EXPECTED_END();

		UNIT_TEST_BLOCK_END();
		
		UNIT_TEST_BLOCK_START();

			int cArray1[3] = {1, 2, 3};
			
			Dia::Core::Containers::DynamicArray<int> tempArray1(cArray1, 3);
			Dia::Core::Containers::DynamicArray<int> array1(tempArray1);
		
			UNIT_TEST_POSITIVE(array1.IsFull(), "DynamicArray(ConstPointer pData, unsigned int _size)");
			UNIT_TEST_POSITIVE(array1.Capacity() == 3, "DynamicArray(ConstPointer pData, unsigned int _size)");	
			UNIT_TEST_POSITIVE(array1.Size() == 3, "DynamicArray(ConstReference data, unsigned int _size)");
			UNIT_TEST_POSITIVE(array1.At(0) == 1, "DynamicArray(ConstReference data, unsigned int _size)");	
			UNIT_TEST_POSITIVE(array1.At(1) == 2, "DynamicArray(ConstReference data, unsigned int _size)");
			UNIT_TEST_POSITIVE(array1.At(2) == 3, "DynamicArray(ConstReference data, unsigned int _size)");

			int cArray3[2] = {1, 2};
			Dia::Core::Containers::DynamicArray<int> tempArray3(cArray3, 2);
			Dia::Core::Containers::DynamicArray<int> array3(tempArray3);

			UNIT_TEST_POSITIVE(array3.IsFull(), "DynamicArray(ConstPointer pData, unsigned int _size)");
			UNIT_TEST_POSITIVE(array3.Capacity() == 2, "DynamicArray(ConstPointer pData, unsigned int _size)");	
			UNIT_TEST_POSITIVE(array3.Size() == 2, "DynamicArray(ConstReference data, unsigned int _size)");
			UNIT_TEST_POSITIVE(array3.At(0) == 1, "DynamicArray(ConstReference data, unsigned int _size)");	
			UNIT_TEST_POSITIVE(array3.At(1) == 2, "DynamicArray(ConstReference data, unsigned int _size)");
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			bool test = (array3.At(2) == 0);
			UNIT_TEST_ASSERT_EXPECTED_END();

		UNIT_TEST_BLOCK_END();
		
		UNIT_TEST_BLOCK_START();

			int cArray1[5] = {1, 2, 3, 4, 5};
			Dia::Core::Containers::DynamicArray<int> tempArray1(cArray1, 5);
			Dia::Core::Containers::DynamicArray<int> array1(tempArray1);

			UNIT_TEST_POSITIVE(array1.IsFull(), "DynamicArray(const DynamicArray<T,_size>& rhs)");
			UNIT_TEST_POSITIVE(array1.Capacity() == 5, "DynamicArray(const DynamicArray<T,_size>& rhs)");
			UNIT_TEST_POSITIVE(array1.Size() == 5, "DynamicArray(const DynamicArray<T,_size>& rhs)");
			UNIT_TEST_POSITIVE(array1.At(0) == 1, "DynamicArray(const DynamicArray<T,_size>& rhs)");	
			UNIT_TEST_POSITIVE(array1.At(1) == 2, "DynamicArray(const DynamicArray<T,_size>& rhs)");
			UNIT_TEST_POSITIVE(array1.At(2) == 3, "DynamicArray(const DynamicArray<T,_size>& rhs)");
			UNIT_TEST_POSITIVE(array1.At(3) == 4, "DynamicArray(const DynamicArray<T,_size>& rhs)");
			UNIT_TEST_POSITIVE(array1.At(4) == 5, "DynamicArray(const DynamicArray<T,_size>& rhs)");

		UNIT_TEST_BLOCK_END();


		UNIT_TEST_BLOCK_START();

			int cArray[5] = {1, 2, 3, 4, 5};
			Dia::Core::Containers::DynamicArray<int> tempArray(cArray, 5);

			UNIT_TEST_ASSERT_EXPECTED_START();
			Dia::Core::Containers::DynamicArray<int> array1(tempArray, 2, 0);
			UNIT_TEST_ASSERT_EXPECTED_END();	
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			Dia::Core::Containers::DynamicArray<int> array1(tempArray, 5, 2);
			UNIT_TEST_ASSERT_EXPECTED_END();	

			int cArray1[5] = {1, 2, 3, 4, 5};
			Dia::Core::Containers::DynamicArray<int> tempArray1(cArray1, 5);
			Dia::Core::Containers::DynamicArray<int> array1(tempArray1, 1, 3);

			UNIT_TEST_POSITIVE(array1.IsFull(), "DynamicArray(const DynamicArray<T,_size>& rhs, unsigned int startIndex, unsigned int numberElements");
			UNIT_TEST_POSITIVE(array1.Capacity() == 3, "DynamicArray(const DynamicArray<T,_size>& rhs, unsigned int startIndex, unsigned int numberElements)");
			UNIT_TEST_POSITIVE(array1.Size() == 3, "DynamicArray(const DynamicArray<T,_size>& rhs, unsigned int startIndex, unsigned int numberElements)");
			UNIT_TEST_POSITIVE(array1.At(0) == 2, "DynamicArray(const DynamicArray<T,_size>& rhs, unsigned int startIndex, unsigned int numberElements)");	
			UNIT_TEST_POSITIVE(array1.At(1) == 3, "DynamicArray(const DynamicArray<T,_size>& rhs, unsigned int startIndex, unsigned int numberElements)");
			UNIT_TEST_POSITIVE(array1.At(2) == 4, "DynamicArray(const DynamicArray<T,_size>& rhs, unsigned int startIndex, unsigned int numberElements)");

			int cArray2[5] = {1, 2, 3, 4, 5};
			Dia::Core::Containers::DynamicArray<int> tempArray2(cArray2, 5);
			Dia::Core::Containers::DynamicArray<int> array2(tempArray2, 1, 2);

			UNIT_TEST_POSITIVE(array2.IsFull(), "DynamicArray(const DynamicArray<T,_size>& rhs, unsigned int startIndex, unsigned int numberElements");
			UNIT_TEST_POSITIVE(array2.Capacity() == 2, "DynamicArray(const DynamicArray<T,_size>& rhs, unsigned int startIndex, unsigned int numberElements)");
			UNIT_TEST_POSITIVE(array2.Size() == 2, "DynamicArray(const DynamicArray<T,_size>& rhs, unsigned int startIndex, unsigned int numberElements)");
			UNIT_TEST_POSITIVE(array2.At(0) == 2, "DynamicArray(const DynamicArray<T,_size>& rhs, unsigned int startIndex, unsigned int numberElements)");	
			UNIT_TEST_POSITIVE(array2.At(1) == 3, "DynamicArray(const DynamicArray<T,_size>& rhs, unsigned int startIndex, unsigned int numberElements)");
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			bool test = (array2.At(2) == 0);
			UNIT_TEST_ASSERT_EXPECTED_END();

		UNIT_TEST_BLOCK_END();

		UNIT_TEST_BLOCK_START();
		
			int cArray1[3] = {1, 2, 3};
			Dia::Core::Containers::DynamicArray<int> tempArray1(cArray1, 3);

			Dia::Core::Containers::DynamicArray<int>::ConstIterator iter1(tempArray1.Begin());
			Dia::Core::Containers::DynamicArray<int> array1(3, iter1);

			UNIT_TEST_POSITIVE(array1.IsFull(), "DynamicArray(Iterator begin)");
			UNIT_TEST_POSITIVE(array1.Capacity() == 3, "DynamicArray(Iterator begin)");
			UNIT_TEST_POSITIVE(array1.Size() == 3, "DynamicArray<T, size>::DynamicArray ( Iterator begin )");
			UNIT_TEST_POSITIVE(array1.At(0) == 1, "DynamicArray<T, size>::DynamicArray ( Iterator begin )");	
			UNIT_TEST_POSITIVE(array1.At(1) == 2, "DynamicArray<T, size>::DynamicArray ( Iterator begin )");
			UNIT_TEST_POSITIVE(array1.At(2) == 3, "DynamicArray<T, size>::DynamicArray ( Iterator begin )");

			int cArray2[2] = {1, 2};
			Dia::Core::Containers::DynamicArray<int> tempArray2(cArray2, 2);
			Dia::Core::Containers::DynamicArray<int> array2(3, tempArray2.BeginConst());

			UNIT_TEST_POSITIVE(!array2.IsFull(), "DynamicArray(Iterator begin)");
			UNIT_TEST_POSITIVE(array2.Capacity() == 3, "DynamicArray(Iterator begin)");
			UNIT_TEST_POSITIVE(array2.Size() == 2, "DynamicArray<T, size>::DynamicArray ( Iterator begin )");
			UNIT_TEST_POSITIVE(array2.At(0) == 1, "DynamicArray<T, size>::DynamicArray ( Iterator begin )");	
			UNIT_TEST_POSITIVE(array2.At(1) == 2, "DynamicArray<T, size>::DynamicArray ( Iterator begin )");
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			bool test = (array2.At(2) == 0);
			UNIT_TEST_ASSERT_EXPECTED_END();

		UNIT_TEST_BLOCK_END();
		
		UNIT_TEST_BLOCK_START();
		
			int cArray1[3] = {1, 2, 3};
			Dia::Core::Containers::DynamicArray<int> tempArray1(cArray1, 3);

			Dia::Core::Containers::DynamicArray<int> array1(3, tempArray1.EndConst());

			UNIT_TEST_POSITIVE(array1.IsFull(), "DynamicArray(ConstReverseIterator begin)");
			UNIT_TEST_POSITIVE(array1.Capacity() == 3, "DynamicArray(ConstReverseIterator begin)");
			UNIT_TEST_POSITIVE(array1.Size() == 3, "DynamicArray<T, size>::DynamicArray ( ConstReverseIterator iter begin )");
			UNIT_TEST_POSITIVE(array1.At(0) == 3, "DynamicArray<T, size>::DynamicArray ( ConstReverseIterator iter begin )");	
			UNIT_TEST_POSITIVE(array1.At(1) == 2, "DynamicArray<T, size>::DynamicArray ( ConstReverseIterator iter begin )");
			UNIT_TEST_POSITIVE(array1.At(2) == 1, "DynamicArray<T, size>::DynamicArray ( ConstReverseIterator iter begin )");

			int cArray2[2] = {1, 2};
			Dia::Core::Containers::DynamicArray<int> tempArray2(cArray2, 2);
			Dia::Core::Containers::DynamicArray<int> array2(3, tempArray2.EndConst());

			UNIT_TEST_POSITIVE(!array2.IsFull(), "DynamicArray(ConstReverseIterator begin)");
			UNIT_TEST_POSITIVE(array2.Capacity() == 3, "DynamicArray(ConstReverseIterator begin)");
			UNIT_TEST_POSITIVE(array2.Size() == 2, "DynamicArray<T, size>::DynamicArray ( ConstReverseIterator iter begin )");
			UNIT_TEST_POSITIVE(array2.At(0) == 2, "DynamicArray<T, size>::DynamicArray ( ConstReverseIterator iter begin )");	
			UNIT_TEST_POSITIVE(array2.At(1) == 1, "DynamicArray<T, size>::DynamicArray ( ConstReverseIterator iter begin )");
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			bool test = (array2.At(2) == 0);
			UNIT_TEST_ASSERT_EXPECTED_END();

		UNIT_TEST_BLOCK_END();

		UNIT_TEST_BLOCK_START();
		
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
			Dia::Core::Containers::DynamicArray<int> tempArray1(cArray1, 5);

			Dia::Core::Containers::DynamicArray<int> array1(5, tempArray1.BeginConst(), filter1);

			UNIT_TEST_POSITIVE(!array1.IsFull(), "DynamicArray(ConstIterator& iter, const EvaluateFunctor& filter)");
			UNIT_TEST_POSITIVE(array1.Capacity() == 5, "DynamicArray(ConstIterator& iter, const EvaluateFunctor& filter)");
			UNIT_TEST_POSITIVE(array1.Size() == 3, "DynamicArray<T, size>::DynamicArray ( ConstIterator& iter, const EvaluateFunctor& filter )");
			UNIT_TEST_POSITIVE(array1.At(0) == 3, "DynamicArray<T, size>::DynamicArray ( ConstIterator& iter, const EvaluateFunctor<T, bool>* filter )");	
			UNIT_TEST_POSITIVE(array1.At(1) == 4, "DynamicArray<T, size>::DynamicArray ( ConstIterator& iter, const EvaluateFunctor<T, bool>* filter )");
			UNIT_TEST_POSITIVE(array1.At(2) == 5, "DynamicArray<T, size>::DynamicArray ( ConstIterator& iter, const EvaluateFunctor<T, bool>* filter )");
		
			UNIT_TEST_ASSERT_EXPECTED_START();
			bool test = (array1.At(3) == 0);
			UNIT_TEST_ASSERT_EXPECTED_END();

			UNIT_TEST_ASSERT_EXPECTED_START();
			bool test = (array1.At(4) == 0);
			UNIT_TEST_ASSERT_EXPECTED_END();

		UNIT_TEST_BLOCK_END();

		UNIT_TEST_BLOCK_START();

			int cArray1[3] = {1, 2, 3};

			UNIT_TEST_ASSERT_EXPECTED_START();
			int* ptr = NULL;
			Dia::Core::Containers::DynamicArray<int> array0(3);
			array0.Assign(ptr, 3);
			UNIT_TEST_ASSERT_EXPECTED_END();			
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			Dia::Core::Containers::DynamicArray<int> array1(3);
			array1.Assign(&cArray1[0], 10);
			UNIT_TEST_ASSERT_EXPECTED_END();

			Dia::Core::Containers::DynamicArray<int> array1(3);
			array1.Assign(&cArray1[0], 3);
				
			UNIT_TEST_POSITIVE(array1.IsFull(), "DynamicArray.Assign(ConstPointer pData, unsigned int _size)");
			UNIT_TEST_POSITIVE(array1.Capacity() == 3, "DynamicArray.Assign(ConstPointer pData, unsigned int _size)");
			UNIT_TEST_POSITIVE(array1.Size() == 3, "DynamicArray.Assign(ConstPointer pData, unsigned int _size)");
			UNIT_TEST_POSITIVE(array1.At(0) == 1, "DynamicArray.Assign(ConstPointer pData, unsigned int _size)");	
			UNIT_TEST_POSITIVE(array1.At(1) == 2, "DynamicArray.Assign(ConstPointer pData, unsigned int _size)");
			UNIT_TEST_POSITIVE(array1.At(2) == 3, "DynamicArray.Assign(ConstPointer pData, unsigned int _size)");

			int cArray2[5] = {1, 2, 3, 4, 5};
			Dia::Core::Containers::DynamicArray<int> array2(3);
			array2.Assign(&cArray2[0], 3);
				
			UNIT_TEST_POSITIVE(array2.IsFull(), "DynamicArray.Assign(ConstPointer pData, unsigned int _size)");
			UNIT_TEST_POSITIVE(array2.Capacity() == 3, "DynamicArray.Assign(ConstPointer pData, unsigned int _size)");
			UNIT_TEST_POSITIVE(array2.Size() == 3, "DynamicArray.Assign(ConstPointer pData, unsigned int _size)");
			UNIT_TEST_POSITIVE(array2.At(0) == 1, "DynamicArray.Assign(ConstPointer pData, unsigned int _size)");	
			UNIT_TEST_POSITIVE(array2.At(1) == 2, "DynamicArray.Assign(ConstPointer pData, unsigned int _size)");
			UNIT_TEST_POSITIVE(array2.At(2) == 3, "DynamicArray.Assign(ConstPointer pData, unsigned int _size)");

			int cArray3[2] = {1, 2};
			Dia::Core::Containers::DynamicArray<int> array3(3);
			array3.Assign(&cArray3[0], 2);
				
			UNIT_TEST_POSITIVE(!array3.IsFull(), "DynamicArray.Assign(ConstPointer pData, unsigned int _size)");
			UNIT_TEST_POSITIVE(array3.Capacity() == 3, "DynamicArray.Assign(ConstPointer pData, unsigned int _size)");
			UNIT_TEST_POSITIVE(array3.Size() == 2, "DynamicArray.Assign(ConstPointer pData, unsigned int _size)");
			UNIT_TEST_POSITIVE(array3.At(0) == 1, "DynamicArray.Assign(ConstPointer pData, unsigned int _size)");	
			UNIT_TEST_POSITIVE(array3.At(1) == 2, "DynamicArray.Assign(ConstPointer pData, unsigned int _size)");
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			bool test = (array3.At(2) == 0);
			UNIT_TEST_ASSERT_EXPECTED_END();
		
		UNIT_TEST_BLOCK_END();
		
		UNIT_TEST_BLOCK_START();

			int cArray1[3] = {1, 2, 3};
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			Dia::Core::Containers::DynamicArray<int> array1(3);
			array1.Assign(cArray1, 10);
			UNIT_TEST_ASSERT_EXPECTED_END();
			
			Dia::Core::Containers::DynamicArray<int> tempArray1;
			tempArray1.Reserve(3);
			tempArray1.Assign(cArray1, 3);
			Dia::Core::Containers::DynamicArray<int> array1(tempArray1);
		
			UNIT_TEST_POSITIVE(array1.IsFull(), "DynamicArray.Assign(ConstReference pData, unsigned int _size)");
			UNIT_TEST_POSITIVE(array1.Capacity() == 3, "DynamicArray.Assign(ConstReference pData, unsigned int _size)");
			UNIT_TEST_POSITIVE(array1.Size() == 3, "DynamicArray.Assign(ConstReference data, unsigned int _size)");
			UNIT_TEST_POSITIVE(array1.At(0) == 1, "DynamicArray.Assign(ConstReference data, unsigned int _size)");	
			UNIT_TEST_POSITIVE(array1.At(1) == 2, "DynamicArray.Assign(ConstReference data, unsigned int _size)");
			UNIT_TEST_POSITIVE(array1.At(2) == 3, "DynamicArray.Assign(ConstReference data, unsigned int _size)");

			int cArray3[2] = {1, 2};
			Dia::Core::Containers::DynamicArray<int> tempArray3;
 			tempArray3.Reserve(3);
 			tempArray3.Assign(cArray3, 2);
 			Dia::Core::Containers::DynamicArray<int> array3(tempArray3);

			UNIT_TEST_POSITIVE(!array3.IsFull(), "DynamicArray.Assign(ConstReference pData, unsigned int _size)");
			UNIT_TEST_POSITIVE(array3.Capacity() == 3, "DynamicArray.Assign(ConstReference pData, unsigned int _size)");
			UNIT_TEST_POSITIVE(array3.Size() == 2, "DynamicArray.Assign(ConstReference data, unsigned int _size)");
			UNIT_TEST_POSITIVE(array3.At(0) == 1, "DynamicArray.Assign(ConstReference data, unsigned int _size)");	
			UNIT_TEST_POSITIVE(array3.At(1) == 2, "DynamicArray.Assign(ConstReference data, unsigned int _size)");
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			bool test = (array3.At(2) == 0);
			UNIT_TEST_ASSERT_EXPECTED_END();

		UNIT_TEST_BLOCK_END();
		
		UNIT_TEST_BLOCK_START();

			int cArray1[5] = {1, 2, 3, 4, 5};
			Dia::Core::Containers::DynamicArray<int> tempArray1(cArray1, 5);
			Dia::Core::Containers::DynamicArray<int> array1;
			array1.Reserve(5);
			array1.Assign(tempArray1);

			UNIT_TEST_POSITIVE(array1.Size() == 5, "DynamicArray(const DynamicArray<T,_size>& rhs)");
			UNIT_TEST_POSITIVE(array1.At(0) == 1, "DynamicArray.Assign(const DynamicArray<T,_size>& rhs)");	
			UNIT_TEST_POSITIVE(array1.At(1) == 2, "DynamicArray.Assign(const DynamicArray<T,_size>& rhs)");
			UNIT_TEST_POSITIVE(array1.At(2) == 3, "DynamicArray.Assign(const DynamicArray<T,_size>& rhs)");
			UNIT_TEST_POSITIVE(array1.At(3) == 4, "DynamicArray.Assign(const DynamicArray<T,_size>& rhs)");
			UNIT_TEST_POSITIVE(array1.At(4) == 5, "DynamicArray.Assign(const DynamicArray<T,_size>& rhs)");

			int cArray2[5] = {1, 2, 3, 4, 5};
			Dia::Core::Containers::DynamicArray<int> tempArray2(cArray2, 5);
			Dia::Core::Containers::DynamicArray<int> array2(3);
			array2.Assign(tempArray2);

			UNIT_TEST_POSITIVE(array2.Size() == 3, "DynamicArray(const DynamicArray<T,_size>& rhs)");
			UNIT_TEST_POSITIVE(array2.At(0) == 1, "DynamicArray.Assign(const DynamicArray<T,_size>& rhs)");	
			UNIT_TEST_POSITIVE(array2.At(1) == 2, "DynamicArray.Assign(const DynamicArray<T,_size>& rhs)");
			UNIT_TEST_POSITIVE(array2.At(2) == 3, "DynamicArray.Assign(const DynamicArray<T,_size>& rhs)");

		UNIT_TEST_BLOCK_END();

		UNIT_TEST_BLOCK_START();

			int cArray[5] = {1, 2, 3, 4, 5};
			Dia::Core::Containers::DynamicArray<int> tempArray(cArray, 5);

			UNIT_TEST_ASSERT_EXPECTED_START();
			Dia::Core::Containers::DynamicArray<int> array1(3);
			array1.Assign(tempArray, 2, 0);
			UNIT_TEST_ASSERT_EXPECTED_END();	
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			Dia::Core::Containers::DynamicArray<int> array1(3);
			array1.Assign(tempArray, 5, 2);
			UNIT_TEST_ASSERT_EXPECTED_END();	
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			Dia::Core::Containers::DynamicArray<int> array1(3);
			array1.Assign(tempArray, 2, 10);
			UNIT_TEST_ASSERT_EXPECTED_END();

			int cArray1[5] = {1, 2, 3, 4, 5};
			Dia::Core::Containers::DynamicArray<int> tempArray1(cArray1, 5);
			Dia::Core::Containers::DynamicArray<int> array1(3);
			array1.Assign(tempArray1, 1, 3);

			UNIT_TEST_POSITIVE(array1.Size() == 3, "DynamicArray.Assign(const DynamicArray<T,_size>& rhs, unsigned int startIndex, unsigned int numberElements)");
			UNIT_TEST_POSITIVE(array1.At(0) == 2, "DynamicArray.Assign(const DynamicArray<T,_size>& rhs, unsigned int startIndex, unsigned int numberElements)");	
			UNIT_TEST_POSITIVE(array1.At(1) == 3, "DynamicArray.Assign(const DynamicArray<T,_size>& rhs, unsigned int startIndex, unsigned int numberElements)");
			UNIT_TEST_POSITIVE(array1.At(2) == 4, "DynamicArray.Assign(const DynamicArray<T,_size>& rhs, unsigned int startIndex, unsigned int numberElements)");

			int cArray2[5] = {1, 2, 3, 4, 5};
			Dia::Core::Containers::DynamicArray<int> tempArray2(cArray2, 5);
			Dia::Core::Containers::DynamicArray<int> array2(3);
			array2.Assign(tempArray2, 1, 2);

			UNIT_TEST_POSITIVE(array2.Size() == 2, "DynamicArray.Assign(const DynamicArray<T,_size>& rhs, unsigned int startIndex, unsigned int numberElements)");
			UNIT_TEST_POSITIVE(array2.At(0) == 2, "DynamicArray.Assign(const DynamicArray<T,_size>& rhs, unsigned int startIndex, unsigned int numberElements)");	
			UNIT_TEST_POSITIVE(array2.At(1) == 3, "DynamicArray.Assign(const DynamicArray<T,_size>& rhs, unsigned int startIndex, unsigned int numberElements)");
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			bool test = (array2.At(2) == 0);
			UNIT_TEST_ASSERT_EXPECTED_END();

		UNIT_TEST_BLOCK_END();

		UNIT_TEST_BLOCK_START();
		
			int cArray1[3] = {1, 2, 3};
			Dia::Core::Containers::DynamicArray<int> tempArray1(cArray1, 3);

			Dia::Core::Containers::DynamicArray<int> array1(3);
			array1.Assign(tempArray1.BeginConst());

			UNIT_TEST_POSITIVE(array1.Size() == 3, "DynamicArray<T, size>::DynamicArray.Assign ( Iterator begin )");
			UNIT_TEST_POSITIVE(array1.At(0) == 1, "DynamicArray<T, size>::DynamicArray.Assign ( Iterator begin )");	
			UNIT_TEST_POSITIVE(array1.At(1) == 2, "DynamicArray<T, size>::DynamicArray.Assign ( Iterator begin )");
			UNIT_TEST_POSITIVE(array1.At(2) == 3, "DynamicArray<T, size>::DynamicArray.Assign ( Iterator begin )");

			int cArray2[2] = {1, 2};
			Dia::Core::Containers::DynamicArray<int> tempArray2(cArray2, 2);
			Dia::Core::Containers::DynamicArray<int> array2(3);
			array2.Assign(tempArray2.BeginConst());

			UNIT_TEST_POSITIVE(array2.Size() == 2, "DynamicArray<T, size>::DynamicArray.Assign ( Iterator begin )");
			UNIT_TEST_POSITIVE(array2.At(0) == 1, "DynamicArray<T, size>::DynamicArray.Assign ( Iterator begin )");	
			UNIT_TEST_POSITIVE(array2.At(1) == 2, "DynamicArray<T, size>::DynamicArray.Assign ( Iterator begin )");
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			bool test = (array2.At(2) == 0);
			UNIT_TEST_ASSERT_EXPECTED_END();

		UNIT_TEST_BLOCK_END();
		
		UNIT_TEST_BLOCK_START();
		
			int cArray1[3] = {1, 2, 3};
			Dia::Core::Containers::DynamicArray<int> tempArray1(cArray1, 3);

			Dia::Core::Containers::DynamicArray<int> array1(3);
			array1.Assign(tempArray1.EndConst());

			UNIT_TEST_POSITIVE(array1.Size() == 3, "DynamicArray<T, size>::DynamicArray.Assign ( ConstReverseIterator iter begin )");
			UNIT_TEST_POSITIVE(array1.At(0) == 3, "DynamicArray<T, size>::DynamicArray.Assign ( ConstReverseIterator iter begin )");	
			UNIT_TEST_POSITIVE(array1.At(1) == 2, "DynamicArray<T, size>::DynamicArray.Assign ( ConstReverseIterator iter begin )");
			UNIT_TEST_POSITIVE(array1.At(2) == 1, "DynamicArray<T, size>::DynamicArray.Assign ( ConstReverseIterator iter begin )");

			int cArray2[2] = {1, 2};
			Dia::Core::Containers::DynamicArray<int> tempArray2(cArray2, 2);
			Dia::Core::Containers::DynamicArray<int> array2(3);
			array2.Assign(tempArray2.EndConst());

			UNIT_TEST_POSITIVE(array2.Size() == 2, "DynamicArray<T, size>::DynamicArray.Assign ( ConstReverseIterator iter begin )");
			UNIT_TEST_POSITIVE(array2.At(0) == 2, "DynamicArray<T, size>::DynamicArray.Assign ( ConstReverseIterator iter begin )");	
			UNIT_TEST_POSITIVE(array2.At(1) == 1, "DynamicArray<T, size>::DynamicArray.Assign ( ConstReverseIterator iter begin )");
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			bool test = (array2.At(2) == 0);
			UNIT_TEST_ASSERT_EXPECTED_END();

		UNIT_TEST_BLOCK_END();
		
		UNIT_TEST_BLOCK_START();
		
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
			Dia::Core::Containers::DynamicArray<int> tempArray1(cArray1, 5);

			Dia::Core::Containers::DynamicArray<int> array1(5);
			array1.Assign(tempArray1.BeginConst(), filter1);

			UNIT_TEST_POSITIVE(array1.Size() == 3, "DynamicArray<T, size>::DynamicArray.Assign ( ConstIterator& iter, const EvaluateFunctor<T, bool>* filter )");
			UNIT_TEST_POSITIVE(array1.At(0) == 3, "DynamicArray<T, size>::DynamicArray.Assign ( ConstIterator& iter, const EvaluateFunctor<T, bool>* filter )");	
			UNIT_TEST_POSITIVE(array1.At(1) == 4, "DynamicArray<T, size>::DynamicArray.Assign ( ConstIterator& iter, const EvaluateFunctor<T, bool>* filter )");
			UNIT_TEST_POSITIVE(array1.At(2) == 5, "DynamicArray<T, size>::DynamicArray.Assign ( ConstIterator& iter, const EvaluateFunctor<T, bool>* filter )");
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			bool test = (array1.At(3) == 0);
			UNIT_TEST_ASSERT_EXPECTED_END();

			UNIT_TEST_ASSERT_EXPECTED_START();
			bool test = (array1.At(4) == 0);
			UNIT_TEST_ASSERT_EXPECTED_END();

		UNIT_TEST_BLOCK_END();
		
		UNIT_TEST_BLOCK_START();

			int cArray1[5] = {1, 2, 3, 4, 5};
			Dia::Core::Containers::DynamicArray<int> tempArray1(cArray1, 5);
			Dia::Core::Containers::DynamicArray<int> array1(5);
			array1 = tempArray1;

			UNIT_TEST_POSITIVE(array1.Size() == 5, "DynamicArray opertor=");
			UNIT_TEST_POSITIVE(array1.At(0) == 1, "DynamicArray opertor=");	
			UNIT_TEST_POSITIVE(array1.At(1) == 2, "DynamicArray opertor=");
			UNIT_TEST_POSITIVE(array1.At(2) == 3, "DynamicArray opertor=");
			UNIT_TEST_POSITIVE(array1.At(3) == 4, "DynamicArray opertor=");
			UNIT_TEST_POSITIVE(array1.At(4) == 5, "DynamicArray opertor=");

			int cArray2[5] = {1, 2, 3, 4, 5};
			Dia::Core::Containers::DynamicArray<int> tempArray2(cArray2, 5);
			Dia::Core::Containers::DynamicArray<int> array2(3);
			array2 = tempArray2;

			UNIT_TEST_POSITIVE(array2.Size() == 3, "DynamicArray opertor=");
			UNIT_TEST_POSITIVE(array2.At(0) == 1, "DynamicArray opertor=");	
			UNIT_TEST_POSITIVE(array2.At(1) == 2, "DynamicArray opertor=");
			UNIT_TEST_POSITIVE(array2.At(2) == 3, "DynamicArray opertor=");

		UNIT_TEST_BLOCK_END();

		UNIT_TEST_BLOCK_START();

			int cArray1[5] = {1, 2, 3, 4, 5};
			
			Dia::Core::Containers::DynamicArray<int> array1(cArray1, 5);
			Dia::Core::Containers::DynamicArray<int> array2(array1);
			Dia::Core::Containers::DynamicArray<int> array3(array1, 1, 3);

			UNIT_TEST_POSITIVE(array1 == array2, "DynamicArray opertor==");
			UNIT_TEST_NEGATIVE(array1 == array3, "DynamicArray opertor==");	
			UNIT_TEST_POSITIVE(array1 != array3, "DynamicArray opertor!=");
			UNIT_TEST_NEGATIVE(array1 != array2, "DynamicArray opertor!=");

		UNIT_TEST_BLOCK_END();

		UNIT_TEST_BLOCK_START();

			int cArray1[5] = {1, 2, 3, 4, 5};
			
			Dia::Core::Containers::DynamicArray<int> array1(cArray1, 5);
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			int temp = array1[-1];
			UNIT_TEST_ASSERT_EXPECTED_END();	
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			int temp = array1[6];
			UNIT_TEST_ASSERT_EXPECTED_END();
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			int temp = array1.At(-1);
			UNIT_TEST_ASSERT_EXPECTED_END();	
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			int temp = array1.At(6);
			UNIT_TEST_ASSERT_EXPECTED_END();

			UNIT_TEST_POSITIVE(array1[0] == array1.At(0), "DynamicArray opertor[]");
			UNIT_TEST_POSITIVE(array1[1] == array1.At(1), "DynamicArray opertor[]");
			UNIT_TEST_POSITIVE(array1[2] == array1.At(2), "DynamicArray opertor[]");
			UNIT_TEST_POSITIVE(array1[3] == array1.At(3), "DynamicArray opertor[]");
			UNIT_TEST_POSITIVE(array1[4] == array1.At(4), "DynamicArray opertor[]");
			
			UNIT_TEST_POSITIVE(array1.Front() == array1.At(0), "DynamicArray Front");
			UNIT_TEST_POSITIVE(array1.Back() == array1.At(4), "DynamicArray Back");
		
		UNIT_TEST_BLOCK_END();
		
		UNIT_TEST_BLOCK_START();

			int cArray1[5] = {1, 2, 3, 4, 5};
			
			Dia::Core::Containers::DynamicArray<int> array1(cArray1, 5);
			
			UNIT_TEST_POSITIVE(&array1[0] == array1.IteratorAt(0).Current(), "DynamicArray IteratorAt");
			UNIT_TEST_POSITIVE(&array1[1] == array1.IteratorAt(1).Current(), "DynamicArray IteratorAt");
			UNIT_TEST_POSITIVE(&array1[2] == array1.IteratorAt(2).Current(), "DynamicArray IteratorAt");
			UNIT_TEST_POSITIVE(&array1[3] == array1.IteratorAt(3).Current(), "DynamicArray IteratorAt");
			UNIT_TEST_POSITIVE(&array1[4] == array1.IteratorAt(4).Current(), "DynamicArray IteratorAt");
			
			UNIT_TEST_POSITIVE(array1.Begin() == array1.IteratorAt(0), "DynamicArray Begin");
			UNIT_TEST_POSITIVE(array1.BeginConst() == array1.IteratorAtConst(0), "DynamicArray BeginConst");

			UNIT_TEST_POSITIVE(&array1[0] == array1.ReverseIteratorAt(0).Current(), "DynamicArray ReverseIteratorAt");
			UNIT_TEST_POSITIVE(&array1[1] == array1.ReverseIteratorAt(1).Current(), "DynamicArray ReverseIteratorAt");
			UNIT_TEST_POSITIVE(&array1[2] == array1.ReverseIteratorAt(2).Current(), "DynamicArray ReverseIteratorAt");
			UNIT_TEST_POSITIVE(&array1[3] == array1.ReverseIteratorAt(3).Current(), "DynamicArray ReverseIteratorAt");
			UNIT_TEST_POSITIVE(&array1[4] == array1.ReverseIteratorAt(4).Current(), "DynamicArray ReverseIteratorAt");
			
			UNIT_TEST_POSITIVE(array1.End() == array1.ReverseIteratorAt(4), "DynamicArray End");
			UNIT_TEST_POSITIVE(array1.EndConst() == array1.ReverseIteratorAtConst(4), "DynamicArray EndConst");
		
		UNIT_TEST_BLOCK_END();

		UNIT_TEST_BLOCK_START();

			int cArray1[10] = {1, 2, 3, 4, 5, 1, 3, 3, 3, 4};
			
			Dia::Core::Containers::DynamicArray<int> array1(cArray1, 10);
			
			UNIT_TEST_POSITIVE(array1.FrequencyOfElement(1) == 2, "DynamicArray FrequencyOfElement");
			UNIT_TEST_POSITIVE(array1.FrequencyOfElement(2) == 1, "DynamicArray FrequencyOfElement");
			UNIT_TEST_POSITIVE(array1.FrequencyOfElement(3) == 4, "DynamicArray FrequencyOfElement");
			UNIT_TEST_POSITIVE(array1.FrequencyOfElement(4) == 2, "DynamicArray FrequencyOfElement");
			UNIT_TEST_POSITIVE(array1.FrequencyOfElement(5) == 1, "DynamicArray FrequencyOfElement");
			UNIT_TEST_POSITIVE(array1.FrequencyOfElement(6) == 0, "DynamicArray FrequencyOfElement");
			
			Dia::Core::Containers::DynamicArray<int> arrayUnique(10);
			array1.UniqueElements(arrayUnique);

			UNIT_TEST_POSITIVE(arrayUnique.Size() == 5, "DynamicArray numberOfUniqueElements");
			UNIT_TEST_POSITIVE(arrayUnique[0] == 1, "DynamicArray numberOfUniqueElements");
			UNIT_TEST_POSITIVE(arrayUnique[1] == 2, "DynamicArray numberOfUniqueElements");
			UNIT_TEST_POSITIVE(arrayUnique[2] == 3, "DynamicArray numberOfUniqueElements");
			UNIT_TEST_POSITIVE(arrayUnique[3] == 4, "DynamicArray numberOfUniqueElements");
			UNIT_TEST_POSITIVE(arrayUnique[4] == 5, "DynamicArray numberOfUniqueElements");
			
			Dia::Core::Containers::DynamicArray<int> arrayFreqUnique(10);
			Dia::Core::Containers::DynamicArray<int> arrayFreq(10);
			array1.FrequencyUniqueElements(arrayFreqUnique, arrayFreq);

			UNIT_TEST_POSITIVE(arrayFreqUnique.Size() == 5, "DynamicArray numberOfUniqueElements");
			UNIT_TEST_POSITIVE(arrayFreqUnique[0] == 1, "DynamicArray numberOfUniqueElements");
			UNIT_TEST_POSITIVE(arrayFreqUnique[1] == 2, "DynamicArray numberOfUniqueElements");
			UNIT_TEST_POSITIVE(arrayFreqUnique[2] == 3, "DynamicArray numberOfUniqueElements");
			UNIT_TEST_POSITIVE(arrayFreqUnique[3] == 4, "DynamicArray numberOfUniqueElements");
			UNIT_TEST_POSITIVE(arrayFreqUnique[4] == 5, "DynamicArray numberOfUniqueElements");

			UNIT_TEST_POSITIVE(arrayFreq[0] == 2, "DynamicArray numberOfUniqueElements");
			UNIT_TEST_POSITIVE(arrayFreq[1] == 1, "DynamicArray numberOfUniqueElements");
			UNIT_TEST_POSITIVE(arrayFreq[2] == 4, "DynamicArray numberOfUniqueElements");
			UNIT_TEST_POSITIVE(arrayFreq[3] == 2, "DynamicArray numberOfUniqueElements");
			UNIT_TEST_POSITIVE(arrayFreq[4] == 1, "DynamicArray numberOfUniqueElements");
		
		UNIT_TEST_BLOCK_END();

		UNIT_TEST_BLOCK_START();
			
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

			Dia::Core::Containers::DynamicArray<int> array1(cArray1, 10);
			Dia::Core::Containers::DynamicArray<int> array2(cArray2, 5);
			
			Dia::Core::Containers::DynamicArray<int> array3(cArray1, 10);
			Dia::Core::Containers::DynamicArray<int> array4(cArray2, 5);

			array1.Sort(order);
			
			UNIT_TEST_POSITIVE(array1[0] == 1, "DynamicArray Sort");
			UNIT_TEST_POSITIVE(array1[1] == 1, "DynamicArray Sort");
			UNIT_TEST_POSITIVE(array1[2] == 2, "DynamicArray Sort");
			UNIT_TEST_POSITIVE(array1[3] == 3, "DynamicArray Sort");
			UNIT_TEST_POSITIVE(array1[4] == 3, "DynamicArray Sort");
			UNIT_TEST_POSITIVE(array1[5] == 3, "DynamicArray Sort");
			UNIT_TEST_POSITIVE(array1[6] == 3, "DynamicArray Sort");
			UNIT_TEST_POSITIVE(array1[7] == 4, "DynamicArray Sort");
			UNIT_TEST_POSITIVE(array1[8] == 4, "DynamicArray Sort");
			UNIT_TEST_POSITIVE(array1[9] == 5, "DynamicArray Sort");
			UNIT_TEST_POSITIVE(array1.IsSorted(order), "DynamicArray Sort");

			array2.Sort(order);
			
			UNIT_TEST_POSITIVE(array2[0] == 1, "DynamicArray Sort");
			UNIT_TEST_POSITIVE(array2[1] == 2, "DynamicArray Sort");
			UNIT_TEST_POSITIVE(array2[2] == 3, "DynamicArray Sort");
			UNIT_TEST_POSITIVE(array2[3] == 4, "DynamicArray Sort");
			UNIT_TEST_POSITIVE(array2[4] == 5, "DynamicArray Sort");
			UNIT_TEST_POSITIVE(array2.IsSorted(order), "DynamicArray Sort");
				
			array3.Sort();

			UNIT_TEST_POSITIVE(array3[0] == 1, "DynamicArray Sort");
			UNIT_TEST_POSITIVE(array3[1] == 1, "DynamicArray Sort");
			UNIT_TEST_POSITIVE(array3[2] == 2, "DynamicArray Sort");
			UNIT_TEST_POSITIVE(array3[3] == 3, "DynamicArray Sort");
			UNIT_TEST_POSITIVE(array3[4] == 3, "DynamicArray Sort");
			UNIT_TEST_POSITIVE(array3[5] == 3, "DynamicArray Sort");
			UNIT_TEST_POSITIVE(array3[6] == 3, "DynamicArray Sort");
			UNIT_TEST_POSITIVE(array3[7] == 4, "DynamicArray Sort");
			UNIT_TEST_POSITIVE(array3[8] == 4, "DynamicArray Sort");
			UNIT_TEST_POSITIVE(array3[9] == 5, "DynamicArray Sort");
			UNIT_TEST_POSITIVE(array3.IsSorted(), "DynamicArray Sort");

			array4.Sort();
			
			UNIT_TEST_POSITIVE(array4[0] == 1, "DynamicArray Sort");
			UNIT_TEST_POSITIVE(array4[1] == 2, "DynamicArray Sort");
			UNIT_TEST_POSITIVE(array4[2] == 3, "DynamicArray Sort");
			UNIT_TEST_POSITIVE(array4[3] == 4, "DynamicArray Sort");
			UNIT_TEST_POSITIVE(array4[4] == 5, "DynamicArray Sort");
			UNIT_TEST_POSITIVE(array4.IsSorted(), "DynamicArray Sort");

		UNIT_TEST_BLOCK_END();
		
		UNIT_TEST_BLOCK_START();
						
			int cArray1[5] = {1, 2, 3, 4, 5};
			int cArray2[5] = {4, 2, 5, 1, 3};

			Dia::Core::Containers::DynamicArray<int> array1(cArray1, 5);
			Dia::Core::Containers::DynamicArray<int> array2(cArray2, 5);
			
			array1.Swap(array2);
			
			UNIT_TEST_POSITIVE(array1[0] == 4, "DynamicArray Swap");
			UNIT_TEST_POSITIVE(array1[1] == 2, "DynamicArray Swap");
			UNIT_TEST_POSITIVE(array1[2] == 5, "DynamicArray Swap");
			UNIT_TEST_POSITIVE(array1[3] == 1, "DynamicArray Swap");
			UNIT_TEST_POSITIVE(array1[4] == 3, "DynamicArray Swap");

			UNIT_TEST_POSITIVE(array2[0] == 1, "DynamicArray Swap");
			UNIT_TEST_POSITIVE(array2[1] == 2, "DynamicArray Swap");
			UNIT_TEST_POSITIVE(array2[2] == 3, "DynamicArray Swap");
			UNIT_TEST_POSITIVE(array2[3] == 4, "DynamicArray Swap");
			UNIT_TEST_POSITIVE(array2[4] == 5, "DynamicArray Swap");

		UNIT_TEST_BLOCK_END();

		UNIT_TEST_BLOCK_START();
						
			int cArray1[10] = {1, 2, 3, 4, 5, 6, 3, 4, 5, 7};

			Dia::Core::Containers::DynamicArray<int> array1(cArray1, 10);
			
			class OneLess
			{
			public:
				bool Equal(const int& object1, const int& object2)const
				{
					return (object1 == (object2 - 1));
				}
			};
			
			OneLess oneLess;

			UNIT_TEST_ASSERT_EXPECTED_START();
			array1.FindBetweenIndex(5, -2, 3);
			UNIT_TEST_ASSERT_EXPECTED_END();
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			array1.FindBetweenIndex(5, 3, 1);
			UNIT_TEST_ASSERT_EXPECTED_END();
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			array1.FindBetweenIndex(5, 0, 17);
			UNIT_TEST_ASSERT_EXPECTED_END();

			UNIT_TEST_POSITIVE(array1.FindIndex(5) == 4 , "DynamicArray FindIndex");
			UNIT_TEST_POSITIVE(array1.FindIndex(7) == 9, "DynamicArray FindIndex");
			UNIT_TEST_POSITIVE(array1.FindIndex(10) == -1, "DynamicArray FindIndex");

			UNIT_TEST_POSITIVE(array1.FindBetweenIndex(5, 0, 7) == 4 , "DynamicArray FindBetweenIndex");
			UNIT_TEST_POSITIVE(array1.FindBetweenIndex(5, 6, 9) == 8, "DynamicArray FindBetweenIndex");
			UNIT_TEST_POSITIVE(array1.FindBetweenIndex(1, 6, 9) == -1, "DynamicArray FindBetweenIndex");
			
			UNIT_TEST_POSITIVE(array1.FindIndex(5, oneLess) == 3 , "DynamicArray FindIndex");
			UNIT_TEST_POSITIVE(array1.FindIndex(7, oneLess) == 5, "DynamicArray FindIndex");
			UNIT_TEST_POSITIVE(array1.FindIndex(10, oneLess) == -1, "DynamicArray FindIndex");

			UNIT_TEST_POSITIVE(array1.FindBetweenIndex(5, oneLess, 0, 7) == 3 , "DynamicArray FindBetweenIndex");
			UNIT_TEST_POSITIVE(array1.FindBetweenIndex(5, oneLess, 6, 9) == 7, "DynamicArray FindBetweenIndex");
			UNIT_TEST_POSITIVE(array1.FindBetweenIndex(1, oneLess, 6, 9) == -1, "DynamicArray FindBetweenIndex");

		UNIT_TEST_BLOCK_END();
		
		UNIT_TEST_BLOCK_START();
						
			int cArray1[10] = {1, 2, 3, 4, 5, 6, 3, 4, 5, 7};

			Dia::Core::Containers::DynamicArray<int> array1(cArray1, 10);
			
			class OneLess
			{
			public:
				bool Equal(const int& object1, const int& object2)const
				{
					return (object1 == (object2 - 1));
				}
			};
			
			OneLess oneLess;

			UNIT_TEST_ASSERT_EXPECTED_START();
			array1.FindLastBetweenIndex(5, -2, 3);
			UNIT_TEST_ASSERT_EXPECTED_END();
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			array1.FindLastBetweenIndex(5, 3, 1);
			UNIT_TEST_ASSERT_EXPECTED_END();
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			array1.FindLastBetweenIndex(5, 0, 17);
			UNIT_TEST_ASSERT_EXPECTED_END();

			UNIT_TEST_POSITIVE(array1.FindLastIndex(5) == 8 , "DynamicArray FindLastIndex");
			UNIT_TEST_POSITIVE(array1.FindLastIndex(3) == 6, "DynamicArray FindLastIndex");
			UNIT_TEST_POSITIVE(array1.FindLastIndex(10) == -1, "DynamicArray FindLastIndex");

			UNIT_TEST_POSITIVE(array1.FindLastBetweenIndex(4, 0, 7) == 7 , "DynamicArray FindLastBetweenIndex");
			UNIT_TEST_POSITIVE(array1.FindLastBetweenIndex(5, 6, 9) == 8, "DynamicArray FindLastBetweenIndex");
			UNIT_TEST_POSITIVE(array1.FindLastBetweenIndex(1, 6, 9) == -1, "DynamicArray FindLastBetweenIndex");
			
			UNIT_TEST_POSITIVE(array1.FindLastIndex(5, oneLess) == 7 , "DynamicArray FindLastIndex");
			UNIT_TEST_POSITIVE(array1.FindLastIndex(7, oneLess) == 5, "DynamicArray FindLastIndex");
			UNIT_TEST_POSITIVE(array1.FindLastIndex(10, oneLess) == -1, "DynamicArray FindLastIndex");

			UNIT_TEST_POSITIVE(array1.FindLastBetweenIndex(5, oneLess, 0, 7) == 7 , "DynamicArray FindLastBetweenIndex");
			UNIT_TEST_POSITIVE(array1.FindLastBetweenIndex(5, oneLess, 6, 9) == 7, "DynamicArray FindLastBetweenIndex");
			UNIT_TEST_POSITIVE(array1.FindLastBetweenIndex(1, oneLess, 6, 9) == -1, "DynamicArray FindLastBetweenIndex");

		UNIT_TEST_BLOCK_END();
		
		UNIT_TEST_BLOCK_START();
						
			int cArray1[10] = {1, 2, 3, 4, 5, 6, 3, 4, 5, 7};

			Dia::Core::Containers::DynamicArray<int> array1(cArray1, 10);
			
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
			
			UNIT_TEST_POSITIVE(array1.IsSorted(), "DynamicArray FindSortedIndex");

			UNIT_TEST_POSITIVE(array1.FindSortedIndex(5) == 6 , "DynamicArray FindSortedIndex");
			UNIT_TEST_POSITIVE(array1.FindSortedIndex(3) == 2, "DynamicArray FindSortedIndex");
			UNIT_TEST_POSITIVE(array1.FindSortedIndex(10) == -1, "DynamicArray FindSortedIndex");

			UNIT_TEST_POSITIVE(array1.FindLastSortedIndex(4) == 5 , "DynamicArray FindLastSortedIndex");
			UNIT_TEST_POSITIVE(array1.FindLastSortedIndex(5) == 7, "DynamicArray FindLastSortedIndex");
			UNIT_TEST_POSITIVE(array1.FindLastSortedIndex(10) == -1, "DynamicArray FindLastSortedIndex");
			
			UNIT_TEST_POSITIVE(array1.FindSortedIndex(5, equality) == 6 , "DynamicArray FindSortedIndex");
			UNIT_TEST_POSITIVE(array1.FindSortedIndex(3, equality) == 2, "DynamicArray FindSortedIndex");
			UNIT_TEST_POSITIVE(array1.FindSortedIndex(10, equality) == -1, "DynamicArray FindSortedIndex");

			UNIT_TEST_POSITIVE(array1.FindLastSortedIndex(4, equality) == 5 , "DynamicArray FindLastSortedIndex");
			UNIT_TEST_POSITIVE(array1.FindLastSortedIndex(5, equality) == 7, "DynamicArray FindLastSortedIndex");
			UNIT_TEST_POSITIVE(array1.FindLastSortedIndex(10, equality) == -1, "DynamicArray FindLastSortedIndex");

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

			int cArray1[10] = {1, 2, 3, 9, 5, 6, 3, 4, 5, 7};
			
			Dia::Core::Containers::DynamicArray<int> array1(cArray1, 10);
					
			UNIT_TEST_POSITIVE(array1.HighestEvalutionIndex(eval) == 3, "DynamicArray HighestEvalutionIndex");

		UNIT_TEST_BLOCK_END();
		
		UNIT_TEST_BLOCK_START();
			
			class Foo
			{
			public:
				Foo(): mSomeData(true), mMoreData(21.0f){}

				bool SomeData(){return mSomeData;}
				float MoreData(){return mMoreData;}
			
				bool	mSomeData;
				float	mMoreData;
			};

			Foo cArray1[3];
			
			Dia::Core::Containers::DynamicArray<Foo> array1(cArray1, 3);
			
			cArray1[0].mSomeData = false;
			cArray1[0].mMoreData = 11.0f;

			UNIT_TEST_POSITIVE(array1.Size() == 3, "DynamicArray Test Non primitive");
			UNIT_TEST_POSITIVE(array1.At(0).SomeData(), "DynamicArray Test Non primitive");	
			UNIT_TEST_POSITIVE(array1.At(1).SomeData(), "DynamicArray Test Non primitive");
			UNIT_TEST_POSITIVE(array1.At(2).SomeData(), "DynamicArray Test Non primitive");

			UNIT_TEST_POSITIVE(array1.At(0).MoreData() == 21.0f, "DynamicArray Test Non primitive");	
			UNIT_TEST_POSITIVE(array1.At(1).MoreData() == 21.0f, "DynamicArray Test Non primitive");
			UNIT_TEST_POSITIVE(array1.At(2).MoreData() == 21.0f, "DynamicArray Test Non primitive");

		UNIT_TEST_BLOCK_END();

		UNIT_TEST_BLOCK_START();

			Dia::Core::Containers::DynamicArray<int> array1(3);

			array1.Add(1);
			array1.AddDefault();
			array1.Add(2);
			
			UNIT_TEST_POSITIVE(array1.Size() == 3, "DynamicArray Add");
			UNIT_TEST_POSITIVE(array1.At(0) == 1, "DynamicArray Add");	
			UNIT_TEST_POSITIVE(array1.At(1) == 0, "DynamicArray AddDefault");
			UNIT_TEST_POSITIVE(array1.At(2) == 2, "DynamicArray Add");
			
			Dia::Core::Containers::DynamicArray<int> array2(3);
			
			array2.FillDefault();

			UNIT_TEST_POSITIVE(array2.Size() == 3, "DynamicArray Add");
			UNIT_TEST_POSITIVE(array2.At(0) == 0, "DynamicArray FillDefault");	
			UNIT_TEST_POSITIVE(array2.At(1) == 0, "DynamicArray FillDefault");
			UNIT_TEST_POSITIVE(array2.At(2) == 0, "DynamicArray FillDefault");
			
			Dia::Core::Containers::DynamicArray<int> array3(3);
			array3.Fill(11);

			UNIT_TEST_POSITIVE(array3.Size() == 3, "DynamicArray Add");
			UNIT_TEST_POSITIVE(array3.At(0) == 11, "DynamicArray Fill");	
			UNIT_TEST_POSITIVE(array3.At(1) == 11, "DynamicArray Fill");
			UNIT_TEST_POSITIVE(array3.At(2) == 11, "DynamicArray Fill");
			
			Dia::Core::Containers::DynamicArray<int> array4(3);
			array4.FillDefault();

			UNIT_TEST_POSITIVE(array4.Size() == 3, "DynamicArray Add");
			UNIT_TEST_POSITIVE(array4.At(0) == 0, "DynamicArray Fill");	
			UNIT_TEST_POSITIVE(array4.At(1) == 0, "DynamicArray Fill");
			UNIT_TEST_POSITIVE(array4.At(2) == 0, "DynamicArray Fill");

			Dia::Core::Containers::DynamicArray<int> array5(5);
			array5.AddDefault();
			array5.AddAt(1, 0);
			array5.AddAt(2, 1);
			array5.AddAt(3, 1);
			array5.AddAt(4, 3);
			UNIT_TEST_POSITIVE(array5.Size() == 5, "DynamicArray Add");
			UNIT_TEST_POSITIVE(array5.At(0) == 1, "DynamicArray Fill");	
			UNIT_TEST_POSITIVE(array5.At(1) == 3, "DynamicArray Fill");
			UNIT_TEST_POSITIVE(array5.At(2) == 2, "DynamicArray Fill");
			UNIT_TEST_POSITIVE(array5.At(3) == 4, "DynamicArray Fill");
			UNIT_TEST_POSITIVE(array5.At(4) == 0, "DynamicArray Fill");
	
		UNIT_TEST_BLOCK_END();

		UNIT_TEST_BLOCK_START();

			Dia::Core::Containers::DynamicArray<int> array1(3);
			array1.Fill(11);

			array1.Remove();
			UNIT_TEST_POSITIVE(array1.Size() == 2, "DynamicArray Remove");
			UNIT_TEST_POSITIVE(array1.At(0) == 11, "DynamicArray Remove");	
			UNIT_TEST_POSITIVE(array1.At(1) == 11, "DynamicArray Remove");

			UNIT_TEST_ASSERT_EXPECTED_START();
			bool test = (array1.At(2) == 0);
			UNIT_TEST_ASSERT_EXPECTED_END();

			Dia::Core::Containers::DynamicArray<int> array2(3);
			array2.Add(1);
			array2.Add(2);
			array2.Add(3);
			array2.RemoveAt(1);
			
			UNIT_TEST_POSITIVE(array2.Size() == 2, "DynamicArray Remove");
			UNIT_TEST_POSITIVE(array2.At(0) == 1, "DynamicArray Remove");	
			UNIT_TEST_POSITIVE(array2.At(1) == 3, "DynamicArray Remove");

			UNIT_TEST_ASSERT_EXPECTED_START();
			bool test = (array2.At(2) == 0);
			UNIT_TEST_ASSERT_EXPECTED_END();
			
			Dia::Core::Containers::DynamicArray<int> array3(3);
			array3.Add(1);
			array3.Add(2);
			array3.Add(3);
			
			array3.RemoveFirst(2);
			
			UNIT_TEST_POSITIVE(array3.Size() == 2, "DynamicArray Remove");
			UNIT_TEST_POSITIVE(array3.At(0) == 1, "DynamicArray Remove");	
			UNIT_TEST_POSITIVE(array3.At(1) == 3, "DynamicArray Remove");

			UNIT_TEST_ASSERT_EXPECTED_START();
			bool test = (array3.At(2) == 0);
			UNIT_TEST_ASSERT_EXPECTED_END();
			
			Dia::Core::Containers::DynamicArray<int> array4(3);
			array4.Add(1);
			array4.Add(2);
			array4.Add(3);
			
			array4.RemoveLast(2);
			
			UNIT_TEST_POSITIVE(array4.Size() == 2, "DynamicArray Remove");
			UNIT_TEST_POSITIVE(array4.At(0) == 1, "DynamicArray Remove");	
			UNIT_TEST_POSITIVE(array4.At(1) == 3, "DynamicArray Remove");

			UNIT_TEST_ASSERT_EXPECTED_START();
			bool test = (array4.At(2) == 0);
			UNIT_TEST_ASSERT_EXPECTED_END();

			Dia::Core::Containers::DynamicArray<int> array5(3);
			array5.Add(1);
			array5.Add(2);
			array5.Add(3);
			
			array5.RemoveAll();
			
			UNIT_TEST_POSITIVE(array5.Size() == 0, "DynamicArray Remove");
			
			Dia::Core::Containers::DynamicArray<int> array6(5);
			array6.Add(1);
			array6.Add(2);
			array6.Add(3);
			array6.Add(2);
			array6.Add(3);
			
			array6.RemoveAll(2);
			
			UNIT_TEST_POSITIVE(array6.Size() == 3, "DynamicArray Remove");
			UNIT_TEST_POSITIVE(array6.At(0) == 1, "DynamicArray Remove");	
			UNIT_TEST_POSITIVE(array6.At(1) == 3, "DynamicArray Remove");
			UNIT_TEST_POSITIVE(array6.At(2) == 3, "DynamicArray Remove");

			UNIT_TEST_ASSERT_EXPECTED_START();
			bool test = (array6.At(3) == 0);
			UNIT_TEST_ASSERT_EXPECTED_END();
			
			class Comparisson
			{
			public:
				bool Evaluate(const int&a)const
				{
					return (a == 2);
				};
			};
			
			Comparisson compare;
			
			Dia::Core::Containers::DynamicArray<int> array7(5);
			array7.Add(1);
			array7.Add(2);
			array7.Add(3);
			array7.Add(2);
			array7.Add(3);
			
			array7.RemoveAll(compare);
			
			UNIT_TEST_POSITIVE(array7.Size() == 3, "DynamicArray Remove");
			UNIT_TEST_POSITIVE(array7.At(0) == 1, "DynamicArray Remove");	
			UNIT_TEST_POSITIVE(array7.At(1) == 3, "DynamicArray Remove");
			UNIT_TEST_POSITIVE(array7.At(2) == 3, "DynamicArray Remove");

			UNIT_TEST_ASSERT_EXPECTED_START();
			bool test = (array7.At(3) == 0);
			UNIT_TEST_ASSERT_EXPECTED_END();
			
		UNIT_TEST_BLOCK_END();
		
		mState = kFinished;
	}
}
