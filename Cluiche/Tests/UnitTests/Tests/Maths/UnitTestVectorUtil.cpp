
#include "UnitTests/Tests/Maths/UnitTestVectorUtil.h"

#include "UnitTests/Infrastructure/UnitTestMacros.h"

namespace UnitTests
{	
	UnitTestVectorUtil::UnitTestVectorUtil(const Dia::Core::Containers::String32& name)
		: UnitTestMaths(name)
	{}

	UnitTestVectorUtil::UnitTestVectorUtil(void)
		: UnitTestMaths()
	{}

	void UnitTestVectorUtil::DoTest()
	{
		UNIT_TEST_BLOCK_START()
			
		//	UNIT_TEST_POSITIVE(Dia::Maths::DegToRadians(0.0f) == 0.0f, "Trigonometry");
					
		UNIT_TEST_BLOCK_END()
		
		mState = kFinished;
	}
}
