
#include "UnitTests/Tests/Collections/UnitTestStringWriter.h"

#include <DiaCore/Containers/Strings/StringWriter.h>
#include <DiaCore/Strings/stringutils.h>
#include <DiaCore/Strings/String8.h>

#include "UnitTests/Infrastructure/UnitTestMacros.h"

namespace UnitTests
{	
	UnitTestStringWriter::UnitTestStringWriter(const Dia::Core::Containers::String32& name)
		: UnitTestCoreContainers(name)
	{}

	UnitTestStringWriter::UnitTestStringWriter(void)
		: UnitTestCoreContainers()
	{}

	void UnitTestStringWriter::DoTest()
	{
		UNIT_TEST_BLOCK_START()

			Dia::Core::Containers::StringWriter buffer;

			UNIT_TEST_ASSERT_EXPECTED_START();
			buffer.AsCStr();
			UNIT_TEST_ASSERT_EXPECTED_END();

			UNIT_TEST_ASSERT_EXPECTED_START();
			buffer.Capacity();
			UNIT_TEST_ASSERT_EXPECTED_END();

			UNIT_TEST_ASSERT_EXPECTED_START();
			buffer.CurrentLength();
			UNIT_TEST_ASSERT_EXPECTED_END();
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			buffer << "BLAH";
			UNIT_TEST_ASSERT_EXPECTED_END();

			UNIT_TEST_ASSERT_EXPECTED_START();
			buffer << 'b';
			UNIT_TEST_ASSERT_EXPECTED_END();

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			char bufferMemory[8];
			Dia::Core::Containers::StringWriter buffer(&bufferMemory[0], 8);

			UNIT_TEST_POSITIVE(buffer.Capacity() == 8, "StringWriter");
			UNIT_TEST_POSITIVE(buffer.CurrentLength() == 0, "StringWriter");

			buffer << 'c';
			
			UNIT_TEST_POSITIVE(Dia::Core::Containers::String8(buffer.AsCStr()) == "c", "StringWriter");
			UNIT_TEST_POSITIVE(buffer.Capacity() == 8, "StringWriter");
			UNIT_TEST_POSITIVE(buffer.CurrentLength() == 1, "StringWriter");
			
			buffer << "ats";

			UNIT_TEST_POSITIVE(Dia::Core::Containers::String8(buffer.AsCStr()) == "cats", "StringWriter");
			UNIT_TEST_POSITIVE(buffer.Capacity() == 8, "StringWriter");
			UNIT_TEST_POSITIVE(buffer.CurrentLength() == 4, "StringWriter");

			UNIT_TEST_ASSERT_EXPECTED_START();
			buffer << "are crap animals";
			UNIT_TEST_ASSERT_EXPECTED_END();

			UNIT_TEST_ASSERT_EXPECTED_START();
			buffer.AssignBuffer(&bufferMemory[0], 8);
			UNIT_TEST_ASSERT_EXPECTED_END();

		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()

			char bufferMemory[8];
			Dia::Core::Containers::StringWriter buffer;
			
			buffer.AssignBuffer(&bufferMemory[0], 8);
			UNIT_TEST_POSITIVE(buffer.Capacity() == 8, "StringWriter");
			UNIT_TEST_POSITIVE(buffer.CurrentLength() == 0, "StringWriter");

			buffer << 'c';

			UNIT_TEST_POSITIVE(Dia::Core::Containers::String8(buffer.AsCStr()) == "c", "StringWriter");
			UNIT_TEST_POSITIVE(buffer.Capacity() == 8, "StringWriter");
			UNIT_TEST_POSITIVE(buffer.CurrentLength() == 1, "StringWriter");

			buffer << "ats";

			UNIT_TEST_POSITIVE(Dia::Core::Containers::String8(buffer.AsCStr()) == "cats", "StringWriter");
			UNIT_TEST_POSITIVE(buffer.Capacity() == 8, "StringWriter");
			UNIT_TEST_POSITIVE(buffer.CurrentLength() == 4, "StringWriter");

			UNIT_TEST_ASSERT_EXPECTED_START();
			buffer << "are crap animals";
			UNIT_TEST_ASSERT_EXPECTED_END();

			UNIT_TEST_ASSERT_EXPECTED_START();
			buffer.AssignBuffer(&bufferMemory[0], 8);
			UNIT_TEST_ASSERT_EXPECTED_END();

		UNIT_TEST_BLOCK_END()

		mState = kFinished;
	}
}
