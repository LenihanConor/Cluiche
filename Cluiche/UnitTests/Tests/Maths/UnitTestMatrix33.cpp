
#include "UnitTests/Tests/Maths/UnitTestMatrix33.h"

#include "UnitTests/Infrastructure/UnitTestMacros.h"


namespace UnitTests
{	
	UnitTestMatrix33::UnitTestMatrix33(const Dia::Core::Containers::String32& name)
		: UnitTestMaths(name)
	{}

	UnitTestMatrix33::UnitTestMatrix33(void)
		: UnitTestMaths()
	{}

	void UnitTestMatrix33::DoTest()
	{
		UNIT_TEST_BLOCK_START()
			
		//	UNIT_TEST_POSITIVE(Dia::Maths::DegToRadians(0.0f) == 0.0f, "Trigonometry");
				
		UNIT_TEST_BLOCK_END()
		
		mState = kFinished;
	}
}
