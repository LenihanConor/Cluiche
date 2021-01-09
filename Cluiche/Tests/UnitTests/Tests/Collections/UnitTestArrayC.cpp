
#include "UnitTests/Tests/Collections/UnitTestArrayC.h"

#include <DiaCore/Containers/Arrays/ArrayC.h>

#include "UnitTests/Infrastructure/UnitTestMacros.h"

namespace UnitTests
{	
	UnitTestArrayC::UnitTestArrayC(const Dia::Core::Containers::String32& name)
		: UnitTestCoreContainers(name)
	{}

	UnitTestArrayC::UnitTestArrayC(void)
		: UnitTestCoreContainers()
	{}

	void UnitTestArrayC::DoTest()
	{
		UNIT_TEST_BLOCK_START();

			Dia::Core::Containers::ArrayC<int, 3> array;

			UNIT_TEST_POSITIVE(array.Size() == 3, "ArrayC()");
			UNIT_TEST_POSITIVE(array.At(0) == 0, "ArrayC()");

		UNIT_TEST_BLOCK_END();
		
		UNIT_TEST_BLOCK_START();

			int cArray1[3] = {1, 2, 3};

			UNIT_TEST_ASSERT_EXPECTED_START();
			int* ptr = NULL;
			Dia::Core::Containers::ArrayC<int, 3> array0(ptr, 3);
			UNIT_TEST_ASSERT_EXPECTED_END();			
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			Dia::Core::Containers::ArrayC<int, 3> array1(&cArray1[0], 10);
			UNIT_TEST_ASSERT_EXPECTED_END();

			Dia::Core::Containers::ArrayC<int, 3> array1(&cArray1[0], 3);
				
			UNIT_TEST_POSITIVE(array1.Size() == 3, "ArrayC(ConstPointer pData, unsigned int _size)");
			UNIT_TEST_POSITIVE(array1.At(0) == 1, "ArrayC(ConstPointer pData, unsigned int _size)");	
			UNIT_TEST_POSITIVE(array1.At(1) == 2, "ArrayC(ConstPointer pData, unsigned int _size)");
			UNIT_TEST_POSITIVE(array1.At(2) == 3, "ArrayC(ConstPointer pData, unsigned int _size)");

			int cArray2[5] = {1, 2, 3, 4, 5};
			Dia::Core::Containers::ArrayC<int, 3> array2(&cArray2[0], 3);
				
			UNIT_TEST_POSITIVE(array2.Size() == 3, "ArrayC(ConstPointer pData, unsigned int _size)");
			UNIT_TEST_POSITIVE(array2.At(0) == 1, "ArrayC(ConstPointer pData, unsigned int _size)");	
			UNIT_TEST_POSITIVE(array2.At(1) == 2, "ArrayC(ConstPointer pData, unsigned int _size)");
			UNIT_TEST_POSITIVE(array2.At(2) == 3, "ArrayC(ConstPointer pData, unsigned int _size)");

			int cArray3[2] = {1, 2};
			Dia::Core::Containers::ArrayC<int, 3> array3(&cArray3[0], 2);
				
			UNIT_TEST_POSITIVE(array3.Size() == 3, "ArrayC(ConstPointer pData, unsigned int _size)");
			UNIT_TEST_POSITIVE(array3.At(0) == 1, "ArrayC(ConstPointer pData, unsigned int _size)");	
			UNIT_TEST_POSITIVE(array3.At(1) == 2, "ArrayC(ConstPointer pData, unsigned int _size)");
			UNIT_TEST_POSITIVE(array3.At(2) == 0, "ArrayC(ConstPointer pData, unsigned int _size)");
		
		UNIT_TEST_BLOCK_END();
		
		UNIT_TEST_BLOCK_START();

			int cArray1[3] = {1, 2, 3};
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			Dia::Core::Containers::ArrayC<int, 3> array1(cArray1, 10);
			UNIT_TEST_ASSERT_EXPECTED_END();
			
			Dia::Core::Containers::ArrayC<int, 3> tempArray1(cArray1, 3);
			Dia::Core::Containers::ArrayC<int, 3> array1(tempArray1);
		
			UNIT_TEST_POSITIVE(array1.Size() == 3, "ArrayC(ConstReference data, unsigned int _size)");
			UNIT_TEST_POSITIVE(array1.At(0) == 1, "ArrayC(ConstReference data, unsigned int _size)");	
			UNIT_TEST_POSITIVE(array1.At(1) == 2, "ArrayC(ConstReference data, unsigned int _size)");
			UNIT_TEST_POSITIVE(array1.At(2) == 3, "ArrayC(ConstReference data, unsigned int _size)");

			int cArray2[5] = {1, 2, 3, 4, 5};
			Dia::Core::Containers::ArrayC<int, 5> tempArray2(cArray2, 5);
			Dia::Core::Containers::ArrayC<int, 3> array2(tempArray2);

			UNIT_TEST_POSITIVE(array2.Size() == 3, "ArrayC(ConstReference data, unsigned int _size)");
			UNIT_TEST_POSITIVE(array2.At(0) == 1, "ArrayC(ConstReference data, unsigned int _size)");	
			UNIT_TEST_POSITIVE(array2.At(1) == 2, "ArrayC(ConstReference data, unsigned int _size)");
			UNIT_TEST_POSITIVE(array2.At(2) == 3, "ArrayC(ConstReference data, unsigned int _size)");

			int cArray3[2] = {1, 2};
			Dia::Core::Containers::ArrayC<int, 2> tempArray3(cArray3, 2);
			Dia::Core::Containers::ArrayC<int, 3> array3(tempArray3);

			UNIT_TEST_POSITIVE(array3.Size() == 3, "ArrayC(ConstReference data, unsigned int _size)");
			UNIT_TEST_POSITIVE(array3.At(0) == 1, "ArrayC(ConstReference data, unsigned int _size)");	
			UNIT_TEST_POSITIVE(array3.At(1) == 2, "ArrayC(ConstReference data, unsigned int _size)");
			UNIT_TEST_POSITIVE(array3.At(2) == 0, "ArrayC(ConstReference data, unsigned int _size)");
		
		UNIT_TEST_BLOCK_END();
		
		UNIT_TEST_BLOCK_START();

			int cArray1[5] = {1, 2, 3, 4, 5};
			Dia::Core::Containers::ArrayC<int, 5> tempArray1(cArray1, 5);
			Dia::Core::Containers::ArrayC<int, 5> array1(tempArray1);

			UNIT_TEST_POSITIVE(array1.Size() == 5, "ArrayC(const ArrayC<T,_size>& rhs)");
			UNIT_TEST_POSITIVE(array1.At(0) == 1, "ArrayC(const ArrayC<T,_size>& rhs)");	
			UNIT_TEST_POSITIVE(array1.At(1) == 2, "ArrayC(const ArrayC<T,_size>& rhs)");
			UNIT_TEST_POSITIVE(array1.At(2) == 3, "ArrayC(const ArrayC<T,_size>& rhs)");
			UNIT_TEST_POSITIVE(array1.At(3) == 4, "ArrayC(const ArrayC<T,_size>& rhs)");
			UNIT_TEST_POSITIVE(array1.At(4) == 5, "ArrayC(const ArrayC<T,_size>& rhs)");

			int cArray2[5] = {1, 2, 3, 4, 5};
			Dia::Core::Containers::ArrayC<int, 5> tempArray2(cArray2, 5);
			Dia::Core::Containers::ArrayC<int, 3> array2(tempArray2);

			UNIT_TEST_POSITIVE(array2.Size() == 3, "ArrayC(const ArrayC<T,_size>& rhs)");
			UNIT_TEST_POSITIVE(array2.At(0) == 1, "ArrayC(const ArrayC<T,_size>& rhs)");	
			UNIT_TEST_POSITIVE(array2.At(1) == 2, "ArrayC(const ArrayC<T,_size>& rhs)");
			UNIT_TEST_POSITIVE(array2.At(2) == 3, "ArrayC(const ArrayC<T,_size>& rhs)");

		UNIT_TEST_BLOCK_END();


		UNIT_TEST_BLOCK_START();

			int cArray[5] = {1, 2, 3, 4, 5};
			Dia::Core::Containers::ArrayC<int, 5> tempArray(cArray, 5);

			UNIT_TEST_ASSERT_EXPECTED_START();
			Dia::Core::Containers::ArrayC<int, 3> array1(tempArray, 2, 0);
			UNIT_TEST_ASSERT_EXPECTED_END();	
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			Dia::Core::Containers::ArrayC<int, 3> array1(tempArray, 5, 2);
			UNIT_TEST_ASSERT_EXPECTED_END();	
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			Dia::Core::Containers::ArrayC<int, 3> array1(tempArray, 2, 10);
			UNIT_TEST_ASSERT_EXPECTED_END();

			int cArray1[5] = {1, 2, 3, 4, 5};
			Dia::Core::Containers::ArrayC<int, 5> tempArray1(cArray1, 5);
			Dia::Core::Containers::ArrayC<int, 3> array1(tempArray1, 1, 3);

			UNIT_TEST_POSITIVE(array1.Size() == 3, "ArrayC(const ArrayC<T,_size>& rhs, unsigned int startIndex, unsigned int numberElements)");
			UNIT_TEST_POSITIVE(array1.At(0) == 2, "ArrayC(const ArrayC<T,_size>& rhs, unsigned int startIndex, unsigned int numberElements)");	
			UNIT_TEST_POSITIVE(array1.At(1) == 3, "ArrayC(const ArrayC<T,_size>& rhs, unsigned int startIndex, unsigned int numberElements)");
			UNIT_TEST_POSITIVE(array1.At(2) == 4, "ArrayC(const ArrayC<T,_size>& rhs, unsigned int startIndex, unsigned int numberElements)");

			int cArray2[5] = {1, 2, 3, 4, 5};
			Dia::Core::Containers::ArrayC<int, 5> tempArray2(cArray2, 5);
			Dia::Core::Containers::ArrayC<int, 3> array2(tempArray2, 1, 2);

			UNIT_TEST_POSITIVE(array2.Size() == 3, "ArrayC(const ArrayC<T,_size>& rhs, unsigned int startIndex, unsigned int numberElements)");
			UNIT_TEST_POSITIVE(array2.At(0) == 2, "ArrayC(const ArrayC<T,_size>& rhs, unsigned int startIndex, unsigned int numberElements)");	
			UNIT_TEST_POSITIVE(array2.At(1) == 3, "ArrayC(const ArrayC<T,_size>& rhs, unsigned int startIndex, unsigned int numberElements)");
			UNIT_TEST_POSITIVE(array2.At(2) == 0, "ArrayC(const ArrayC<T,_size>& rhs, unsigned int startIndex, unsigned int numberElements)");

		UNIT_TEST_BLOCK_END();

		UNIT_TEST_BLOCK_START();
		
			int cArray1[3] = {1, 2, 3};
			Dia::Core::Containers::ArrayC<int, 3> tempArray1(cArray1, 3);

			Dia::Core::Containers::ArrayC<int, 3>::ConstIterator iter1(tempArray1.Begin());
			Dia::Core::Containers::ArrayC<int, 3> array1(iter1);

			UNIT_TEST_POSITIVE(array1.Size() == 3, "ArrayC<T, size>::ArrayC ( Iterator begin )");
			UNIT_TEST_POSITIVE(array1.At(0) == 1, "ArrayC<T, size>::ArrayC ( Iterator begin )");	
			UNIT_TEST_POSITIVE(array1.At(1) == 2, "ArrayC<T, size>::ArrayC ( Iterator begin )");
			UNIT_TEST_POSITIVE(array1.At(2) == 3, "ArrayC<T, size>::ArrayC ( Iterator begin )");

			int cArray2[2] = {1, 2};
			Dia::Core::Containers::ArrayC<int, 3> tempArray2(cArray2, 2);
			Dia::Core::Containers::ArrayC<int, 3> array2(tempArray2.BeginConst());

			UNIT_TEST_POSITIVE(array2.Size() == 3, "ArrayC<T, size>::ArrayC ( Iterator begin )");
			UNIT_TEST_POSITIVE(array2.At(0) == 1, "ArrayC<T, size>::ArrayC ( Iterator begin )");	
			UNIT_TEST_POSITIVE(array2.At(1) == 2, "ArrayC<T, size>::ArrayC ( Iterator begin )");
			UNIT_TEST_POSITIVE(array2.At(2) == 0, "ArrayC<T, size>::ArrayC ( Iterator begin )");

		UNIT_TEST_BLOCK_END();
		
		UNIT_TEST_BLOCK_START();
		
			int cArray1[3] = {1, 2, 3};
			Dia::Core::Containers::ArrayC<int, 3> tempArray1(cArray1, 3);

			Dia::Core::Containers::ArrayC<int, 3> array1(tempArray1.EndConst());

			UNIT_TEST_POSITIVE(array1.Size() == 3, "ArrayC<T, size>::ArrayC ( ConstReverseIterator iter begin )");
			UNIT_TEST_POSITIVE(array1.At(0) == 3, "ArrayC<T, size>::ArrayC ( ConstReverseIterator iter begin )");	
			UNIT_TEST_POSITIVE(array1.At(1) == 2, "ArrayC<T, size>::ArrayC ( ConstReverseIterator iter begin )");
			UNIT_TEST_POSITIVE(array1.At(2) == 1, "ArrayC<T, size>::ArrayC ( ConstReverseIterator iter begin )");

			int cArray2[2] = {1, 2};
			Dia::Core::Containers::ArrayC<int, 2> tempArray2(cArray2, 2);
			Dia::Core::Containers::ArrayC<int, 3> array2(tempArray2.EndConst());

			UNIT_TEST_POSITIVE(array2.Size() == 3, "ArrayC<T, size>::ArrayC ( ConstReverseIterator iter begin )");
			UNIT_TEST_POSITIVE(array2.At(0) == 2, "ArrayC<T, size>::ArrayC ( ConstReverseIterator iter begin )");	
			UNIT_TEST_POSITIVE(array2.At(1) == 1, "ArrayC<T, size>::ArrayC ( ConstReverseIterator iter begin )");
			UNIT_TEST_POSITIVE(array2.At(2) == 0, "ArrayC<T, size>::ArrayC ( ConstReverseIterator iter begin )");

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
			Dia::Core::Containers::ArrayC<int, 5> tempArray1(cArray1, 5);

			Dia::Core::Containers::ArrayC<int, 5> array1(tempArray1.BeginConst(), filter1);

			UNIT_TEST_POSITIVE(array1.Size() == 5, "ArrayC<T, size>::ArrayC ( ConstIterator& iter, const Evalutor& filter )");
			UNIT_TEST_POSITIVE(array1.At(0) == 3, "ArrayC<T, size>::ArrayC ( ConstIterator& iter, const Evalutor& filter )");	
			UNIT_TEST_POSITIVE(array1.At(1) == 4, "ArrayC<T, size>::ArrayC ( ConstIterator& iter, const Evalutor& filter )");
			UNIT_TEST_POSITIVE(array1.At(2) == 5, "ArrayC<T, size>::ArrayC ( ConstIterator& iter, const Evalutor& filter )");
			UNIT_TEST_POSITIVE(array1.At(3) == 0, "ArrayC<T, size>::ArrayC ( ConstIterator& iter, const Evalutor& filter )");
			UNIT_TEST_POSITIVE(array1.At(4) == 0, "ArrayC<T, size>::ArrayC ( ConstIterator& iter, const Evalutor& filter )");

		UNIT_TEST_BLOCK_END();


		UNIT_TEST_BLOCK_START();

			int cArray1[3] = {1, 2, 3};

			UNIT_TEST_ASSERT_EXPECTED_START();
			int* ptr = NULL;
			Dia::Core::Containers::ArrayC<int, 3> array0;
			array0.Assign(ptr, 3);
			UNIT_TEST_ASSERT_EXPECTED_END();			
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			Dia::Core::Containers::ArrayC<int, 3> array1;
			array1.Assign(&cArray1[0], 10);
			UNIT_TEST_ASSERT_EXPECTED_END();

			Dia::Core::Containers::ArrayC<int, 3> array1;
			array1.Assign(&cArray1[0], 3);
				
			UNIT_TEST_POSITIVE(array1.Size() == 3, "ArrayC.Assign(ConstPointer pData, unsigned int _size)");
			UNIT_TEST_POSITIVE(array1.At(0) == 1, "ArrayC.Assign(ConstPointer pData, unsigned int _size)");	
			UNIT_TEST_POSITIVE(array1.At(1) == 2, "ArrayC.Assign(ConstPointer pData, unsigned int _size)");
			UNIT_TEST_POSITIVE(array1.At(2) == 3, "ArrayC.Assign(ConstPointer pData, unsigned int _size)");

			int cArray2[5] = {1, 2, 3, 4, 5};
			Dia::Core::Containers::ArrayC<int, 3> array2;
			array2.Assign(&cArray2[0], 3);
				
			UNIT_TEST_POSITIVE(array2.Size() == 3, "ArrayC.Assign(ConstPointer pData, unsigned int _size)");
			UNIT_TEST_POSITIVE(array2.At(0) == 1, "ArrayC.Assign(ConstPointer pData, unsigned int _size)");	
			UNIT_TEST_POSITIVE(array2.At(1) == 2, "ArrayC.Assign(ConstPointer pData, unsigned int _size)");
			UNIT_TEST_POSITIVE(array2.At(2) == 3, "ArrayC.Assign(ConstPointer pData, unsigned int _size)");

			int cArray3[2] = {1, 2};
			Dia::Core::Containers::ArrayC<int, 3> array3;
			array3.Assign(&cArray3[0], 2);
				
			UNIT_TEST_POSITIVE(array3.Size() == 3, "ArrayC.Assign(ConstPointer pData, unsigned int _size)");
			UNIT_TEST_POSITIVE(array3.At(0) == 1, "ArrayC.Assign(ConstPointer pData, unsigned int _size)");	
			UNIT_TEST_POSITIVE(array3.At(1) == 2, "ArrayC.Assign(ConstPointer pData, unsigned int _size)");
			UNIT_TEST_POSITIVE(array3.At(2) == 0, "ArrayC.Assign(ConstPointer pData, unsigned int _size)");
		
		UNIT_TEST_BLOCK_END();
		
		UNIT_TEST_BLOCK_START();

			int cArray1[3] = {1, 2, 3};
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			Dia::Core::Containers::ArrayC<int, 3> array1;
			array1.Assign(cArray1, 10);
			UNIT_TEST_ASSERT_EXPECTED_END();
			
			Dia::Core::Containers::ArrayC<int, 3> tempArray1;
			tempArray1.Assign(cArray1, 3);
			Dia::Core::Containers::ArrayC<int, 3> array1(tempArray1);
		
			UNIT_TEST_POSITIVE(array1.Size() == 3, "ArrayC.Assign(ConstReference data, unsigned int _size)");
			UNIT_TEST_POSITIVE(array1.At(0) == 1, "ArrayC.Assign(ConstReference data, unsigned int _size)");	
			UNIT_TEST_POSITIVE(array1.At(1) == 2, "ArrayC.Assign(ConstReference data, unsigned int _size)");
			UNIT_TEST_POSITIVE(array1.At(2) == 3, "ArrayC.Assign(ConstReference data, unsigned int _size)");

			int cArray2[5] = {1, 2, 3, 4, 5};
			Dia::Core::Containers::ArrayC<int, 5> tempArray2;
			tempArray2.Assign(cArray2, 5);
			Dia::Core::Containers::ArrayC<int, 3> array2(tempArray2);

			UNIT_TEST_POSITIVE(array2.Size() == 3, "ArrayC.Assign(ConstReference data, unsigned int _size)");
			UNIT_TEST_POSITIVE(array2.At(0) == 1, "ArrayC.Assign(ConstReference data, unsigned int _size)");	
			UNIT_TEST_POSITIVE(array2.At(1) == 2, "ArrayC.Assign(ConstReference data, unsigned int _size)");
			UNIT_TEST_POSITIVE(array2.At(2) == 3, "ArrayC.Assign(ConstReference data, unsigned int _size)");

			int cArray3[2] = {1, 2};
			Dia::Core::Containers::ArrayC<int, 2> tempArray3;
			tempArray3.Assign(cArray3, 2);
			Dia::Core::Containers::ArrayC<int, 3> array3(tempArray3);

			UNIT_TEST_POSITIVE(array3.Size() == 3, "ArrayC.Assign(ConstReference data, unsigned int _size)");
			UNIT_TEST_POSITIVE(array3.At(0) == 1, "ArrayC.Assign(ConstReference data, unsigned int _size)");	
			UNIT_TEST_POSITIVE(array3.At(1) == 2, "ArrayC.Assign(ConstReference data, unsigned int _size)");
			UNIT_TEST_POSITIVE(array3.At(2) == 0, "ArrayC.Assign(ConstReference data, unsigned int _size)");
		
		UNIT_TEST_BLOCK_END();
		
		UNIT_TEST_BLOCK_START();

			int cArray1[5] = {1, 2, 3, 4, 5};
			Dia::Core::Containers::ArrayC<int, 5> tempArray1(cArray1, 5);
			Dia::Core::Containers::ArrayC<int, 5> array1;
			array1.Assign(tempArray1);

			UNIT_TEST_POSITIVE(array1.Size() == 5, "ArrayC(const ArrayC<T,_size>& rhs)");
			UNIT_TEST_POSITIVE(array1.At(0) == 1, "ArrayC.Assign(const ArrayC<T,_size>& rhs)");	
			UNIT_TEST_POSITIVE(array1.At(1) == 2, "ArrayC.Assign(const ArrayC<T,_size>& rhs)");
			UNIT_TEST_POSITIVE(array1.At(2) == 3, "ArrayC.Assign(const ArrayC<T,_size>& rhs)");
			UNIT_TEST_POSITIVE(array1.At(3) == 4, "ArrayC.Assign(const ArrayC<T,_size>& rhs)");
			UNIT_TEST_POSITIVE(array1.At(4) == 5, "ArrayC.Assign(const ArrayC<T,_size>& rhs)");

			int cArray2[5] = {1, 2, 3, 4, 5};
			Dia::Core::Containers::ArrayC<int, 5> tempArray2(cArray2, 5);
			Dia::Core::Containers::ArrayC<int, 3> array2;
			array2.Assign(tempArray2);

			UNIT_TEST_POSITIVE(array2.Size() == 3, "ArrayC(const ArrayC<T,_size>& rhs)");
			UNIT_TEST_POSITIVE(array2.At(0) == 1, "ArrayC.Assign(const ArrayC<T,_size>& rhs)");	
			UNIT_TEST_POSITIVE(array2.At(1) == 2, "ArrayC.Assign(const ArrayC<T,_size>& rhs)");
			UNIT_TEST_POSITIVE(array2.At(2) == 3, "ArrayC.Assign(const ArrayC<T,_size>& rhs)");

		UNIT_TEST_BLOCK_END();

		UNIT_TEST_BLOCK_START();

			int cArray[5] = {1, 2, 3, 4, 5};
			Dia::Core::Containers::ArrayC<int, 5> tempArray(cArray, 5);

			UNIT_TEST_ASSERT_EXPECTED_START();
			Dia::Core::Containers::ArrayC<int, 3> array1;
			array1.Assign(tempArray, 2, 0);
			UNIT_TEST_ASSERT_EXPECTED_END();	
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			Dia::Core::Containers::ArrayC<int, 3> array1;
			array1.Assign(tempArray, 5, 2);
			UNIT_TEST_ASSERT_EXPECTED_END();	
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			Dia::Core::Containers::ArrayC<int, 3> array1;
			array1.Assign(tempArray, 2, 10);
			UNIT_TEST_ASSERT_EXPECTED_END();

			int cArray1[5] = {1, 2, 3, 4, 5};
			Dia::Core::Containers::ArrayC<int, 5> tempArray1(cArray1, 5);
			Dia::Core::Containers::ArrayC<int, 3> array1;
			array1.Assign(tempArray1, 1, 3);

			UNIT_TEST_POSITIVE(array1.Size() == 3, "ArrayC.Assign(const ArrayC<T,_size>& rhs, unsigned int startIndex, unsigned int numberElements)");
			UNIT_TEST_POSITIVE(array1.At(0) == 2, "ArrayC.Assign(const ArrayC<T,_size>& rhs, unsigned int startIndex, unsigned int numberElements)");	
			UNIT_TEST_POSITIVE(array1.At(1) == 3, "ArrayC.Assign(const ArrayC<T,_size>& rhs, unsigned int startIndex, unsigned int numberElements)");
			UNIT_TEST_POSITIVE(array1.At(2) == 4, "ArrayC.Assign(const ArrayC<T,_size>& rhs, unsigned int startIndex, unsigned int numberElements)");

			int cArray2[5] = {1, 2, 3, 4, 5};
			Dia::Core::Containers::ArrayC<int, 5> tempArray2(cArray2, 5);
			Dia::Core::Containers::ArrayC<int, 3> array2;
			array2.Assign(tempArray2, 1, 2);

			UNIT_TEST_POSITIVE(array2.Size() == 3, "ArrayC.Assign(const ArrayC<T,_size>& rhs, unsigned int startIndex, unsigned int numberElements)");
			UNIT_TEST_POSITIVE(array2.At(0) == 2, "ArrayC.Assign(const ArrayC<T,_size>& rhs, unsigned int startIndex, unsigned int numberElements)");	
			UNIT_TEST_POSITIVE(array2.At(1) == 3, "ArrayC.Assign(const ArrayC<T,_size>& rhs, unsigned int startIndex, unsigned int numberElements)");
			UNIT_TEST_POSITIVE(array2.At(2) == 0, "ArrayC.Assign(const ArrayC<T,_size>& rhs, unsigned int startIndex, unsigned int numberElements)");

		UNIT_TEST_BLOCK_END();

		UNIT_TEST_BLOCK_START();
		
			int cArray1[3] = {1, 2, 3};
			Dia::Core::Containers::ArrayC<int, 3> tempArray1(cArray1, 3);

			Dia::Core::Containers::ArrayC<int, 3> array1;
			array1.Assign(tempArray1.BeginConst());

			UNIT_TEST_POSITIVE(array1.Size() == 3, "ArrayC<T, size>::ArrayC.Assign ( Iterator begin )");
			UNIT_TEST_POSITIVE(array1.At(0) == 1, "ArrayC<T, size>::ArrayC.Assign ( Iterator begin )");	
			UNIT_TEST_POSITIVE(array1.At(1) == 2, "ArrayC<T, size>::ArrayC.Assign ( Iterator begin )");
			UNIT_TEST_POSITIVE(array1.At(2) == 3, "ArrayC<T, size>::ArrayC.Assign ( Iterator begin )");

			int cArray2[2] = {1, 2};
			Dia::Core::Containers::ArrayC<int, 3> tempArray2(cArray2, 2);
			Dia::Core::Containers::ArrayC<int, 3> array2;
			array2.Assign(tempArray2.BeginConst());

			UNIT_TEST_POSITIVE(array2.Size() == 3, "ArrayC<T, size>::ArrayC.Assign ( Iterator begin )");
			UNIT_TEST_POSITIVE(array2.At(0) == 1, "ArrayC<T, size>::ArrayC.Assign ( Iterator begin )");	
			UNIT_TEST_POSITIVE(array2.At(1) == 2, "ArrayC<T, size>::ArrayC.Assign ( Iterator begin )");
			UNIT_TEST_POSITIVE(array2.At(2) == 0, "ArrayC<T, size>::ArrayC.Assign ( Iterator begin )");

		UNIT_TEST_BLOCK_END();
		
		UNIT_TEST_BLOCK_START();
		
			int cArray1[3] = {1, 2, 3};
			Dia::Core::Containers::ArrayC<int, 3> tempArray1(cArray1, 3);

			Dia::Core::Containers::ArrayC<int, 3> array1;
			array1.Assign(tempArray1.EndConst());

			UNIT_TEST_POSITIVE(array1.Size() == 3, "ArrayC<T, size>::ArrayC.Assign ( ConstReverseIterator iter begin )");
			UNIT_TEST_POSITIVE(array1.At(0) == 3, "ArrayC<T, size>::ArrayC.Assign ( ConstReverseIterator iter begin )");	
			UNIT_TEST_POSITIVE(array1.At(1) == 2, "ArrayC<T, size>::ArrayC.Assign ( ConstReverseIterator iter begin )");
			UNIT_TEST_POSITIVE(array1.At(2) == 1, "ArrayC<T, size>::ArrayC.Assign ( ConstReverseIterator iter begin )");

			int cArray2[2] = {1, 2};
			Dia::Core::Containers::ArrayC<int, 2> tempArray2(cArray2, 2);
			Dia::Core::Containers::ArrayC<int, 3> array2;
			array2.Assign(tempArray2.EndConst());

			UNIT_TEST_POSITIVE(array2.Size() == 3, "ArrayC<T, size>::ArrayC.Assign ( ConstReverseIterator iter begin )");
			UNIT_TEST_POSITIVE(array2.At(0) == 2, "ArrayC<T, size>::ArrayC.Assign ( ConstReverseIterator iter begin )");	
			UNIT_TEST_POSITIVE(array2.At(1) == 1, "ArrayC<T, size>::ArrayC.Assign ( ConstReverseIterator iter begin )");
			UNIT_TEST_POSITIVE(array2.At(2) == 0, "ArrayC<T, size>::ArrayC.Assign ( ConstReverseIterator iter begin )");

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
			Dia::Core::Containers::ArrayC<int, 5> tempArray1(cArray1, 5);

			Dia::Core::Containers::ArrayC<int, 5> array1;
			array1.Assign(tempArray1.BeginConst(), filter1);

			UNIT_TEST_POSITIVE(array1.Size() == 5, "ArrayC<T, size>::ArrayC.Assign ( ConstIterator& iter, const Evaluator& filter )");
			UNIT_TEST_POSITIVE(array1.At(0) == 3, "ArrayC<T, size>::ArrayC.Assign ( ConstIterator& iter, const Evaluator& filter )");	
			UNIT_TEST_POSITIVE(array1.At(1) == 4, "ArrayC<T, size>::ArrayC.Assign ( ConstIterator& iter, const Evaluator& filter )");
			UNIT_TEST_POSITIVE(array1.At(2) == 5, "ArrayC<T, size>::ArrayC.Assign ( ConstIterator& iter, const Evaluator& filter )");
			UNIT_TEST_POSITIVE(array1.At(3) == 0, "ArrayC<T, size>::ArrayC.Assign ( ConstIterator& iter, const Evaluator& filter )");
			UNIT_TEST_POSITIVE(array1.At(4) == 0, "ArrayC<T, size>::ArrayC.Assign ( ConstIterator& iter, const Evaluator& filter )");

		UNIT_TEST_BLOCK_END();
		
		UNIT_TEST_BLOCK_START();

			int cArray1[5] = {1, 2, 3, 4, 5};
			Dia::Core::Containers::ArrayC<int, 5> tempArray1(cArray1, 5);
			Dia::Core::Containers::ArrayC<int, 5> array1 = tempArray1;

			UNIT_TEST_POSITIVE(array1.Size() == 5, "ArrayC opertor=");
			UNIT_TEST_POSITIVE(array1.At(0) == 1, "ArrayC opertor=");	
			UNIT_TEST_POSITIVE(array1.At(1) == 2, "ArrayC opertor=");
			UNIT_TEST_POSITIVE(array1.At(2) == 3, "ArrayC opertor=");
			UNIT_TEST_POSITIVE(array1.At(3) == 4, "ArrayC opertor=");
			UNIT_TEST_POSITIVE(array1.At(4) == 5, "ArrayC opertor=");

			int cArray2[5] = {1, 2, 3, 4, 5};
			Dia::Core::Containers::ArrayC<int, 5> tempArray2(cArray2, 5);
			Dia::Core::Containers::ArrayC<int, 3> array2;
			array2 = tempArray2;

			UNIT_TEST_POSITIVE(array2.Size() == 3, "ArrayC opertor=");
			UNIT_TEST_POSITIVE(array2.At(0) == 1, "ArrayC opertor=");	
			UNIT_TEST_POSITIVE(array2.At(1) == 2, "ArrayC opertor=");
			UNIT_TEST_POSITIVE(array2.At(2) == 3, "ArrayC opertor=");

		UNIT_TEST_BLOCK_END();

		UNIT_TEST_BLOCK_START();

			int cArray1[5] = {1, 2, 3, 4, 5};
			
			Dia::Core::Containers::ArrayC<int, 5> array1(cArray1, 5);
			Dia::Core::Containers::ArrayC<int, 5> array2(array1);
			Dia::Core::Containers::ArrayC<int, 5> array3(array1, 1, 3);

			UNIT_TEST_POSITIVE(array1 == array2, "ArrayC opertor==");
			UNIT_TEST_NEGATIVE(array1 == array3, "ArrayC opertor==");	
			UNIT_TEST_POSITIVE(array1 != array3, "ArrayC opertor!=");
			UNIT_TEST_NEGATIVE(array1 != array2, "ArrayC opertor!=");

		UNIT_TEST_BLOCK_END();

		UNIT_TEST_BLOCK_START();

			int cArray1[5] = {1, 2, 3, 4, 5};
			
			Dia::Core::Containers::ArrayC<int, 5> array1(cArray1, 5);
			
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

			UNIT_TEST_POSITIVE(array1[0] == array1.At(0), "ArrayC opertor[]");
			UNIT_TEST_POSITIVE(array1[1] == array1.At(1), "ArrayC opertor[]");
			UNIT_TEST_POSITIVE(array1[2] == array1.At(2), "ArrayC opertor[]");
			UNIT_TEST_POSITIVE(array1[3] == array1.At(3), "ArrayC opertor[]");
			UNIT_TEST_POSITIVE(array1[4] == array1.At(4), "ArrayC opertor[]");
			
			UNIT_TEST_POSITIVE(array1.Front() == array1.At(0), "ArrayC Front");
			UNIT_TEST_POSITIVE(array1.Back() == array1.At(4), "ArrayC Back");
		
		UNIT_TEST_BLOCK_END();
		
		UNIT_TEST_BLOCK_START();

			int cArray1[5] = {1, 2, 3, 4, 5};
			
			Dia::Core::Containers::ArrayC<int, 5> array1(cArray1, 5);
			
			UNIT_TEST_POSITIVE(&array1[0] == array1.IteratorAt(0).Current(), "ArrayC IteratorAt");
			UNIT_TEST_POSITIVE(&array1[1] == array1.IteratorAt(1).Current(), "ArrayC IteratorAt");
			UNIT_TEST_POSITIVE(&array1[2] == array1.IteratorAt(2).Current(), "ArrayC IteratorAt");
			UNIT_TEST_POSITIVE(&array1[3] == array1.IteratorAt(3).Current(), "ArrayC IteratorAt");
			UNIT_TEST_POSITIVE(&array1[4] == array1.IteratorAt(4).Current(), "ArrayC IteratorAt");
			
			UNIT_TEST_POSITIVE(array1.Begin() == array1.IteratorAt(0), "ArrayC Begin");
			UNIT_TEST_POSITIVE(array1.BeginConst() == array1.IteratorAtConst(0), "ArrayC BeginConst");

			UNIT_TEST_POSITIVE(&array1[0] == array1.ReverseIteratorAt(0).Current(), "ArrayC ReverseIteratorAt");
			UNIT_TEST_POSITIVE(&array1[1] == array1.ReverseIteratorAt(1).Current(), "ArrayC ReverseIteratorAt");
			UNIT_TEST_POSITIVE(&array1[2] == array1.ReverseIteratorAt(2).Current(), "ArrayC ReverseIteratorAt");
			UNIT_TEST_POSITIVE(&array1[3] == array1.ReverseIteratorAt(3).Current(), "ArrayC ReverseIteratorAt");
			UNIT_TEST_POSITIVE(&array1[4] == array1.ReverseIteratorAt(4).Current(), "ArrayC ReverseIteratorAt");
			
			UNIT_TEST_POSITIVE(array1.End() == array1.ReverseIteratorAt(4), "ArrayC End");
			UNIT_TEST_POSITIVE(array1.EndConst() == array1.ReverseIteratorAtConst(4), "ArrayC EndConst");
		
		UNIT_TEST_BLOCK_END();

		UNIT_TEST_BLOCK_START();

			int cArray1[10] = {1, 2, 3, 4, 5, 1, 3, 3, 3, 4};
			
			Dia::Core::Containers::ArrayC<int, 10> array1(cArray1, 10);
			
			UNIT_TEST_POSITIVE(array1.FrequencyOfElement(1) == 2, "ArrayC FrequencyOfElement");
			UNIT_TEST_POSITIVE(array1.FrequencyOfElement(2) == 1, "ArrayC FrequencyOfElement");
			UNIT_TEST_POSITIVE(array1.FrequencyOfElement(3) == 4, "ArrayC FrequencyOfElement");
			UNIT_TEST_POSITIVE(array1.FrequencyOfElement(4) == 2, "ArrayC FrequencyOfElement");
			UNIT_TEST_POSITIVE(array1.FrequencyOfElement(5) == 1, "ArrayC FrequencyOfElement");
			UNIT_TEST_POSITIVE(array1.FrequencyOfElement(6) == 0, "ArrayC FrequencyOfElement");
			
			unsigned int numberOfUniqueElements = 0;
			Dia::Core::Containers::ArrayC<int, 10> arrayUnique;
			array1.UniqueElements(arrayUnique, numberOfUniqueElements);

			UNIT_TEST_POSITIVE(numberOfUniqueElements == 5, "ArrayC numberOfUniqueElements");
			UNIT_TEST_POSITIVE(arrayUnique[0] == 1, "ArrayC numberOfUniqueElements");
			UNIT_TEST_POSITIVE(arrayUnique[1] == 2, "ArrayC numberOfUniqueElements");
			UNIT_TEST_POSITIVE(arrayUnique[2] == 3, "ArrayC numberOfUniqueElements");
			UNIT_TEST_POSITIVE(arrayUnique[3] == 4, "ArrayC numberOfUniqueElements");
			UNIT_TEST_POSITIVE(arrayUnique[4] == 5, "ArrayC numberOfUniqueElements");
			
			unsigned int numberOfFreqUniqueElements = 0;
			Dia::Core::Containers::ArrayC<int, 10> arrayFreqUnique;
			Dia::Core::Containers::ArrayC<int, 10> arrayFreq;
			array1.FrequencyUniqueElements(arrayFreqUnique, arrayFreq, numberOfFreqUniqueElements);

			UNIT_TEST_POSITIVE(numberOfFreqUniqueElements == 5, "ArrayC numberOfUniqueElements");
			UNIT_TEST_POSITIVE(arrayFreqUnique[0] == 1, "ArrayC numberOfUniqueElements");
			UNIT_TEST_POSITIVE(arrayFreqUnique[1] == 2, "ArrayC numberOfUniqueElements");
			UNIT_TEST_POSITIVE(arrayFreqUnique[2] == 3, "ArrayC numberOfUniqueElements");
			UNIT_TEST_POSITIVE(arrayFreqUnique[3] == 4, "ArrayC numberOfUniqueElements");
			UNIT_TEST_POSITIVE(arrayFreqUnique[4] == 5, "ArrayC numberOfUniqueElements");

			UNIT_TEST_POSITIVE(arrayFreq[0] == 2, "ArrayC numberOfUniqueElements");
			UNIT_TEST_POSITIVE(arrayFreq[1] == 1, "ArrayC numberOfUniqueElements");
			UNIT_TEST_POSITIVE(arrayFreq[2] == 4, "ArrayC numberOfUniqueElements");
			UNIT_TEST_POSITIVE(arrayFreq[3] == 2, "ArrayC numberOfUniqueElements");
			UNIT_TEST_POSITIVE(arrayFreq[4] == 1, "ArrayC numberOfUniqueElements");
		
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

			Dia::Core::Containers::ArrayC<int, 10> array1(cArray1, 10);
			Dia::Core::Containers::ArrayC<int, 5> array2(cArray2, 5);
			
			Dia::Core::Containers::ArrayC<int, 10> array3(cArray1, 10);
			Dia::Core::Containers::ArrayC<int, 5> array4(cArray2, 5);

			array1.Sort(order);
			
			UNIT_TEST_POSITIVE(array1[0] == 1, "ArrayC Sort");
			UNIT_TEST_POSITIVE(array1[1] == 1, "ArrayC Sort");
			UNIT_TEST_POSITIVE(array1[2] == 2, "ArrayC Sort");
			UNIT_TEST_POSITIVE(array1[3] == 3, "ArrayC Sort");
			UNIT_TEST_POSITIVE(array1[4] == 3, "ArrayC Sort");
			UNIT_TEST_POSITIVE(array1[5] == 3, "ArrayC Sort");
			UNIT_TEST_POSITIVE(array1[6] == 3, "ArrayC Sort");
			UNIT_TEST_POSITIVE(array1[7] == 4, "ArrayC Sort");
			UNIT_TEST_POSITIVE(array1[8] == 4, "ArrayC Sort");
			UNIT_TEST_POSITIVE(array1[9] == 5, "ArrayC Sort");
			UNIT_TEST_POSITIVE(array1.IsSorted(order), "ArrayC Sort");

			array2.Sort(order);
			
			UNIT_TEST_POSITIVE(array2[0] == 1, "ArrayC Sort");
			UNIT_TEST_POSITIVE(array2[1] == 2, "ArrayC Sort");
			UNIT_TEST_POSITIVE(array2[2] == 3, "ArrayC Sort");
			UNIT_TEST_POSITIVE(array2[3] == 4, "ArrayC Sort");
			UNIT_TEST_POSITIVE(array2[4] == 5, "ArrayC Sort");
			UNIT_TEST_POSITIVE(array2.IsSorted(order), "ArrayC Sort");
				
			array3.Sort();

			UNIT_TEST_POSITIVE(array3[0] == 1, "ArrayC Sort");
			UNIT_TEST_POSITIVE(array3[1] == 1, "ArrayC Sort");
			UNIT_TEST_POSITIVE(array3[2] == 2, "ArrayC Sort");
			UNIT_TEST_POSITIVE(array3[3] == 3, "ArrayC Sort");
			UNIT_TEST_POSITIVE(array3[4] == 3, "ArrayC Sort");
			UNIT_TEST_POSITIVE(array3[5] == 3, "ArrayC Sort");
			UNIT_TEST_POSITIVE(array3[6] == 3, "ArrayC Sort");
			UNIT_TEST_POSITIVE(array3[7] == 4, "ArrayC Sort");
			UNIT_TEST_POSITIVE(array3[8] == 4, "ArrayC Sort");
			UNIT_TEST_POSITIVE(array3[9] == 5, "ArrayC Sort");
			UNIT_TEST_POSITIVE(array3.IsSorted(), "ArrayC Sort");

			array4.Sort();
			
			UNIT_TEST_POSITIVE(array4[0] == 1, "ArrayC Sort");
			UNIT_TEST_POSITIVE(array4[1] == 2, "ArrayC Sort");
			UNIT_TEST_POSITIVE(array4[2] == 3, "ArrayC Sort");
			UNIT_TEST_POSITIVE(array4[3] == 4, "ArrayC Sort");
			UNIT_TEST_POSITIVE(array4[4] == 5, "ArrayC Sort");
			UNIT_TEST_POSITIVE(array4.IsSorted(), "ArrayC Sort");

		UNIT_TEST_BLOCK_END();
		
		UNIT_TEST_BLOCK_START();
						
			int cArray1[5] = {1, 2, 3, 4, 5};
			int cArray2[5] = {4, 2, 5, 1, 3};

			Dia::Core::Containers::ArrayC<int, 5> array1(cArray1, 5);
			Dia::Core::Containers::ArrayC<int, 5> array2(cArray2, 5);
			
			array1.Swap(array2);
			
			UNIT_TEST_POSITIVE(array1[0] == 4, "ArrayC Swap");
			UNIT_TEST_POSITIVE(array1[1] == 2, "ArrayC Swap");
			UNIT_TEST_POSITIVE(array1[2] == 5, "ArrayC Swap");
			UNIT_TEST_POSITIVE(array1[3] == 1, "ArrayC Swap");
			UNIT_TEST_POSITIVE(array1[4] == 3, "ArrayC Swap");

			UNIT_TEST_POSITIVE(array2[0] == 1, "ArrayC Swap");
			UNIT_TEST_POSITIVE(array2[1] == 2, "ArrayC Swap");
			UNIT_TEST_POSITIVE(array2[2] == 3, "ArrayC Swap");
			UNIT_TEST_POSITIVE(array2[3] == 4, "ArrayC Swap");
			UNIT_TEST_POSITIVE(array2[4] == 5, "ArrayC Swap");

		UNIT_TEST_BLOCK_END();

		UNIT_TEST_BLOCK_START();
						
			int cArray1[10] = {1, 2, 3, 4, 5, 6, 3, 4, 5, 7};

			Dia::Core::Containers::ArrayC<int, 10> array1(cArray1, 10);
			
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

			UNIT_TEST_POSITIVE(array1.FindIndex(5) == 4 , "ArrayC FindIndex");
			UNIT_TEST_POSITIVE(array1.FindIndex(7) == 9, "ArrayC FindIndex");
			UNIT_TEST_POSITIVE(array1.FindIndex(10) == -1, "ArrayC FindIndex");

			UNIT_TEST_POSITIVE(array1.FindBetweenIndex(5, 0, 7) == 4 , "ArrayC FindBetweenIndex");
			UNIT_TEST_POSITIVE(array1.FindBetweenIndex(5, 6, 9) == 8, "ArrayC FindBetweenIndex");
			UNIT_TEST_POSITIVE(array1.FindBetweenIndex(1, 6, 9) == -1, "ArrayC FindBetweenIndex");
			
			UNIT_TEST_POSITIVE(array1.FindIndex(5, oneLess) == 3 , "ArrayC FindIndex");
			UNIT_TEST_POSITIVE(array1.FindIndex(7, oneLess) == 5, "ArrayC FindIndex");
			UNIT_TEST_POSITIVE(array1.FindIndex(10, oneLess) == -1, "ArrayC FindIndex");

			UNIT_TEST_POSITIVE(array1.FindBetweenIndex(5, oneLess, 0, 7) == 3 , "ArrayC FindBetweenIndex");
			UNIT_TEST_POSITIVE(array1.FindBetweenIndex(5, oneLess, 6, 9) == 7, "ArrayC FindBetweenIndex");
			UNIT_TEST_POSITIVE(array1.FindBetweenIndex(1, oneLess, 6, 9) == -1, "ArrayC FindBetweenIndex");

		UNIT_TEST_BLOCK_END();
		
		UNIT_TEST_BLOCK_START();
						
			int cArray1[10] = {1, 2, 3, 4, 5, 6, 3, 4, 5, 7};

			Dia::Core::Containers::ArrayC<int, 10> array1(cArray1, 10);
			
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

			UNIT_TEST_POSITIVE(array1.FindLastIndex(5) == 8 , "ArrayC FindLastIndex");
			UNIT_TEST_POSITIVE(array1.FindLastIndex(3) == 6, "ArrayC FindLastIndex");
			UNIT_TEST_POSITIVE(array1.FindLastIndex(10) == -1, "ArrayC FindLastIndex");

			UNIT_TEST_POSITIVE(array1.FindLastBetweenIndex(4, 0, 7) == 7 , "ArrayC FindLastBetweenIndex");
			UNIT_TEST_POSITIVE(array1.FindLastBetweenIndex(5, 6, 9) == 8, "ArrayC FindLastBetweenIndex");
			UNIT_TEST_POSITIVE(array1.FindLastBetweenIndex(1, 6, 9) == -1, "ArrayC FindLastBetweenIndex");
			
			UNIT_TEST_POSITIVE(array1.FindLastIndex(5, oneLess) == 7 , "ArrayC FindLastIndex");
			UNIT_TEST_POSITIVE(array1.FindLastIndex(7, oneLess) == 5, "ArrayC FindLastIndex");
			UNIT_TEST_POSITIVE(array1.FindLastIndex(10, oneLess) == -1, "ArrayC FindLastIndex");

			UNIT_TEST_POSITIVE(array1.FindLastBetweenIndex(5, oneLess, 0, 7) == 7 , "ArrayC FindLastBetweenIndex");
			UNIT_TEST_POSITIVE(array1.FindLastBetweenIndex(5, oneLess, 6, 9) == 7, "ArrayC FindLastBetweenIndex");
			UNIT_TEST_POSITIVE(array1.FindLastBetweenIndex(1, oneLess, 6, 9) == -1, "ArrayC FindLastBetweenIndex");

		UNIT_TEST_BLOCK_END();
		
		UNIT_TEST_BLOCK_START();
						
			int cArray1[10] = {1, 2, 3, 4, 5, 6, 3, 4, 5, 7};
			// Sorted =		  {1, 2, 3, 3, 4, 4, 5, 5, 6, 7}
			Dia::Core::Containers::ArrayC<int, 10> array1(cArray1, 10);
			
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
			
			UNIT_TEST_POSITIVE(array1.IsSorted(), "ArrayC FindSortedIndex");

			UNIT_TEST_POSITIVE(array1.FindSortedIndex(5) == 6 , "ArrayC FindSortedIndex");
			UNIT_TEST_POSITIVE(array1.FindSortedIndex(3) == 2, "ArrayC FindSortedIndex");
			UNIT_TEST_POSITIVE(array1.FindSortedIndex(10) == -1, "ArrayC FindSortedIndex");

			UNIT_TEST_POSITIVE(array1.FindLastSortedIndex(4) == 5 , "ArrayC FindLastSortedIndex");
			UNIT_TEST_POSITIVE(array1.FindLastSortedIndex(5) == 7, "ArrayC FindLastSortedIndex");
			UNIT_TEST_POSITIVE(array1.FindLastSortedIndex(10) == -1, "ArrayC FindLastSortedIndex");
			
			UNIT_TEST_POSITIVE(array1.FindSortedIndex(5, equality) == 6 , "ArrayC FindSortedIndex");
			UNIT_TEST_POSITIVE(array1.FindSortedIndex(3, equality) == 2, "ArrayC FindSortedIndex");
			UNIT_TEST_POSITIVE(array1.FindSortedIndex(10, equality) == -1, "ArrayC FindSortedIndex");

			UNIT_TEST_POSITIVE(array1.FindLastSortedIndex(4, equality) == 5 , "ArrayC FindLastSortedIndex");
			UNIT_TEST_POSITIVE(array1.FindLastSortedIndex(5, equality) == 7, "ArrayC FindLastSortedIndex");
			UNIT_TEST_POSITIVE(array1.FindLastSortedIndex(10, equality) == -1, "ArrayC FindLastSortedIndex");

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
			
			Dia::Core::Containers::ArrayC<int, 10> array1(cArray1, 10);
					
			UNIT_TEST_POSITIVE(array1.HighestEvalutionIndex(eval) == 3, "ArrayC HighestEvalutionIndex");

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
			
			Dia::Core::Containers::ArrayC<Foo, 3> array1(cArray1, 3);
			
			cArray1[0].mSomeData = false;
			cArray1[0].mMoreData = 11.0f;

			UNIT_TEST_POSITIVE(array1.Size() == 3, "ArrayC Test Non primitive");
			UNIT_TEST_POSITIVE(array1.At(0).SomeData(), "ArrayC Test Non primitive");	
			UNIT_TEST_POSITIVE(array1.At(1).SomeData(), "ArrayC Test Non primitive");
			UNIT_TEST_POSITIVE(array1.At(2).SomeData(), "ArrayC Test Non primitive");

			UNIT_TEST_POSITIVE(array1.At(0).MoreData() == 21.0f, "ArrayC Test Non primitive");	
			UNIT_TEST_POSITIVE(array1.At(1).MoreData() == 21.0f, "ArrayC Test Non primitive");
			UNIT_TEST_POSITIVE(array1.At(2).MoreData() == 21.0f, "ArrayC Test Non primitive");

		UNIT_TEST_BLOCK_END();
		mState = kFinished;
	}
}
