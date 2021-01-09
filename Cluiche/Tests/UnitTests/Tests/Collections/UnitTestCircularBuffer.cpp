
#include "UnitTests/Tests/Collections/UnitTestCircularBuffer.h"

#include <DiaCore/Containers/Misc/CircularBufferC.h>

#include "UnitTests/Infrastructure/UnitTestMacros.h"

namespace UnitTests
{	
	UnitTestCircularBuffer::UnitTestCircularBuffer(const Dia::Core::Containers::String32& name)
		: UnitTestCoreContainers(name)
	{}

	UnitTestCircularBuffer::UnitTestCircularBuffer(void)
		: UnitTestCoreContainers()
	{}

	void UnitTestCircularBuffer::DoTest()
	{
		UNIT_TEST_BLOCK_START();

			Dia::Core::Containers::CircularBufferC<int, 3> buffer;

			UNIT_TEST_POSITIVE(buffer.Size() == 0, "CircularBufferC()");
			UNIT_TEST_POSITIVE(buffer.IsEmpty(), "CircularBufferC()");

			buffer.PushNext(0);

			UNIT_TEST_POSITIVE(buffer.Size() == 1, "CircularBufferC()");
			UNIT_TEST_POSITIVE(!buffer.IsEmpty(), "CircularBufferC()");
			UNIT_TEST_POSITIVE(buffer.Front() == 0, "CircularBufferC()");
			UNIT_TEST_POSITIVE(buffer.Back() == 0, "CircularBufferC()");
	
			buffer.PushNext(1);

			UNIT_TEST_POSITIVE(buffer.Size() == 2, "CircularBufferC()");
			UNIT_TEST_POSITIVE(!buffer.IsEmpty(), "CircularBufferC()");
			UNIT_TEST_POSITIVE(buffer.Front() == 1, "CircularBufferC()");
			UNIT_TEST_POSITIVE(buffer.Back() == 0, "CircularBufferC()");

			buffer.PushNext(2);

			UNIT_TEST_POSITIVE(buffer.Size() == 3, "CircularBufferC()");
			UNIT_TEST_POSITIVE(!buffer.IsEmpty(), "CircularBufferC()");
			UNIT_TEST_POSITIVE(buffer.Front() == 2, "CircularBufferC()");
			UNIT_TEST_POSITIVE(buffer.Back() == 0, "CircularBufferC()");

			buffer.PushNext(3);

			UNIT_TEST_POSITIVE(buffer.Size() == 3, "CircularBufferC()");
			UNIT_TEST_POSITIVE(!buffer.IsEmpty(), "CircularBufferC()");
			UNIT_TEST_POSITIVE(buffer.Front() == 3, "CircularBufferC()");
			UNIT_TEST_POSITIVE(buffer.Back() == 1, "CircularBufferC()");

			buffer.PushNext(4);

			UNIT_TEST_POSITIVE(buffer.Size() == 3, "CircularBufferC()");
			UNIT_TEST_POSITIVE(!buffer.IsEmpty(), "CircularBufferC()");
			UNIT_TEST_POSITIVE(buffer.Front() == 4, "CircularBufferC()");
			UNIT_TEST_POSITIVE(buffer.Back() == 2, "CircularBufferC()");

			buffer.RemoveAll();

			UNIT_TEST_POSITIVE(buffer.Size() == 0, "CircularBufferC()");
			UNIT_TEST_POSITIVE(buffer.IsEmpty(), "CircularBufferC()");

		UNIT_TEST_BLOCK_END();
		
		UNIT_TEST_BLOCK_START();

			Dia::Core::Containers::CircularBufferC<int, 3> buffer;

			UNIT_TEST_POSITIVE(buffer.Size() == 0, "CircularBufferC()");
			UNIT_TEST_POSITIVE(buffer.IsEmpty(), "CircularBufferC()");

			buffer.PushNext(1);
			buffer.PushNext(2);
			buffer.PushNext(3);

			Dia::Core::Containers::CircularBufferC<int, 3>::ConstIterator iter(&buffer.Back(), &buffer.Back(), &buffer.Front());

			UNIT_TEST_POSITIVE(buffer.Size() == 3, "CircularBufferC()");

			UNIT_TEST_POSITIVE(*iter.Current() == 1, "CircularBufferC()");
			iter.Next();
			UNIT_TEST_POSITIVE(*iter.Current() == 2, "CircularBufferC()");
			iter.Next();
			UNIT_TEST_POSITIVE(*iter.Current() == 3, "CircularBufferC()");
			iter.Next();
			UNIT_TEST_POSITIVE(*iter.Current() == 1, "CircularBufferC()");
			iter.Next();
			UNIT_TEST_POSITIVE(*iter.Current() == 2, "CircularBufferC()");
			iter.Next();
			UNIT_TEST_POSITIVE(*iter.Current() == 3, "CircularBufferC()");
			iter.Next();

		UNIT_TEST_BLOCK_END();

		UNIT_TEST_BLOCK_START();

			struct Foo
			{
				Foo() : x(0), y(0){}

				Foo(int _x, int _y)
					: x(_x)
					, y(_y)
				{}

				int x;
				int y;
			};

			Dia::Core::Containers::CircularBufferC<Foo, 3> buffer;

			UNIT_TEST_POSITIVE(buffer.Size() == 0, "CircularBufferC()");
			UNIT_TEST_POSITIVE(buffer.IsEmpty(), "CircularBufferC()");

			buffer.PushNext(Foo(0,9));
			buffer.PushNext(Foo(1, 8));
			buffer.PushNext(Foo(2, 7));

			Dia::Core::Containers::CircularBufferC<Foo, 3>::ConstIterator iter(&buffer.Back(), &buffer.Back(), &buffer.Front());

			UNIT_TEST_POSITIVE(buffer.Size() == 3, "CircularBufferC()");

			UNIT_TEST_POSITIVE((*iter.Current()).y == 9, "CircularBufferC()");
			iter.Next();
			UNIT_TEST_POSITIVE((*iter.Current()).y == 8, "CircularBufferC()");
			iter.Next();
			UNIT_TEST_POSITIVE((*iter.Current()).y == 7, "CircularBufferC()");
			iter.Next();
			UNIT_TEST_POSITIVE((*iter.Current()).y == 9, "CircularBufferC()");
			iter.Previous();
			UNIT_TEST_POSITIVE((*iter.Current()).y == 7, "CircularBufferC()");
			iter.Previous();
			UNIT_TEST_POSITIVE((*iter.Current()).y == 8, "CircularBufferC()");
			iter.Previous();
			UNIT_TEST_POSITIVE((*iter.Current()).y == 9, "CircularBufferC()");
		UNIT_TEST_BLOCK_END();

		mState = kFinished;
	}
}
