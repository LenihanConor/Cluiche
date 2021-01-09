
#include "UnitTests/Tests/Maths/UnitTestIntersectionTest.h"

#include "UnitTests/Infrastructure/UnitTestMacros.h"

#include <DiaMaths/Shape/Common/IntersectionTests.h>

namespace UnitTests
{	
	UnitTestIntersectionTest::UnitTestIntersectionTest(const Dia::Core::Containers::String32& name)
		: UnitTestMaths(name)
	{}

	UnitTestIntersectionTest::UnitTestIntersectionTest(void)
		: UnitTestMaths()
	{}

	void UnitTestIntersectionTest::DoTest()
	{
		/*UNIT_TEST_BLOCK_START()
			
			UNIT_TEST_POSITIVE(Dia::Maths::DegToRadians(0.0f) == 0.0f, "Trigonometry");
			UNIT_TEST_POSITIVE(Dia::Maths::DegToRadians(45.0f) == 0.785398163f, "Trigonometry");
			UNIT_TEST_POSITIVE(Dia::Maths::DegToRadians(90.0f) == 1.57079633f, "Trigonometry");
			UNIT_TEST_POSITIVE(Dia::Maths::DegToRadians(135.0f) == 2.35619449f, "Trigonometry");
			UNIT_TEST_POSITIVE(Dia::Maths::DegToRadians(180.0f) == 3.14159265f, "Trigonometry");
			UNIT_TEST_POSITIVE(Dia::Maths::DegToRadians(-45.0f) == -0.785398163f, "Trigonometry");
			
		UNIT_TEST_BLOCK_END()*/
		
		

		mState = kFinished;
	}
}
