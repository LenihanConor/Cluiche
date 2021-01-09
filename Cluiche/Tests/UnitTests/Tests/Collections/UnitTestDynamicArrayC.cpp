
#include "UnitTests/Tests/Collections/UnitTestDynamicArrayC.h"

#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

#include "UnitTests/Infrastructure/UnitTestMacros.h"

namespace UnitTests
{	
	UnitTestDynamicArrayC::UnitTestDynamicArrayC(const Dia::Core::Containers::String32& name)
		: UnitTestCoreContainers(name)
	{}

	UnitTestDynamicArrayC::UnitTestDynamicArrayC(void)
		: UnitTestCoreContainers()
	{}

	void UnitTestDynamicArrayC::DoTest()
	{
		UNIT_TEST_BLOCK_START();

			Dia::Core::Containers::DynamicArrayC<int, 3> array;
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			bool test = (array.At(0) == 0);
			UNIT_TEST_ASSERT_EXPECTED_END();

			UNIT_TEST_POSITIVE(array.Capacity() == 3, "DynamicArrayC()");
			UNIT_TEST_POSITIVE(array.Size() == 0, "DynamicArrayC()");

		UNIT_TEST_BLOCK_END();
		
		UNIT_TEST_BLOCK_START();

			int cArray1[3] = {1, 2, 3};

			UNIT_TEST_ASSERT_EXPECTED_START();
			int* ptr = NULL;
			Dia::Core::Containers::DynamicArrayC<int, 3> array0(ptr, 3);
			UNIT_TEST_ASSERT_EXPECTED_END();			
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			Dia::Core::Containers::DynamicArrayC<int, 3> array1(&cArray1[0], 10);
			UNIT_TEST_ASSERT_EXPECTED_END();

			Dia::Core::Containers::DynamicArrayC<int, 3> array1(&cArray1[0], 3);
				
			UNIT_TEST_POSITIVE(array1.Capacity() == 3, "DynamicArrayC(ConstPointer pData, unsigned int _size)");
			UNIT_TEST_POSITIVE(array1.IsFull(), "DynamicArrayC(ConstPointer pData, unsigned int _size)");
			UNIT_TEST_POSITIVE(array1.Size() == 3, "DynamicArrayC(ConstPointer pData, unsigned int _size)");
			UNIT_TEST_POSITIVE(array1.At(0) == 1, "DynamicArrayC(ConstPointer pData, unsigned int _size)");	
			UNIT_TEST_POSITIVE(array1.At(1) == 2, "DynamicArrayC(ConstPointer pData, unsigned int _size)");
			UNIT_TEST_POSITIVE(array1.At(2) == 3, "DynamicArrayC(ConstPointer pData, unsigned int _size)");

			int cArray2[5] = {1, 2, 3, 4, 5};
			Dia::Core::Containers::DynamicArrayC<int, 3> array2(&cArray2[0], 3);
			
			UNIT_TEST_POSITIVE(array2.Capacity() == 3, "DynamicArrayC(ConstPointer pData, unsigned int _size)");
			UNIT_TEST_POSITIVE(array2.IsFull(), "DynamicArrayC(ConstPointer pData, unsigned int _size)");
			UNIT_TEST_POSITIVE(array2.Size() == 3, "DynamicArrayC(ConstPointer pData, unsigned int _size)");
			UNIT_TEST_POSITIVE(array2.At(0) == 1, "DynamicArrayC(ConstPointer pData, unsigned int _size)");	
			UNIT_TEST_POSITIVE(array2.At(1) == 2, "DynamicArrayC(ConstPointer pData, unsigned int _size)");
			UNIT_TEST_POSITIVE(array2.At(2) == 3, "DynamicArrayC(ConstPointer pData, unsigned int _size)");

			int cArray3[2] = {1, 2};
			Dia::Core::Containers::DynamicArrayC<int, 3> array3(&cArray3[0], 2);
				
			UNIT_TEST_POSITIVE(array3.Capacity() == 3, "DynamicArrayC(ConstPointer pData, unsigned int _size)");
			UNIT_TEST_POSITIVE(!array3.IsFull(), "DynamicArrayC(ConstPointer pData, unsigned int _size)");
			UNIT_TEST_POSITIVE(array3.Size() == 2, "DynamicArrayC(ConstPointer pData, unsigned int _size)");
			UNIT_TEST_POSITIVE(array3.At(0) == 1, "DynamicArrayC(ConstPointer pData, unsigned int _size)");	
			UNIT_TEST_POSITIVE(array3.At(1) == 2, "DynamicArrayC(ConstPointer pData, unsigned int _size)");
		
			UNIT_TEST_ASSERT_EXPECTED_START();
			bool test = (array3.At(2) == 0);
			UNIT_TEST_ASSERT_EXPECTED_END();

		UNIT_TEST_BLOCK_END();
		
		UNIT_TEST_BLOCK_START();

			int cArray1[3] = {1, 2, 3};
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			Dia::Core::Containers::DynamicArrayC<int, 3> array1(cArray1, 10);
			UNIT_TEST_ASSERT_EXPECTED_END();
			
			Dia::Core::Containers::DynamicArrayC<int, 3> tempArray1(cArray1, 3);
			Dia::Core::Containers::DynamicArrayC<int, 3> array1(tempArray1);
		
			UNIT_TEST_POSITIVE(array1.IsFull(), "DynamicArrayC(ConstPointer pData, unsigned int _size)");
			UNIT_TEST_POSITIVE(array1.Capacity() == 3, "DynamicArrayC(ConstPointer pData, unsigned int _size)");	
			UNIT_TEST_POSITIVE(array1.Size() == 3, "DynamicArrayC(ConstReference data, unsigned int _size)");
			UNIT_TEST_POSITIVE(array1.At(0) == 1, "DynamicArrayC(ConstReference data, unsigned int _size)");	
			UNIT_TEST_POSITIVE(array1.At(1) == 2, "DynamicArrayC(ConstReference data, unsigned int _size)");
			UNIT_TEST_POSITIVE(array1.At(2) == 3, "DynamicArrayC(ConstReference data, unsigned int _size)");

			UNIT_TEST_ASSERT_EXPECTED_START();
			int cArray2[5] = {1, 2, 3, 4, 5};
			Dia::Core::Containers::DynamicArrayC<int, 5> tempArray2(cArray2, 5);
			Dia::Core::Containers::DynamicArrayC<int, 3> array2(tempArray2);
			UNIT_TEST_ASSERT_EXPECTED_END();

			int cArray3[2] = {1, 2};
			Dia::Core::Containers::DynamicArrayC<int, 2> tempArray3(cArray3, 2);
			Dia::Core::Containers::DynamicArrayC<int, 3> array3(tempArray3);

			UNIT_TEST_POSITIVE(!array3.IsFull(), "DynamicArrayC(ConstPointer pData, unsigned int _size)");
			UNIT_TEST_POSITIVE(array3.Capacity() == 3, "DynamicArrayC(ConstPointer pData, unsigned int _size)");	
			UNIT_TEST_POSITIVE(array3.Size() == 2, "DynamicArrayC(ConstReference data, unsigned int _size)");
			UNIT_TEST_POSITIVE(array3.At(0) == 1, "DynamicArrayC(ConstReference data, unsigned int _size)");	
			UNIT_TEST_POSITIVE(array3.At(1) == 2, "DynamicArrayC(ConstReference data, unsigned int _size)");
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			bool test = (array3.At(2) == 0);
			UNIT_TEST_ASSERT_EXPECTED_END();

		UNIT_TEST_BLOCK_END();
		
		UNIT_TEST_BLOCK_START();

			int cArray1[5] = {1, 2, 3, 4, 5};
			Dia::Core::Containers::DynamicArrayC<int, 5> tempArray1(cArray1, 5);
			Dia::Core::Containers::DynamicArrayC<int, 5> array1(tempArray1);

			UNIT_TEST_POSITIVE(array1.IsFull(), "DynamicArrayC(const DynamicArrayC<T,_size>& rhs)");
			UNIT_TEST_POSITIVE(array1.Capacity() == 5, "DynamicArrayC(const DynamicArrayC<T,_size>& rhs)");
			UNIT_TEST_POSITIVE(array1.Size() == 5, "DynamicArrayC(const DynamicArrayC<T,_size>& rhs)");
			UNIT_TEST_POSITIVE(array1.At(0) == 1, "DynamicArrayC(const DynamicArrayC<T,_size>& rhs)");	
			UNIT_TEST_POSITIVE(array1.At(1) == 2, "DynamicArrayC(const DynamicArrayC<T,_size>& rhs)");
			UNIT_TEST_POSITIVE(array1.At(2) == 3, "DynamicArrayC(const DynamicArrayC<T,_size>& rhs)");
			UNIT_TEST_POSITIVE(array1.At(3) == 4, "DynamicArrayC(const DynamicArrayC<T,_size>& rhs)");
			UNIT_TEST_POSITIVE(array1.At(4) == 5, "DynamicArrayC(const DynamicArrayC<T,_size>& rhs)");

			UNIT_TEST_ASSERT_EXPECTED_START();
			int cArray2[5] = {1, 2, 3, 4, 5};
			Dia::Core::Containers::DynamicArrayC<int, 5> tempArray2(cArray2, 5);
			Dia::Core::Containers::DynamicArrayC<int, 3> array2(tempArray2);
			UNIT_TEST_ASSERT_EXPECTED_END();

		UNIT_TEST_BLOCK_END();


		UNIT_TEST_BLOCK_START();

			int cArray[5] = {1, 2, 3, 4, 5};
			Dia::Core::Containers::DynamicArrayC<int, 5> tempArray(cArray, 5);

			UNIT_TEST_ASSERT_EXPECTED_START();
			Dia::Core::Containers::DynamicArrayC<int, 3> array1(tempArray, 2, 0);
			UNIT_TEST_ASSERT_EXPECTED_END();	
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			Dia::Core::Containers::DynamicArrayC<int, 3> array1(tempArray, 5, 2);
			UNIT_TEST_ASSERT_EXPECTED_END();	
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			Dia::Core::Containers::DynamicArrayC<int, 3> array1(tempArray, 2, 10);
			UNIT_TEST_ASSERT_EXPECTED_END();

			int cArray1[5] = {1, 2, 3, 4, 5};
			Dia::Core::Containers::DynamicArrayC<int, 5> tempArray1(cArray1, 5);
			Dia::Core::Containers::DynamicArrayC<int, 3> array1(tempArray1, 1, 3);

			UNIT_TEST_POSITIVE(array1.IsFull(), "DynamicArrayC(const DynamicArrayC<T,_size>& rhs, unsigned int startIndex, unsigned int numberElements");
			UNIT_TEST_POSITIVE(array1.Capacity() == 3, "DynamicArrayC(const DynamicArrayC<T,_size>& rhs, unsigned int startIndex, unsigned int numberElements)");
			UNIT_TEST_POSITIVE(array1.Size() == 3, "DynamicArrayC(const DynamicArrayC<T,_size>& rhs, unsigned int startIndex, unsigned int numberElements)");
			UNIT_TEST_POSITIVE(array1.At(0) == 2, "DynamicArrayC(const DynamicArrayC<T,_size>& rhs, unsigned int startIndex, unsigned int numberElements)");	
			UNIT_TEST_POSITIVE(array1.At(1) == 3, "DynamicArrayC(const DynamicArrayC<T,_size>& rhs, unsigned int startIndex, unsigned int numberElements)");
			UNIT_TEST_POSITIVE(array1.At(2) == 4, "DynamicArrayC(const DynamicArrayC<T,_size>& rhs, unsigned int startIndex, unsigned int numberElements)");

			int cArray2[5] = {1, 2, 3, 4, 5};
			Dia::Core::Containers::DynamicArrayC<int, 5> tempArray2(cArray2, 5);
			Dia::Core::Containers::DynamicArrayC<int, 3> array2(tempArray2, 1, 2);

			UNIT_TEST_POSITIVE(!array2.IsFull(), "DynamicArrayC(const DynamicArrayC<T,_size>& rhs, unsigned int startIndex, unsigned int numberElements");
			UNIT_TEST_POSITIVE(array2.Capacity() == 3, "DynamicArrayC(const DynamicArrayC<T,_size>& rhs, unsigned int startIndex, unsigned int numberElements)");
			UNIT_TEST_POSITIVE(array2.Size() == 2, "DynamicArrayC(const DynamicArrayC<T,_size>& rhs, unsigned int startIndex, unsigned int numberElements)");
			UNIT_TEST_POSITIVE(array2.At(0) == 2, "DynamicArrayC(const DynamicArrayC<T,_size>& rhs, unsigned int startIndex, unsigned int numberElements)");	
			UNIT_TEST_POSITIVE(array2.At(1) == 3, "DynamicArrayC(const DynamicArrayC<T,_size>& rhs, unsigned int startIndex, unsigned int numberElements)");
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			bool test = (array2.At(2) == 0);
			UNIT_TEST_ASSERT_EXPECTED_END();

		UNIT_TEST_BLOCK_END();

		UNIT_TEST_BLOCK_START();
		
			int cArray1[3] = {1, 2, 3};
			Dia::Core::Containers::DynamicArrayC<int, 3> tempArray1(cArray1, 3);

			Dia::Core::Containers::DynamicArrayC<int, 3>::ConstIterator iter1(tempArray1.Begin());
			Dia::Core::Containers::DynamicArrayC<int, 3> array1(iter1);

			UNIT_TEST_POSITIVE(array1.IsFull(), "DynamicArrayC(Iterator begin)");
			UNIT_TEST_POSITIVE(array1.Capacity() == 3, "DynamicArrayC(Iterator begin)");
			UNIT_TEST_POSITIVE(array1.Size() == 3, "DynamicArrayC<T, size>::DynamicArrayC ( Iterator begin )");
			UNIT_TEST_POSITIVE(array1.At(0) == 1, "DynamicArrayC<T, size>::DynamicArrayC ( Iterator begin )");	
			UNIT_TEST_POSITIVE(array1.At(1) == 2, "DynamicArrayC<T, size>::DynamicArrayC ( Iterator begin )");
			UNIT_TEST_POSITIVE(array1.At(2) == 3, "DynamicArrayC<T, size>::DynamicArrayC ( Iterator begin )");

			int cArray2[2] = {1, 2};
			Dia::Core::Containers::DynamicArrayC<int, 3> tempArray2(cArray2, 2);
			Dia::Core::Containers::DynamicArrayC<int, 3> array2(tempArray2.BeginConst());

			UNIT_TEST_POSITIVE(!array2.IsFull(), "DynamicArrayC(Iterator begin)");
			UNIT_TEST_POSITIVE(array2.Capacity() == 3, "DynamicArrayC(Iterator begin)");
			UNIT_TEST_POSITIVE(array2.Size() == 2, "DynamicArrayC<T, size>::DynamicArrayC ( Iterator begin )");
			UNIT_TEST_POSITIVE(array2.At(0) == 1, "DynamicArrayC<T, size>::DynamicArrayC ( Iterator begin )");	
			UNIT_TEST_POSITIVE(array2.At(1) == 2, "DynamicArrayC<T, size>::DynamicArrayC ( Iterator begin )");
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			bool test = (array2.At(2) == 0);
			UNIT_TEST_ASSERT_EXPECTED_END();

		UNIT_TEST_BLOCK_END();
		
		UNIT_TEST_BLOCK_START();
		
			int cArray1[3] = {1, 2, 3};
			Dia::Core::Containers::DynamicArrayC<int, 3> tempArray1(cArray1, 3);

			Dia::Core::Containers::DynamicArrayC<int, 3> array1(tempArray1.EndConst());

			UNIT_TEST_POSITIVE(array1.IsFull(), "DynamicArrayC(ConstReverseIterator begin)");
			UNIT_TEST_POSITIVE(array1.Capacity() == 3, "DynamicArrayC(ConstReverseIterator begin)");
			UNIT_TEST_POSITIVE(array1.Size() == 3, "DynamicArrayC<T, size>::DynamicArrayC ( ConstReverseIterator iter begin )");
			UNIT_TEST_POSITIVE(array1.At(0) == 3, "DynamicArrayC<T, size>::DynamicArrayC ( ConstReverseIterator iter begin )");	
			UNIT_TEST_POSITIVE(array1.At(1) == 2, "DynamicArrayC<T, size>::DynamicArrayC ( ConstReverseIterator iter begin )");
			UNIT_TEST_POSITIVE(array1.At(2) == 1, "DynamicArrayC<T, size>::DynamicArrayC ( ConstReverseIterator iter begin )");

			int cArray2[2] = {1, 2};
			Dia::Core::Containers::DynamicArrayC<int, 2> tempArray2(cArray2, 2);
			Dia::Core::Containers::DynamicArrayC<int, 3> array2(tempArray2.EndConst());

			UNIT_TEST_POSITIVE(!array2.IsFull(), "DynamicArrayC(ConstReverseIterator begin)");
			UNIT_TEST_POSITIVE(array2.Capacity() == 3, "DynamicArrayC(ConstReverseIterator begin)");
			UNIT_TEST_POSITIVE(array2.Size() == 2, "DynamicArrayC<T, size>::DynamicArrayC ( ConstReverseIterator iter begin )");
			UNIT_TEST_POSITIVE(array2.At(0) == 2, "DynamicArrayC<T, size>::DynamicArrayC ( ConstReverseIterator iter begin )");	
			UNIT_TEST_POSITIVE(array2.At(1) == 1, "DynamicArrayC<T, size>::DynamicArrayC ( ConstReverseIterator iter begin )");
			
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
			Dia::Core::Containers::DynamicArrayC<int, 5> tempArray1(cArray1, 5);

			Dia::Core::Containers::DynamicArrayC<int, 5> array1(tempArray1.BeginConst(), filter1);

			UNIT_TEST_POSITIVE(!array1.IsFull(), "DynamicArrayC(ConstIterator& iter, const EvaluateFunctor& filter)");
			UNIT_TEST_POSITIVE(array1.Capacity() == 5, "DynamicArrayC(ConstIterator& iter, const EvaluateFunctor& filter)");
			UNIT_TEST_POSITIVE(array1.Size() == 3, "DynamicArrayC<T, size>::DynamicArrayC ( ConstIterator& iter, const EvaluateFunctor& filter )");
			UNIT_TEST_POSITIVE(array1.At(0) == 3, "DynamicArrayC<T, size>::DynamicArrayC ( ConstIterator& iter, const EvaluateFunctor<T, bool>* filter )");	
			UNIT_TEST_POSITIVE(array1.At(1) == 4, "DynamicArrayC<T, size>::DynamicArrayC ( ConstIterator& iter, const EvaluateFunctor<T, bool>* filter )");
			UNIT_TEST_POSITIVE(array1.At(2) == 5, "DynamicArrayC<T, size>::DynamicArrayC ( ConstIterator& iter, const EvaluateFunctor<T, bool>* filter )");
		
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
			Dia::Core::Containers::DynamicArrayC<int, 3> array0;
			array0.Assign(ptr, 3);
			UNIT_TEST_ASSERT_EXPECTED_END();			
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			Dia::Core::Containers::DynamicArrayC<int, 3> array1;
			array1.Assign(&cArray1[0], 10);
			UNIT_TEST_ASSERT_EXPECTED_END();

			Dia::Core::Containers::DynamicArrayC<int, 3> array1;
			array1.Assign(&cArray1[0], 3);
				
			UNIT_TEST_POSITIVE(array1.IsFull(), "DynamicArrayC.Assign(ConstPointer pData, unsigned int _size)");
			UNIT_TEST_POSITIVE(array1.Capacity() == 3, "DynamicArrayC.Assign(ConstPointer pData, unsigned int _size)");
			UNIT_TEST_POSITIVE(array1.Size() == 3, "DynamicArrayC.Assign(ConstPointer pData, unsigned int _size)");
			UNIT_TEST_POSITIVE(array1.At(0) == 1, "DynamicArrayC.Assign(ConstPointer pData, unsigned int _size)");	
			UNIT_TEST_POSITIVE(array1.At(1) == 2, "DynamicArrayC.Assign(ConstPointer pData, unsigned int _size)");
			UNIT_TEST_POSITIVE(array1.At(2) == 3, "DynamicArrayC.Assign(ConstPointer pData, unsigned int _size)");

			int cArray2[5] = {1, 2, 3, 4, 5};
			Dia::Core::Containers::DynamicArrayC<int, 3> array2;
			array2.Assign(&cArray2[0], 3);
				
			UNIT_TEST_POSITIVE(array2.IsFull(), "DynamicArrayC.Assign(ConstPointer pData, unsigned int _size)");
			UNIT_TEST_POSITIVE(array2.Capacity() == 3, "DynamicArrayC.Assign(ConstPointer pData, unsigned int _size)");
			UNIT_TEST_POSITIVE(array2.Size() == 3, "DynamicArrayC.Assign(ConstPointer pData, unsigned int _size)");
			UNIT_TEST_POSITIVE(array2.At(0) == 1, "DynamicArrayC.Assign(ConstPointer pData, unsigned int _size)");	
			UNIT_TEST_POSITIVE(array2.At(1) == 2, "DynamicArrayC.Assign(ConstPointer pData, unsigned int _size)");
			UNIT_TEST_POSITIVE(array2.At(2) == 3, "DynamicArrayC.Assign(ConstPointer pData, unsigned int _size)");

			int cArray3[2] = {1, 2};
			Dia::Core::Containers::DynamicArrayC<int, 3> array3;
			array3.Assign(&cArray3[0], 2);
				
			UNIT_TEST_POSITIVE(!array3.IsFull(), "DynamicArrayC.Assign(ConstPointer pData, unsigned int _size)");
			UNIT_TEST_POSITIVE(array3.Capacity() == 3, "DynamicArrayC.Assign(ConstPointer pData, unsigned int _size)");
			UNIT_TEST_POSITIVE(array3.Size() == 2, "DynamicArrayC.Assign(ConstPointer pData, unsigned int _size)");
			UNIT_TEST_POSITIVE(array3.At(0) == 1, "DynamicArrayC.Assign(ConstPointer pData, unsigned int _size)");	
			UNIT_TEST_POSITIVE(array3.At(1) == 2, "DynamicArrayC.Assign(ConstPointer pData, unsigned int _size)");
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			bool test = (array3.At(2) == 0);
			UNIT_TEST_ASSERT_EXPECTED_END();
		
		UNIT_TEST_BLOCK_END();
		
		UNIT_TEST_BLOCK_START();

			int cArray1[3] = {1, 2, 3};
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			Dia::Core::Containers::DynamicArrayC<int, 3> array1;
			array1.Assign(cArray1, 10);
			UNIT_TEST_ASSERT_EXPECTED_END();
			
			Dia::Core::Containers::DynamicArrayC<int, 3> tempArray1;
			tempArray1.Assign(cArray1, 3);
			Dia::Core::Containers::DynamicArrayC<int, 3> array1(tempArray1);
		
			UNIT_TEST_POSITIVE(array1.IsFull(), "DynamicArrayC.Assign(ConstReference pData, unsigned int _size)");
			UNIT_TEST_POSITIVE(array1.Capacity() == 3, "DynamicArrayC.Assign(ConstReference pData, unsigned int _size)");
			UNIT_TEST_POSITIVE(array1.Size() == 3, "DynamicArrayC.Assign(ConstReference data, unsigned int _size)");
			UNIT_TEST_POSITIVE(array1.At(0) == 1, "DynamicArrayC.Assign(ConstReference data, unsigned int _size)");	
			UNIT_TEST_POSITIVE(array1.At(1) == 2, "DynamicArrayC.Assign(ConstReference data, unsigned int _size)");
			UNIT_TEST_POSITIVE(array1.At(2) == 3, "DynamicArrayC.Assign(ConstReference data, unsigned int _size)");

			UNIT_TEST_ASSERT_EXPECTED_START();
			int cArray2[5] = {1, 2, 3, 4, 5};
			Dia::Core::Containers::DynamicArrayC<int, 5> tempArray2;
			tempArray2.Assign(cArray2, 5);
			Dia::Core::Containers::DynamicArrayC<int, 3> array2(tempArray2);
			UNIT_TEST_ASSERT_EXPECTED_END();

			int cArray3[2] = {1, 2};
			Dia::Core::Containers::DynamicArrayC<int, 2> tempArray3;
			tempArray3.Assign(cArray3, 2);
			Dia::Core::Containers::DynamicArrayC<int, 3> array3(tempArray3);

			UNIT_TEST_POSITIVE(!array3.IsFull(), "DynamicArrayC.Assign(ConstReference pData, unsigned int _size)");
			UNIT_TEST_POSITIVE(array3.Capacity() == 3, "DynamicArrayC.Assign(ConstReference pData, unsigned int _size)");
			UNIT_TEST_POSITIVE(array3.Size() == 2, "DynamicArrayC.Assign(ConstReference data, unsigned int _size)");
			UNIT_TEST_POSITIVE(array3.At(0) == 1, "DynamicArrayC.Assign(ConstReference data, unsigned int _size)");	
			UNIT_TEST_POSITIVE(array3.At(1) == 2, "DynamicArrayC.Assign(ConstReference data, unsigned int _size)");
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			bool test = (array3.At(2) == 0);
			UNIT_TEST_ASSERT_EXPECTED_END();

		UNIT_TEST_BLOCK_END();
		
		UNIT_TEST_BLOCK_START();

			int cArray1[5] = {1, 2, 3, 4, 5};
			Dia::Core::Containers::DynamicArrayC<int, 5> tempArray1(cArray1, 5);
			Dia::Core::Containers::DynamicArrayC<int, 5> array1;
			array1.Assign(tempArray1);

			UNIT_TEST_POSITIVE(array1.Size() == 5, "DynamicArrayC(const DynamicArrayC<T,_size>& rhs)");
			UNIT_TEST_POSITIVE(array1.At(0) == 1, "DynamicArrayC.Assign(const DynamicArrayC<T,_size>& rhs)");	
			UNIT_TEST_POSITIVE(array1.At(1) == 2, "DynamicArrayC.Assign(const DynamicArrayC<T,_size>& rhs)");
			UNIT_TEST_POSITIVE(array1.At(2) == 3, "DynamicArrayC.Assign(const DynamicArrayC<T,_size>& rhs)");
			UNIT_TEST_POSITIVE(array1.At(3) == 4, "DynamicArrayC.Assign(const DynamicArrayC<T,_size>& rhs)");
			UNIT_TEST_POSITIVE(array1.At(4) == 5, "DynamicArrayC.Assign(const DynamicArrayC<T,_size>& rhs)");

			int cArray2[5] = {1, 2, 3, 4, 5};
			Dia::Core::Containers::DynamicArrayC<int, 5> tempArray2(cArray2, 5);
			Dia::Core::Containers::DynamicArrayC<int, 3> array2;
			array2.Assign(tempArray2);

			UNIT_TEST_POSITIVE(array2.Size() == 3, "DynamicArrayC(const DynamicArrayC<T,_size>& rhs)");
			UNIT_TEST_POSITIVE(array2.At(0) == 1, "DynamicArrayC.Assign(const DynamicArrayC<T,_size>& rhs)");	
			UNIT_TEST_POSITIVE(array2.At(1) == 2, "DynamicArrayC.Assign(const DynamicArrayC<T,_size>& rhs)");
			UNIT_TEST_POSITIVE(array2.At(2) == 3, "DynamicArrayC.Assign(const DynamicArrayC<T,_size>& rhs)");

		UNIT_TEST_BLOCK_END();

		UNIT_TEST_BLOCK_START();

			int cArray[5] = {1, 2, 3, 4, 5};
			Dia::Core::Containers::DynamicArrayC<int, 5> tempArray(cArray, 5);

			UNIT_TEST_ASSERT_EXPECTED_START();
			Dia::Core::Containers::DynamicArrayC<int, 3> array1;
			array1.Assign(tempArray, 2, 0);
			UNIT_TEST_ASSERT_EXPECTED_END();	
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			Dia::Core::Containers::DynamicArrayC<int, 3> array1;
			array1.Assign(tempArray, 5, 2);
			UNIT_TEST_ASSERT_EXPECTED_END();	
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			Dia::Core::Containers::DynamicArrayC<int, 3> array1;
			array1.Assign(tempArray, 2, 10);
			UNIT_TEST_ASSERT_EXPECTED_END();

			int cArray1[5] = {1, 2, 3, 4, 5};
			Dia::Core::Containers::DynamicArrayC<int, 5> tempArray1(cArray1, 5);
			Dia::Core::Containers::DynamicArrayC<int, 3> array1;
			array1.Assign(tempArray1, 1, 3);

			UNIT_TEST_POSITIVE(array1.Size() == 3, "DynamicArrayC.Assign(const DynamicArrayC<T,_size>& rhs, unsigned int startIndex, unsigned int numberElements)");
			UNIT_TEST_POSITIVE(array1.At(0) == 2, "DynamicArrayC.Assign(const DynamicArrayC<T,_size>& rhs, unsigned int startIndex, unsigned int numberElements)");	
			UNIT_TEST_POSITIVE(array1.At(1) == 3, "DynamicArrayC.Assign(const DynamicArrayC<T,_size>& rhs, unsigned int startIndex, unsigned int numberElements)");
			UNIT_TEST_POSITIVE(array1.At(2) == 4, "DynamicArrayC.Assign(const DynamicArrayC<T,_size>& rhs, unsigned int startIndex, unsigned int numberElements)");

			int cArray2[5] = {1, 2, 3, 4, 5};
			Dia::Core::Containers::DynamicArrayC<int, 5> tempArray2(cArray2, 5);
			Dia::Core::Containers::DynamicArrayC<int, 3> array2;
			array2.Assign(tempArray2, 1, 2);

			UNIT_TEST_POSITIVE(array2.Size() == 2, "DynamicArrayC.Assign(const DynamicArrayC<T,_size>& rhs, unsigned int startIndex, unsigned int numberElements)");
			UNIT_TEST_POSITIVE(array2.At(0) == 2, "DynamicArrayC.Assign(const DynamicArrayC<T,_size>& rhs, unsigned int startIndex, unsigned int numberElements)");	
			UNIT_TEST_POSITIVE(array2.At(1) == 3, "DynamicArrayC.Assign(const DynamicArrayC<T,_size>& rhs, unsigned int startIndex, unsigned int numberElements)");
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			bool test = (array2.At(2) == 0);
			UNIT_TEST_ASSERT_EXPECTED_END();

		UNIT_TEST_BLOCK_END();

		UNIT_TEST_BLOCK_START();
		
			int cArray1[3] = {1, 2, 3};
			Dia::Core::Containers::DynamicArrayC<int, 3> tempArray1(cArray1, 3);

			Dia::Core::Containers::DynamicArrayC<int, 3> array1;
			array1.Assign(tempArray1.BeginConst());

			UNIT_TEST_POSITIVE(array1.Size() == 3, "DynamicArrayC<T, size>::DynamicArrayC.Assign ( Iterator begin )");
			UNIT_TEST_POSITIVE(array1.At(0) == 1, "DynamicArrayC<T, size>::DynamicArrayC.Assign ( Iterator begin )");	
			UNIT_TEST_POSITIVE(array1.At(1) == 2, "DynamicArrayC<T, size>::DynamicArrayC.Assign ( Iterator begin )");
			UNIT_TEST_POSITIVE(array1.At(2) == 3, "DynamicArrayC<T, size>::DynamicArrayC.Assign ( Iterator begin )");

			int cArray2[2] = {1, 2};
			Dia::Core::Containers::DynamicArrayC<int, 3> tempArray2(cArray2, 2);
			Dia::Core::Containers::DynamicArrayC<int, 3> array2;
			array2.Assign(tempArray2.BeginConst());

			UNIT_TEST_POSITIVE(array2.Size() == 2, "DynamicArrayC<T, size>::DynamicArrayC.Assign ( Iterator begin )");
			UNIT_TEST_POSITIVE(array2.At(0) == 1, "DynamicArrayC<T, size>::DynamicArrayC.Assign ( Iterator begin )");	
			UNIT_TEST_POSITIVE(array2.At(1) == 2, "DynamicArrayC<T, size>::DynamicArrayC.Assign ( Iterator begin )");
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			bool test = (array2.At(2) == 0);
			UNIT_TEST_ASSERT_EXPECTED_END();

		UNIT_TEST_BLOCK_END();
		
		UNIT_TEST_BLOCK_START();
		
			int cArray1[3] = {1, 2, 3};
			Dia::Core::Containers::DynamicArrayC<int, 3> tempArray1(cArray1, 3);

			Dia::Core::Containers::DynamicArrayC<int, 3> array1;
			array1.Assign(tempArray1.EndConst());

			UNIT_TEST_POSITIVE(array1.Size() == 3, "DynamicArrayC<T, size>::DynamicArrayC.Assign ( ConstReverseIterator iter begin )");
			UNIT_TEST_POSITIVE(array1.At(0) == 3, "DynamicArrayC<T, size>::DynamicArrayC.Assign ( ConstReverseIterator iter begin )");	
			UNIT_TEST_POSITIVE(array1.At(1) == 2, "DynamicArrayC<T, size>::DynamicArrayC.Assign ( ConstReverseIterator iter begin )");
			UNIT_TEST_POSITIVE(array1.At(2) == 1, "DynamicArrayC<T, size>::DynamicArrayC.Assign ( ConstReverseIterator iter begin )");

			int cArray2[2] = {1, 2};
			Dia::Core::Containers::DynamicArrayC<int, 2> tempArray2(cArray2, 2);
			Dia::Core::Containers::DynamicArrayC<int, 3> array2;
			array2.Assign(tempArray2.EndConst());

			UNIT_TEST_POSITIVE(array2.Size() == 2, "DynamicArrayC<T, size>::DynamicArrayC.Assign ( ConstReverseIterator iter begin )");
			UNIT_TEST_POSITIVE(array2.At(0) == 2, "DynamicArrayC<T, size>::DynamicArrayC.Assign ( ConstReverseIterator iter begin )");	
			UNIT_TEST_POSITIVE(array2.At(1) == 1, "DynamicArrayC<T, size>::DynamicArrayC.Assign ( ConstReverseIterator iter begin )");
			
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
			Dia::Core::Containers::DynamicArrayC<int, 5> tempArray1(cArray1, 5);

			Dia::Core::Containers::DynamicArrayC<int, 5> array1;
			array1.Assign(tempArray1.BeginConst(), filter1);

			UNIT_TEST_POSITIVE(array1.Size() == 3, "DynamicArrayC<T, size>::DynamicArrayC.Assign ( ConstIterator& iter, const EvaluateFunctor<T, bool>* filter )");
			UNIT_TEST_POSITIVE(array1.At(0) == 3, "DynamicArrayC<T, size>::DynamicArrayC.Assign ( ConstIterator& iter, const EvaluateFunctor<T, bool>* filter )");	
			UNIT_TEST_POSITIVE(array1.At(1) == 4, "DynamicArrayC<T, size>::DynamicArrayC.Assign ( ConstIterator& iter, const EvaluateFunctor<T, bool>* filter )");
			UNIT_TEST_POSITIVE(array1.At(2) == 5, "DynamicArrayC<T, size>::DynamicArrayC.Assign ( ConstIterator& iter, const EvaluateFunctor<T, bool>* filter )");
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			bool test = (array1.At(3) == 0);
			UNIT_TEST_ASSERT_EXPECTED_END();

			UNIT_TEST_ASSERT_EXPECTED_START();
			bool test = (array1.At(4) == 0);
			UNIT_TEST_ASSERT_EXPECTED_END();

		UNIT_TEST_BLOCK_END();
		
		UNIT_TEST_BLOCK_START();

			int cArray1[5] = {1, 2, 3, 4, 5};
			Dia::Core::Containers::DynamicArrayC<int, 5> tempArray1(cArray1, 5);
			Dia::Core::Containers::DynamicArrayC<int, 5> array1 = tempArray1;

			UNIT_TEST_POSITIVE(array1.Size() == 5, "DynamicArrayC opertor=");
			UNIT_TEST_POSITIVE(array1.At(0) == 1, "DynamicArrayC opertor=");	
			UNIT_TEST_POSITIVE(array1.At(1) == 2, "DynamicArrayC opertor=");
			UNIT_TEST_POSITIVE(array1.At(2) == 3, "DynamicArrayC opertor=");
			UNIT_TEST_POSITIVE(array1.At(3) == 4, "DynamicArrayC opertor=");
			UNIT_TEST_POSITIVE(array1.At(4) == 5, "DynamicArrayC opertor=");

			int cArray2[5] = {1, 2, 3, 4, 5};
			Dia::Core::Containers::DynamicArrayC<int, 5> tempArray2(cArray2, 5);
			Dia::Core::Containers::DynamicArrayC<int, 3> array2;
			array2 = tempArray2;

			UNIT_TEST_POSITIVE(array2.Size() == 3, "DynamicArrayC opertor=");
			UNIT_TEST_POSITIVE(array2.At(0) == 1, "DynamicArrayC opertor=");	
			UNIT_TEST_POSITIVE(array2.At(1) == 2, "DynamicArrayC opertor=");
			UNIT_TEST_POSITIVE(array2.At(2) == 3, "DynamicArrayC opertor=");

		UNIT_TEST_BLOCK_END();

		UNIT_TEST_BLOCK_START();

			int cArray1[5] = {1, 2, 3, 4, 5};
			
			Dia::Core::Containers::DynamicArrayC<int, 5> array1(cArray1, 5);
			Dia::Core::Containers::DynamicArrayC<int, 5> array2(array1);
			Dia::Core::Containers::DynamicArrayC<int, 5> array3(array1, 1, 3);

			UNIT_TEST_POSITIVE(array1 == array2, "DynamicArrayC opertor==");
			UNIT_TEST_NEGATIVE(array1 == array3, "DynamicArrayC opertor==");	
			UNIT_TEST_POSITIVE(array1 != array3, "DynamicArrayC opertor!=");
			UNIT_TEST_NEGATIVE(array1 != array2, "DynamicArrayC opertor!=");

		UNIT_TEST_BLOCK_END();

		UNIT_TEST_BLOCK_START();

			int cArray1[5] = {1, 2, 3, 4, 5};
			
			Dia::Core::Containers::DynamicArrayC<int, 5> array1(cArray1, 5);
			
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

			UNIT_TEST_POSITIVE(array1[0] == array1.At(0), "DynamicArrayC opertor[]");
			UNIT_TEST_POSITIVE(array1[1] == array1.At(1), "DynamicArrayC opertor[]");
			UNIT_TEST_POSITIVE(array1[2] == array1.At(2), "DynamicArrayC opertor[]");
			UNIT_TEST_POSITIVE(array1[3] == array1.At(3), "DynamicArrayC opertor[]");
			UNIT_TEST_POSITIVE(array1[4] == array1.At(4), "DynamicArrayC opertor[]");
			
			UNIT_TEST_POSITIVE(array1.Front() == array1.At(0), "DynamicArrayC Front");
			UNIT_TEST_POSITIVE(array1.Back() == array1.At(4), "DynamicArrayC Back");
		
		UNIT_TEST_BLOCK_END();
		
		UNIT_TEST_BLOCK_START();

			int cArray1[5] = {1, 2, 3, 4, 5};
			
			Dia::Core::Containers::DynamicArrayC<int, 5> array1(cArray1, 5);
			
			UNIT_TEST_POSITIVE(&array1[0] == array1.IteratorAt(0).Current(), "DynamicArrayC IteratorAt");
			UNIT_TEST_POSITIVE(&array1[1] == array1.IteratorAt(1).Current(), "DynamicArrayC IteratorAt");
			UNIT_TEST_POSITIVE(&array1[2] == array1.IteratorAt(2).Current(), "DynamicArrayC IteratorAt");
			UNIT_TEST_POSITIVE(&array1[3] == array1.IteratorAt(3).Current(), "DynamicArrayC IteratorAt");
			UNIT_TEST_POSITIVE(&array1[4] == array1.IteratorAt(4).Current(), "DynamicArrayC IteratorAt");
			
			UNIT_TEST_POSITIVE(array1.Begin() == array1.IteratorAt(0), "DynamicArrayC Begin");
			UNIT_TEST_POSITIVE(array1.BeginConst() == array1.IteratorAtConst(0), "DynamicArrayC BeginConst");

			UNIT_TEST_POSITIVE(&array1[0] == array1.ReverseIteratorAt(0).Current(), "DynamicArrayC ReverseIteratorAt");
			UNIT_TEST_POSITIVE(&array1[1] == array1.ReverseIteratorAt(1).Current(), "DynamicArrayC ReverseIteratorAt");
			UNIT_TEST_POSITIVE(&array1[2] == array1.ReverseIteratorAt(2).Current(), "DynamicArrayC ReverseIteratorAt");
			UNIT_TEST_POSITIVE(&array1[3] == array1.ReverseIteratorAt(3).Current(), "DynamicArrayC ReverseIteratorAt");
			UNIT_TEST_POSITIVE(&array1[4] == array1.ReverseIteratorAt(4).Current(), "DynamicArrayC ReverseIteratorAt");
			
			UNIT_TEST_POSITIVE(array1.End() == array1.ReverseIteratorAt(4), "DynamicArrayC End");
			UNIT_TEST_POSITIVE(array1.EndConst() == array1.ReverseIteratorAtConst(4), "DynamicArrayC EndConst");
		
		UNIT_TEST_BLOCK_END();

		UNIT_TEST_BLOCK_START();

			int cArray1[10] = {1, 2, 3, 4, 5, 1, 3, 3, 3, 4};
			
			Dia::Core::Containers::DynamicArrayC<int, 10> array1(cArray1, 10);
			
			UNIT_TEST_POSITIVE(array1.FrequencyOfElement(1) == 2, "DynamicArrayC FrequencyOfElement");
			UNIT_TEST_POSITIVE(array1.FrequencyOfElement(2) == 1, "DynamicArrayC FrequencyOfElement");
			UNIT_TEST_POSITIVE(array1.FrequencyOfElement(3) == 4, "DynamicArrayC FrequencyOfElement");
			UNIT_TEST_POSITIVE(array1.FrequencyOfElement(4) == 2, "DynamicArrayC FrequencyOfElement");
			UNIT_TEST_POSITIVE(array1.FrequencyOfElement(5) == 1, "DynamicArrayC FrequencyOfElement");
			UNIT_TEST_POSITIVE(array1.FrequencyOfElement(6) == 0, "DynamicArrayC FrequencyOfElement");
			
			Dia::Core::Containers::DynamicArrayC<int, 10> arrayUnique;
			array1.UniqueElements(arrayUnique);

			UNIT_TEST_POSITIVE(arrayUnique.Size() == 5, "DynamicArrayC numberOfUniqueElements");
			UNIT_TEST_POSITIVE(arrayUnique[0] == 1, "DynamicArrayC numberOfUniqueElements");
			UNIT_TEST_POSITIVE(arrayUnique[1] == 2, "DynamicArrayC numberOfUniqueElements");
			UNIT_TEST_POSITIVE(arrayUnique[2] == 3, "DynamicArrayC numberOfUniqueElements");
			UNIT_TEST_POSITIVE(arrayUnique[3] == 4, "DynamicArrayC numberOfUniqueElements");
			UNIT_TEST_POSITIVE(arrayUnique[4] == 5, "DynamicArrayC numberOfUniqueElements");
			
			Dia::Core::Containers::DynamicArrayC<int, 10> arrayFreqUnique;
			Dia::Core::Containers::DynamicArrayC<int, 10> arrayFreq;
			array1.FrequencyUniqueElements(arrayFreqUnique, arrayFreq);

			UNIT_TEST_POSITIVE(arrayFreqUnique.Size() == 5, "DynamicArrayC numberOfUniqueElements");
			UNIT_TEST_POSITIVE(arrayFreqUnique[0] == 1, "DynamicArrayC numberOfUniqueElements");
			UNIT_TEST_POSITIVE(arrayFreqUnique[1] == 2, "DynamicArrayC numberOfUniqueElements");
			UNIT_TEST_POSITIVE(arrayFreqUnique[2] == 3, "DynamicArrayC numberOfUniqueElements");
			UNIT_TEST_POSITIVE(arrayFreqUnique[3] == 4, "DynamicArrayC numberOfUniqueElements");
			UNIT_TEST_POSITIVE(arrayFreqUnique[4] == 5, "DynamicArrayC numberOfUniqueElements");

			UNIT_TEST_POSITIVE(arrayFreq[0] == 2, "DynamicArrayC numberOfUniqueElements");
			UNIT_TEST_POSITIVE(arrayFreq[1] == 1, "DynamicArrayC numberOfUniqueElements");
			UNIT_TEST_POSITIVE(arrayFreq[2] == 4, "DynamicArrayC numberOfUniqueElements");
			UNIT_TEST_POSITIVE(arrayFreq[3] == 2, "DynamicArrayC numberOfUniqueElements");
			UNIT_TEST_POSITIVE(arrayFreq[4] == 1, "DynamicArrayC numberOfUniqueElements");
		
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

			Dia::Core::Containers::DynamicArrayC<int, 10> array1(cArray1, 10);
			Dia::Core::Containers::DynamicArrayC<int, 5> array2(cArray2, 5);
			
			Dia::Core::Containers::DynamicArrayC<int, 10> array3(cArray1, 10);
			Dia::Core::Containers::DynamicArrayC<int, 5> array4(cArray2, 5);

			array1.Sort(order);
			
			UNIT_TEST_POSITIVE(array1[0] == 1, "DynamicArrayC Sort");
			UNIT_TEST_POSITIVE(array1[1] == 1, "DynamicArrayC Sort");
			UNIT_TEST_POSITIVE(array1[2] == 2, "DynamicArrayC Sort");
			UNIT_TEST_POSITIVE(array1[3] == 3, "DynamicArrayC Sort");
			UNIT_TEST_POSITIVE(array1[4] == 3, "DynamicArrayC Sort");
			UNIT_TEST_POSITIVE(array1[5] == 3, "DynamicArrayC Sort");
			UNIT_TEST_POSITIVE(array1[6] == 3, "DynamicArrayC Sort");
			UNIT_TEST_POSITIVE(array1[7] == 4, "DynamicArrayC Sort");
			UNIT_TEST_POSITIVE(array1[8] == 4, "DynamicArrayC Sort");
			UNIT_TEST_POSITIVE(array1[9] == 5, "DynamicArrayC Sort");
			UNIT_TEST_POSITIVE(array1.IsSorted(order), "DynamicArrayC Sort");

			array2.Sort(order);
			
			UNIT_TEST_POSITIVE(array2[0] == 1, "DynamicArrayC Sort");
			UNIT_TEST_POSITIVE(array2[1] == 2, "DynamicArrayC Sort");
			UNIT_TEST_POSITIVE(array2[2] == 3, "DynamicArrayC Sort");
			UNIT_TEST_POSITIVE(array2[3] == 4, "DynamicArrayC Sort");
			UNIT_TEST_POSITIVE(array2[4] == 5, "DynamicArrayC Sort");
			UNIT_TEST_POSITIVE(array2.IsSorted(order), "DynamicArrayC Sort");
				
			array3.Sort();

			UNIT_TEST_POSITIVE(array3[0] == 1, "DynamicArrayC Sort");
			UNIT_TEST_POSITIVE(array3[1] == 1, "DynamicArrayC Sort");
			UNIT_TEST_POSITIVE(array3[2] == 2, "DynamicArrayC Sort");
			UNIT_TEST_POSITIVE(array3[3] == 3, "DynamicArrayC Sort");
			UNIT_TEST_POSITIVE(array3[4] == 3, "DynamicArrayC Sort");
			UNIT_TEST_POSITIVE(array3[5] == 3, "DynamicArrayC Sort");
			UNIT_TEST_POSITIVE(array3[6] == 3, "DynamicArrayC Sort");
			UNIT_TEST_POSITIVE(array3[7] == 4, "DynamicArrayC Sort");
			UNIT_TEST_POSITIVE(array3[8] == 4, "DynamicArrayC Sort");
			UNIT_TEST_POSITIVE(array3[9] == 5, "DynamicArrayC Sort");
			UNIT_TEST_POSITIVE(array3.IsSorted(), "DynamicArrayC Sort");

			array4.Sort();
			
			UNIT_TEST_POSITIVE(array4[0] == 1, "DynamicArrayC Sort");
			UNIT_TEST_POSITIVE(array4[1] == 2, "DynamicArrayC Sort");
			UNIT_TEST_POSITIVE(array4[2] == 3, "DynamicArrayC Sort");
			UNIT_TEST_POSITIVE(array4[3] == 4, "DynamicArrayC Sort");
			UNIT_TEST_POSITIVE(array4[4] == 5, "DynamicArrayC Sort");
			UNIT_TEST_POSITIVE(array4.IsSorted(), "DynamicArrayC Sort");

		UNIT_TEST_BLOCK_END();
		
		UNIT_TEST_BLOCK_START();
						
			int cArray1[5] = {1, 2, 3, 4, 5};
			int cArray2[5] = {4, 2, 5, 1, 3};

			Dia::Core::Containers::DynamicArrayC<int, 5> array1(cArray1, 5);
			Dia::Core::Containers::DynamicArrayC<int, 5> array2(cArray2, 5);
			
			array1.Swap(array2);
			
			UNIT_TEST_POSITIVE(array1[0] == 4, "DynamicArrayC Swap");
			UNIT_TEST_POSITIVE(array1[1] == 2, "DynamicArrayC Swap");
			UNIT_TEST_POSITIVE(array1[2] == 5, "DynamicArrayC Swap");
			UNIT_TEST_POSITIVE(array1[3] == 1, "DynamicArrayC Swap");
			UNIT_TEST_POSITIVE(array1[4] == 3, "DynamicArrayC Swap");

			UNIT_TEST_POSITIVE(array2[0] == 1, "DynamicArrayC Swap");
			UNIT_TEST_POSITIVE(array2[1] == 2, "DynamicArrayC Swap");
			UNIT_TEST_POSITIVE(array2[2] == 3, "DynamicArrayC Swap");
			UNIT_TEST_POSITIVE(array2[3] == 4, "DynamicArrayC Swap");
			UNIT_TEST_POSITIVE(array2[4] == 5, "DynamicArrayC Swap");

		UNIT_TEST_BLOCK_END();

		UNIT_TEST_BLOCK_START();
						
			int cArray1[10] = {1, 2, 3, 4, 5, 6, 3, 4, 5, 7};

			Dia::Core::Containers::DynamicArrayC<int, 10> array1(cArray1, 10);
			
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

			UNIT_TEST_POSITIVE(array1.FindIndex(5) == 4 , "DynamicArrayC FindIndex");
			UNIT_TEST_POSITIVE(array1.FindIndex(7) == 9, "DynamicArrayC FindIndex");
			UNIT_TEST_POSITIVE(array1.FindIndex(10) == -1, "DynamicArrayC FindIndex");

			UNIT_TEST_POSITIVE(array1.FindBetweenIndex(5, 0, 7) == 4 , "DynamicArrayC FindBetweenIndex");
			UNIT_TEST_POSITIVE(array1.FindBetweenIndex(5, 6, 9) == 8, "DynamicArrayC FindBetweenIndex");
			UNIT_TEST_POSITIVE(array1.FindBetweenIndex(1, 6, 9) == -1, "DynamicArrayC FindBetweenIndex");
			
			UNIT_TEST_POSITIVE(array1.FindIndex(5, oneLess) == 3 , "DynamicArrayC FindIndex");
			UNIT_TEST_POSITIVE(array1.FindIndex(7, oneLess) == 5, "DynamicArrayC FindIndex");
			UNIT_TEST_POSITIVE(array1.FindIndex(10, oneLess) == -1, "DynamicArrayC FindIndex");

			UNIT_TEST_POSITIVE(array1.FindBetweenIndex(5, oneLess, 0, 7) == 3 , "DynamicArrayC FindBetweenIndex");
			UNIT_TEST_POSITIVE(array1.FindBetweenIndex(5, oneLess, 6, 9) == 7, "DynamicArrayC FindBetweenIndex");
			UNIT_TEST_POSITIVE(array1.FindBetweenIndex(1, oneLess, 6, 9) == -1, "DynamicArrayC FindBetweenIndex");

		UNIT_TEST_BLOCK_END();
		
		UNIT_TEST_BLOCK_START();
						
			int cArray1[10] = {1, 2, 3, 4, 5, 6, 3, 4, 5, 7};

			Dia::Core::Containers::DynamicArrayC<int, 10> array1(cArray1, 10);
			
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

			UNIT_TEST_POSITIVE(array1.FindLastIndex(5) == 8 , "DynamicArrayC FindLastIndex");
			UNIT_TEST_POSITIVE(array1.FindLastIndex(3) == 6, "DynamicArrayC FindLastIndex");
			UNIT_TEST_POSITIVE(array1.FindLastIndex(10) == -1, "DynamicArrayC FindLastIndex");

			UNIT_TEST_POSITIVE(array1.FindLastBetweenIndex(4, 0, 7) == 7 , "DynamicArrayC FindLastBetweenIndex");
			UNIT_TEST_POSITIVE(array1.FindLastBetweenIndex(5, 6, 9) == 8, "DynamicArrayC FindLastBetweenIndex");
			UNIT_TEST_POSITIVE(array1.FindLastBetweenIndex(1, 6, 9) == -1, "DynamicArrayC FindLastBetweenIndex");
			
			UNIT_TEST_POSITIVE(array1.FindLastIndex(5, oneLess) == 7 , "DynamicArrayC FindLastIndex");
			UNIT_TEST_POSITIVE(array1.FindLastIndex(7, oneLess) == 5, "DynamicArrayC FindLastIndex");
			UNIT_TEST_POSITIVE(array1.FindLastIndex(10, oneLess) == -1, "DynamicArrayC FindLastIndex");

			UNIT_TEST_POSITIVE(array1.FindLastBetweenIndex(5, oneLess, 0, 7) == 7 , "DynamicArrayC FindLastBetweenIndex");
			UNIT_TEST_POSITIVE(array1.FindLastBetweenIndex(5, oneLess, 6, 9) == 7, "DynamicArrayC FindLastBetweenIndex");
			UNIT_TEST_POSITIVE(array1.FindLastBetweenIndex(1, oneLess, 6, 9) == -1, "DynamicArrayC FindLastBetweenIndex");

		UNIT_TEST_BLOCK_END();
		
		UNIT_TEST_BLOCK_START();
						
			int cArray1[10] = {1, 2, 3, 4, 5, 6, 3, 4, 5, 7};
			// Sorted =		  {1, 2, 3, 3, 4, 4, 5, 5, 6, 7}
			Dia::Core::Containers::DynamicArrayC<int, 10> array1(cArray1, 10);
			
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
			
			UNIT_TEST_POSITIVE(array1.IsSorted(), "DynamicArrayC FindSortedIndex");

			UNIT_TEST_POSITIVE(array1.FindSortedIndex(5) == 6 , "DynamicArrayC FindSortedIndex");
			UNIT_TEST_POSITIVE(array1.FindSortedIndex(3) == 2, "DynamicArrayC FindSortedIndex");
			UNIT_TEST_POSITIVE(array1.FindSortedIndex(10) == -1, "DynamicArrayC FindSortedIndex");

			UNIT_TEST_POSITIVE(array1.FindLastSortedIndex(4) == 5 , "DynamicArrayC FindLastSortedIndex");
			UNIT_TEST_POSITIVE(array1.FindLastSortedIndex(5) == 7, "DynamicArrayC FindLastSortedIndex");
			UNIT_TEST_POSITIVE(array1.FindLastSortedIndex(10) == -1, "DynamicArrayC FindLastSortedIndex");
			
			UNIT_TEST_POSITIVE(array1.FindSortedIndex(5, equality) == 6 , "DynamicArrayC FindSortedIndex");
			UNIT_TEST_POSITIVE(array1.FindSortedIndex(3, equality) == 2, "DynamicArrayC FindSortedIndex");
			UNIT_TEST_POSITIVE(array1.FindSortedIndex(10, equality) == -1, "DynamicArrayC FindSortedIndex");

			UNIT_TEST_POSITIVE(array1.FindLastSortedIndex(4, equality) == 5 , "DynamicArrayC FindLastSortedIndex");
			UNIT_TEST_POSITIVE(array1.FindLastSortedIndex(5, equality) == 7, "DynamicArrayC FindLastSortedIndex");
			UNIT_TEST_POSITIVE(array1.FindLastSortedIndex(10, equality) == -1, "DynamicArrayC FindLastSortedIndex");

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
			
			Dia::Core::Containers::DynamicArrayC<int, 10> array1(cArray1, 10);
					
			UNIT_TEST_POSITIVE(array1.HighestEvalutionIndex(eval) == 3, "DynamicArrayC HighestEvalutionIndex");

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
			
			Dia::Core::Containers::DynamicArrayC<Foo, 3> array1(cArray1, 3);
			
			cArray1[0].mSomeData = false;
			cArray1[0].mMoreData = 11.0f;

			UNIT_TEST_POSITIVE(array1.Size() == 3, "DynamicArrayC Test Non primitive");
			UNIT_TEST_POSITIVE(array1.At(0).SomeData(), "DynamicArrayC Test Non primitive");	
			UNIT_TEST_POSITIVE(array1.At(1).SomeData(), "DynamicArrayC Test Non primitive");
			UNIT_TEST_POSITIVE(array1.At(2).SomeData(), "DynamicArrayC Test Non primitive");

			UNIT_TEST_POSITIVE(array1.At(0).MoreData() == 21.0f, "DynamicArrayC Test Non primitive");	
			UNIT_TEST_POSITIVE(array1.At(1).MoreData() == 21.0f, "DynamicArrayC Test Non primitive");
			UNIT_TEST_POSITIVE(array1.At(2).MoreData() == 21.0f, "DynamicArrayC Test Non primitive");

		UNIT_TEST_BLOCK_END();

		UNIT_TEST_BLOCK_START();

			Dia::Core::Containers::DynamicArrayC<int, 3> array1;

			array1.Add(1);
			array1.AddDefault();
			array1.Add(2);
			
			UNIT_TEST_POSITIVE(array1.Size() == 3, "DynamicArrayC Add");
			UNIT_TEST_POSITIVE(array1.At(0) == 1, "DynamicArrayC Add");	
			UNIT_TEST_POSITIVE(array1.At(1) == 0, "DynamicArrayC AddDefault");
			UNIT_TEST_POSITIVE(array1.At(2) == 2, "DynamicArrayC Add");
			
			Dia::Core::Containers::DynamicArrayC<int, 3> array2;
			
			array2.FillDefault();

			UNIT_TEST_POSITIVE(array2.Size() == 3, "DynamicArrayC Add");
			UNIT_TEST_POSITIVE(array2.At(0) == 0, "DynamicArrayC FillDefault");	
			UNIT_TEST_POSITIVE(array2.At(1) == 0, "DynamicArrayC FillDefault");
			UNIT_TEST_POSITIVE(array2.At(2) == 0, "DynamicArrayC FillDefault");
			
			Dia::Core::Containers::DynamicArrayC<int, 3> array3;
			array3.Fill(11);

			UNIT_TEST_POSITIVE(array3.Size() == 3, "DynamicArrayC Add");
			UNIT_TEST_POSITIVE(array3.At(0) == 11, "DynamicArrayC Fill");	
			UNIT_TEST_POSITIVE(array3.At(1) == 11, "DynamicArrayC Fill");
			UNIT_TEST_POSITIVE(array3.At(2) == 11, "DynamicArrayC Fill");
			
			Dia::Core::Containers::DynamicArrayC<int, 3> array4;
			array4.FillDefault();

			UNIT_TEST_POSITIVE(array4.Size() == 3, "DynamicArrayC Add");
			UNIT_TEST_POSITIVE(array4.At(0) == 0, "DynamicArrayC Fill");	
			UNIT_TEST_POSITIVE(array4.At(1) == 0, "DynamicArrayC Fill");
			UNIT_TEST_POSITIVE(array4.At(2) == 0, "DynamicArrayC Fill");

			Dia::Core::Containers::DynamicArrayC<int, 5> array5;
			array5.AddDefault();
			array5.AddAt(1, 0);
			array5.AddAt(2, 1);
			array5.AddAt(3, 1);
			array5.AddAt(4, 3);
			UNIT_TEST_POSITIVE(array5.Size() == 5, "DynamicArrayC Add");
			UNIT_TEST_POSITIVE(array5.At(0) == 1, "DynamicArrayC Fill");	
			UNIT_TEST_POSITIVE(array5.At(1) == 3, "DynamicArrayC Fill");
			UNIT_TEST_POSITIVE(array5.At(2) == 2, "DynamicArrayC Fill");
			UNIT_TEST_POSITIVE(array5.At(3) == 4, "DynamicArrayC Fill");
			UNIT_TEST_POSITIVE(array5.At(4) == 0, "DynamicArrayC Fill");
	
		UNIT_TEST_BLOCK_END();

		UNIT_TEST_BLOCK_START();

			Dia::Core::Containers::DynamicArrayC<int, 3> array1;
			array1.Fill(11);

			array1.Remove();
			UNIT_TEST_POSITIVE(array1.Size() == 2, "DynamicArrayC Remove");
			UNIT_TEST_POSITIVE(array1.At(0) == 11, "DynamicArrayC Remove");	
			UNIT_TEST_POSITIVE(array1.At(1) == 11, "DynamicArrayC Remove");

			UNIT_TEST_ASSERT_EXPECTED_START();
			bool test = (array1.At(2) == 0);
			UNIT_TEST_ASSERT_EXPECTED_END();

			Dia::Core::Containers::DynamicArrayC<int, 3> array2;
			array2.Add(1);
			array2.Add(2);
			array2.Add(3);
			array2.RemoveAt(1);
			
			UNIT_TEST_POSITIVE(array2.Size() == 2, "DynamicArrayC Remove");
			UNIT_TEST_POSITIVE(array2.At(0) == 1, "DynamicArrayC Remove");	
			UNIT_TEST_POSITIVE(array2.At(1) == 3, "DynamicArrayC Remove");

			UNIT_TEST_ASSERT_EXPECTED_START();
			bool test = (array2.At(2) == 0);
			UNIT_TEST_ASSERT_EXPECTED_END();
			
			Dia::Core::Containers::DynamicArrayC<int, 3> array3;
			array3.Add(1);
			array3.Add(2);
			array3.Add(3);
			
			array3.RemoveFirst(2);
			
			UNIT_TEST_POSITIVE(array3.Size() == 2, "DynamicArrayC Remove");
			UNIT_TEST_POSITIVE(array3.At(0) == 1, "DynamicArrayC Remove");	
			UNIT_TEST_POSITIVE(array3.At(1) == 3, "DynamicArrayC Remove");

			UNIT_TEST_ASSERT_EXPECTED_START();
			bool test = (array3.At(2) == 0);
			UNIT_TEST_ASSERT_EXPECTED_END();
			
			Dia::Core::Containers::DynamicArrayC<int, 3> array4;
			array4.Add(1);
			array4.Add(2);
			array4.Add(3);
			
			array4.RemoveLast(2);
			
			UNIT_TEST_POSITIVE(array4.Size() == 2, "DynamicArrayC Remove");
			UNIT_TEST_POSITIVE(array4.At(0) == 1, "DynamicArrayC Remove");	
			UNIT_TEST_POSITIVE(array4.At(1) == 3, "DynamicArrayC Remove");

			UNIT_TEST_ASSERT_EXPECTED_START();
			bool test = (array4.At(2) == 0);
			UNIT_TEST_ASSERT_EXPECTED_END();

			Dia::Core::Containers::DynamicArrayC<int, 3> array5;
			array5.Add(1);
			array5.Add(2);
			array5.Add(3);
			
			array5.RemoveAll();
			
			UNIT_TEST_POSITIVE(array5.Size() == 0, "DynamicArrayC Remove");
			
			Dia::Core::Containers::DynamicArrayC<int, 5> array6;
			array6.Add(1);
			array6.Add(2);
			array6.Add(3);
			array6.Add(2);
			array6.Add(3);
			
			array6.RemoveAll(2);
			
			UNIT_TEST_POSITIVE(array6.Size() == 3, "DynamicArrayC Remove");
			UNIT_TEST_POSITIVE(array6.At(0) == 1, "DynamicArrayC Remove");	
			UNIT_TEST_POSITIVE(array6.At(1) == 3, "DynamicArrayC Remove");
			UNIT_TEST_POSITIVE(array6.At(2) == 3, "DynamicArrayC Remove");

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
			
			Dia::Core::Containers::DynamicArrayC<int, 5> array7;
			array7.Add(1);
			array7.Add(2);
			array7.Add(3);
			array7.Add(2);
			array7.Add(3);
			
			array7.RemoveAll(compare);
			
			UNIT_TEST_POSITIVE(array7.Size() == 3, "DynamicArrayC Remove");
			UNIT_TEST_POSITIVE(array7.At(0) == 1, "DynamicArrayC Remove");	
			UNIT_TEST_POSITIVE(array7.At(1) == 3, "DynamicArrayC Remove");
			UNIT_TEST_POSITIVE(array7.At(2) == 3, "DynamicArrayC Remove");

			UNIT_TEST_ASSERT_EXPECTED_START();
			bool test = (array7.At(3) == 0);
			UNIT_TEST_ASSERT_EXPECTED_END();
			
		UNIT_TEST_BLOCK_END();
		
		mState = kFinished;
	}
}
