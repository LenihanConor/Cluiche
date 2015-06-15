
#include "UnitTests/Tests/Maths/UnitTestVector4D.h"

#include "UnitTests/Infrastructure/UnitTestMacros.h"

namespace UnitTests
{	
	UnitTestVector4D::UnitTestVector4D(const Dia::Core::Containers::String32& name)
		: UnitTestMaths(name)
	{}

	UnitTestVector4D::UnitTestVector4D(void)
		: UnitTestMaths()
	{}

	void UnitTestVector4D::DoTest()
	{
		UNIT_TEST_BLOCK_START()
			
		//	UNIT_TEST_POSITIVE(Dia::Maths::DegToRadians(0.0f) == 0.0f, "Trigonometry");
			
		UNIT_TEST_BLOCK_END()
		
		mState = kFinished;
	}
}
