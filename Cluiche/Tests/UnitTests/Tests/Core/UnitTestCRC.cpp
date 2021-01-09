#include "UnitTests/Tests/Core/UnitTestCRC.h"

#include "UnitTests/Infrastructure/UnitTestMacros.h"

#include <DiaCore/CRC/CRC.h>
#include <DiaCore/CRC/StringCRC.h>

using namespace Dia::Core;

namespace UnitTests
{	
	UnitTestCRC::UnitTestCRC(const Dia::Core::Containers::String32& name)
		: UnitTestCore(name)
	{}

	UnitTestCRC::UnitTestCRC(void)
		: UnitTestCore()
	{}

	void UnitTestCRC::DoTest()
	{
		UNIT_TEST_BLOCK_START()

 			CRC crc1;	
 	
 			UNIT_TEST_POSITIVE(crc1.Value() == 0, "CRC");
 			
			CRC crc2(2);

			UNIT_TEST_POSITIVE(crc2.Value() == 2, "CRC");
			
			CRC crc3(crc2);

			UNIT_TEST_POSITIVE(crc3.Value() == 2, "CRC");
			
			CRC crc4("Conor's Test");
			CRC crc5("Conor's Test");

			UNIT_TEST_POSITIVE(crc4 == crc5, "CRC");
			
		UNIT_TEST_BLOCK_END()	

		UNIT_TEST_BLOCK_START()

			CRC crc1(2);
			CRC crc2;

			crc2 = crc1;

			UNIT_TEST_POSITIVE(crc2 == crc1, "CRC");
			
			CRC crc3;

			crc3 = crc1.Value();

			UNIT_TEST_POSITIVE(crc3 == crc1, "CRC");
			
			CRC crc4(2);

			unsigned int value = crc4;

			UNIT_TEST_POSITIVE(value == 2, "CRC");

		UNIT_TEST_BLOCK_END()

		mState = kFinished;
	}

	UnitTestStringCRC::UnitTestStringCRC(const Dia::Core::Containers::String32& name)
		: UnitTestCore(name)
	{}

	UnitTestStringCRC::UnitTestStringCRC(void)
		: UnitTestCore()
	{}

	void UnitTestStringCRC::DoTest()
	{
		UNIT_TEST_BLOCK_START()

			StringCRC crc1;	

			UNIT_TEST_POSITIVE(crc1.Value() == 0, "CRC");
			UNIT_TEST_POSITIVE(strlen(crc1.AsChar()) == 0, "CRC");

		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()

			StringCRC crc1("Conor");	

			UNIT_TEST_POSITIVE(!strcmp(crc1.AsChar(), "Conor"), "CRC");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			StringCRC crc1("Conor");	
			StringCRC crc2(crc1);

			UNIT_TEST_POSITIVE(crc1 == crc2, "CRC");
			UNIT_TEST_POSITIVE(!strcmp(crc1.AsChar(), "Conor"), "CRC");

		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()

			StringCRC crc1("Conor");	
			StringCRC crc2;

			crc2 = crc1;

			UNIT_TEST_POSITIVE(crc1 == crc2, "CRC");
			UNIT_TEST_POSITIVE(!strcmp(crc1.AsChar(), "Conor"), "CRC");

		UNIT_TEST_BLOCK_END()
			
		mState = kFinished;
	}


	UnitTestStripStringCRC::UnitTestStripStringCRC(const Dia::Core::Containers::String32& name)
		: UnitTestCore(name)
	{}

	UnitTestStripStringCRC::UnitTestStripStringCRC(void)
		: UnitTestCore()
	{}

	void UnitTestStripStringCRC::DoTest()
	{
		UNIT_TEST_BLOCK_START()

			StringCRC crc1;	

			UNIT_TEST_POSITIVE(crc1.Value() == 0, "CRC");
#ifdef DEBUG			
			UNIT_TEST_POSITIVE(strlen(crc1.AsChar()) ==0, "CRC");
#endif
		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			StringCRC crc1("Conor");	
#ifdef DEBUG
			UNIT_TEST_POSITIVE(!strcmp(crc1.AsChar(), "Conor"), "CRC");
#endif
		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			StringCRC crc1("Conor");	
			StringCRC crc2(crc1);

			UNIT_TEST_POSITIVE(crc1 == crc2, "CRC");
#ifdef DEBUG
			UNIT_TEST_POSITIVE(!strcmp(crc1.AsChar(), "Conor"), "CRC");
#endif
		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			StringCRC crc1("Conor");	
			StringCRC crc2;

			crc2 = crc1;

			UNIT_TEST_POSITIVE(crc1 == crc2, "CRC");
#ifdef DEBUG
			UNIT_TEST_POSITIVE(!strcmp(crc1.AsChar(), "Conor"), "CRC");
#endif
		UNIT_TEST_BLOCK_END()

		mState = kFinished;
	}
}