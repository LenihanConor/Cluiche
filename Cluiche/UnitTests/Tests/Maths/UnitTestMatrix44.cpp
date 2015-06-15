
#include "UnitTests/Tests/Maths/UnitTestMatrix44.h"

#include "UnitTests/Infrastructure/UnitTestMacros.h"


namespace UnitTests
{	
	UnitTestMatrix44::UnitTestMatrix44(const Dia::Core::Containers::String32& name)
		: UnitTestMaths(name)
	{}

	UnitTestMatrix44::UnitTestMatrix44(void)
		: UnitTestMaths()
	{}

	void UnitTestMatrix44::DoTest()
	{
		UNIT_TEST_BLOCK_START()
			
		//	UNIT_TEST_POSITIVE(Dia::Maths::DegToRadians(0.0f) == 0.0f, "Trigonometry");
				
		UNIT_TEST_BLOCK_END()
		
		mState = kFinished;
	}
}
