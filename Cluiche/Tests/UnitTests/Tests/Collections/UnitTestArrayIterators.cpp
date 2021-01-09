
#include "UnitTests/Tests/Collections/UnitTestArrayIterators.h"

#include <DiaCore/Containers/Arrays/ArrayC.h>

#include "UnitTests/Infrastructure/UnitTestMacros.h"

namespace UnitTests
{	
	UnitTestArrayIterators::UnitTestArrayIterators(const Dia::Core::Containers::String32& name)
		: UnitTestCoreContainers(name)
	{}

	UnitTestArrayIterators::UnitTestArrayIterators(void)
		: UnitTestCoreContainers()
	{}

	void UnitTestArrayIterators::DoTest()
	{
		UNIT_TEST_BLOCK_START();

			int cArray1[3] = {1, 2, 3};
			Dia::Core::Containers::ArrayC<int, 3> tempArray(cArray1, 3);

			Dia::Core::Containers::ArrayC<int, 3>::ConstIterator iter1(tempArray.Begin());
			Dia::Core::Containers::ArrayC<int, 3>::ConstIterator iter2(tempArray.Begin());

			UNIT_TEST_POSITIVE(iter1.Begin() == &tempArray[0], "Dia::Core::Containers::ArrayC<T, size>::Iterator");
			UNIT_TEST_POSITIVE(iter1.Begin() == iter1.Current(), "Dia::Core::Containers::ArrayC<T, size>::Iterator");
			UNIT_TEST_POSITIVE(*iter1.Begin() == 1, "Dia::Core::Containers::ArrayC<T, size>::Iterator");

			UNIT_TEST_POSITIVE(iter1.End() == &tempArray[2], "Dia::Core::Containers::ArrayC<T, size>::Iterator");
			UNIT_TEST_POSITIVE(*iter1.End() == 3, "Dia::Core::Containers::ArrayC<T, size>::Iterator");

			UNIT_TEST_POSITIVE(!iter1.IsDone(), "Dia::Core::Containers::ArrayC<T, size>::Iterator");
			UNIT_TEST_POSITIVE(iter1 == iter2, "Dia::Core::Containers::ArrayC<T, size>::Iterator");
			UNIT_TEST_POSITIVE(iter1 <= iter2, "Dia::Core::Containers::ArrayC<T, size>::Iterator");
			UNIT_TEST_POSITIVE(iter1 >= iter2, "Dia::Core::Containers::ArrayC<T, size>::Iterator");

			UNIT_TEST_NEGATIVE(iter1 != iter2, "Dia::Core::Containers::ArrayC<T, size>::Iterator");
			UNIT_TEST_NEGATIVE(iter1 > iter2, "Dia::Core::Containers::ArrayC<T, size>::Iterator");
			UNIT_TEST_NEGATIVE(iter1 < iter2, "Dia::Core::Containers::ArrayC<T, size>::Iterator");

			iter2.Next();

			UNIT_TEST_POSITIVE(!iter2.IsDone(), "Dia::Core::Containers::ArrayC<T, size>::Iterator");
			UNIT_TEST_POSITIVE(iter2.Current() == &tempArray[1], "Dia::Core::Containers::ArrayC<T, size>::Iterator");
			UNIT_TEST_POSITIVE(*iter2.Current() == 2, "Dia::Core::Containers::ArrayC<T, size>::Iterator");

			UNIT_TEST_POSITIVE(iter1 != iter2, "Dia::Core::Containers::ArrayC<T, size>::Iterator");
			UNIT_TEST_POSITIVE(iter1 < iter2, "Dia::Core::Containers::ArrayC<T, size>::Iterator");
			UNIT_TEST_POSITIVE(iter1 <= iter2, "Dia::Core::Containers::ArrayC<T, size>::Iterator");

			UNIT_TEST_NEGATIVE(iter1 == iter2, "Dia::Core::Containers::ArrayC<T, size>::Iterator");
			UNIT_TEST_NEGATIVE(iter1 > iter2, "Dia::Core::Containers::ArrayC<T, size>::Iterator");
			UNIT_TEST_NEGATIVE(iter1 >= iter2, "Dia::Core::Containers::ArrayC<T, size>::Iterator");

			iter2.Next();

			UNIT_TEST_POSITIVE(!iter2.IsDone(), "Dia::Core::Containers::ArrayC<T, size>::Iterator");
			UNIT_TEST_POSITIVE(iter2.Current() == &tempArray[2], "Dia::Core::Containers::ArrayC<T, size>::Iterator");
			UNIT_TEST_POSITIVE(*iter2.Current() == 3, "Dia::Core::Containers::ArrayC<T, size>::Iterator");
			UNIT_TEST_POSITIVE(iter2.Current() == iter2.End(), "Dia::Core::Containers::ArrayC<T, size>::Iterator");

			iter2.Next();

			UNIT_TEST_POSITIVE(iter2.IsDone(), "Dia::Core::Containers::ArrayC<T, size>::Iterator");

			UNIT_TEST_ASSERT_EXPECTED_START();
			iter2.Current();
			UNIT_TEST_ASSERT_EXPECTED_END();

			iter2.Previous();
			
			UNIT_TEST_POSITIVE(!iter2.IsDone(), "Dia::Core::Containers::ArrayC<T, size>::Iterator");
			UNIT_TEST_POSITIVE(iter2.Current() == &tempArray[2], "Dia::Core::Containers::ArrayC<T, size>::Iterator");
			UNIT_TEST_POSITIVE(*iter2.Current() == 3, "Dia::Core::Containers::ArrayC<T, size>::Iterator");
			UNIT_TEST_POSITIVE(iter2.Current() == iter2.End(), "Dia::Core::Containers::ArrayC<T, size>::Iterator");

			iter1.Previous();
			
			UNIT_TEST_POSITIVE(iter1.IsDone(), "Dia::Core::Containers::ArrayC<T, size>::Iterator");

			UNIT_TEST_ASSERT_EXPECTED_START();
			iter1.Current();
			UNIT_TEST_ASSERT_EXPECTED_END();

		UNIT_TEST_BLOCK_END();
		
		UNIT_TEST_BLOCK_START();

			int cArray1[3] = {1, 2, 3};
			Dia::Core::Containers::ArrayC<int, 3> tempArray(cArray1, 3);

			Dia::Core::Containers::ArrayC<int, 3>::ConstReverseIterator iter1(tempArray.End());
			Dia::Core::Containers::ArrayC<int, 3>::ConstReverseIterator iter2(tempArray.End());
			Dia::Core::Containers::ArrayC<int, 3>::ConstReverseIterator iter3(tempArray.End());

			UNIT_TEST_POSITIVE(iter1.Begin() == &tempArray[2], "Dia::Core::Containers::ArrayC<T, size>::ConstReverseIterator");
			UNIT_TEST_POSITIVE(iter1.Begin() == iter1.Current(), "Dia::Core::Containers::ArrayC<T, size>::ConstReverseIterator");
			UNIT_TEST_POSITIVE(*iter1.Begin() == 3, "Dia::Core::Containers::ArrayC<T, size>::ConstReverseIterator");

			UNIT_TEST_POSITIVE(iter1.End() == &tempArray[0], "Dia::Core::Containers::ArrayC<T, size>::ConstReverseIterator");
			UNIT_TEST_POSITIVE(*iter1.End() == 1, "Dia::Core::Containers::ArrayC<T, size>::ConstReverseIterator");

			UNIT_TEST_POSITIVE(!iter1.IsDone(), "Dia::Core::Containers::ArrayC<T, size>::ConstReverseIterator");
			UNIT_TEST_POSITIVE(iter1 == iter2, "Dia::Core::Containers::ArrayC<T, size>::ConstReverseIterator");
			UNIT_TEST_POSITIVE(iter1 <= iter2, "Dia::Core::Containers::ArrayC<T, size>::ConstReverseIterator");
			UNIT_TEST_POSITIVE(iter1 >= iter2, "Dia::Core::Containers::ArrayC<T, size>::ConstReverseIterator");

			UNIT_TEST_NEGATIVE(iter1 != iter2, "Dia::Core::Containers::ArrayC<T, size>::ConstReverseIterator");
			UNIT_TEST_NEGATIVE(iter1 > iter2, "Dia::Core::Containers::ArrayC<T, size>::ConstReverseIterator");
			UNIT_TEST_NEGATIVE(iter1 < iter2, "Dia::Core::Containers::ArrayC<T, size>::ConstReverseIterator");

			iter2.Next();

			UNIT_TEST_POSITIVE(!iter2.IsDone(), "Dia::Core::Containers::ArrayC<T, size>::ConstReverseIterator");
			UNIT_TEST_POSITIVE(iter2.Current() == &tempArray[1], "Dia::Core::Containers::ArrayC<T, size>::ConstReverseIterator");
			UNIT_TEST_POSITIVE(*iter2.Current() == 2, "Dia::Core::Containers::ArrayC<T, size>::ConstReverseIterator");

			UNIT_TEST_POSITIVE(iter1 != iter2, "Dia::Core::Containers::ArrayC<T, size>::ConstReverseIterator");
			UNIT_TEST_POSITIVE(iter1 < iter2, "Dia::Core::Containers::ArrayC<T, size>::ConstReverseIterator");
			UNIT_TEST_POSITIVE(iter1 <= iter2, "Dia::Core::Containers::ArrayC<T, size>::ConstReverseIterator");

			UNIT_TEST_NEGATIVE(iter1 == iter2, "Dia::Core::Containers::ArrayC<T, size>::ConstReverseIterator");
			UNIT_TEST_NEGATIVE(iter1 > iter2, "Dia::Core::Containers::ArrayC<T, size>::ConstReverseIterator");
			UNIT_TEST_NEGATIVE(iter1 >= iter2, "Dia::Core::Containers::ArrayC<T, size>::ConstReverseIterator");

			iter2.Next();

			UNIT_TEST_POSITIVE(!iter2.IsDone(), "Dia::Core::Containers::ArrayC<T, size>::ConstReverseIterator");
			UNIT_TEST_POSITIVE(iter2.Current() == &tempArray[0], "Dia::Core::Containers::ArrayC<T, size>::ConstReverseIterator");
			UNIT_TEST_POSITIVE(*iter2.Current() == 1, "Dia::Core::Containers::ArrayC<T, size>::ConstReverseIterator");
			UNIT_TEST_POSITIVE(iter2.Current() == iter2.End(), "Dia::Core::Containers::ArrayC<T, size>::ConstReverseIterator");

			iter2.Next();

			UNIT_TEST_POSITIVE(iter2.IsDone(), "Dia::Core::Containers::ArrayC<T, size>::ConstReverseIterator");

			UNIT_TEST_ASSERT_EXPECTED_START();
			iter2.Current();
			UNIT_TEST_ASSERT_EXPECTED_END();

			iter2.Previous();
			
			UNIT_TEST_POSITIVE(!iter2.IsDone(), "Dia::Core::Containers::ArrayC<T, size>::ConstReverseIterator");
			UNIT_TEST_POSITIVE(iter2.Current() == &tempArray[0], "Dia::Core::Containers::ArrayC<T, size>::ConstReverseIterator");
			UNIT_TEST_POSITIVE(*iter2.Current() == 1, "Dia::Core::Containers::ArrayC<T, size>::ConstReverseIterator");
			UNIT_TEST_POSITIVE(iter2.Current() == iter2.End(), "Dia::Core::Containers::ArrayC<T, size>::ConstReverseIterator");

			iter1.Previous();
			
			UNIT_TEST_POSITIVE(iter1.IsDone(), "Dia::Core::Containers::ArrayC<T, size>::ConstReverseIterator");

			UNIT_TEST_ASSERT_EXPECTED_START();
			iter1.Current();
			UNIT_TEST_ASSERT_EXPECTED_END();

		UNIT_TEST_BLOCK_END();

		mState = kFinished;
	}
}
