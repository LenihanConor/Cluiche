
#include "UnitTests/Tests/Maths/UnitTestVector3D.h"

#include "UnitTests/Infrastructure/UnitTestMacros.h"


namespace UnitTests
{	
	UnitTestVector3D::UnitTestVector3D(const Dia::Core::Containers::String32& name)
		: UnitTestMaths(name)
	{}

	UnitTestVector3D::UnitTestVector3D(void)
		: UnitTestMaths()
	{}

	void UnitTestVector3D::DoTest()
	{
		UNIT_TEST_BLOCK_START()
			
		//	UNIT_TEST_POSITIVE(Dia::Maths::DegToRadians(0.0f) == 0.0f, "Trigonometry");
				
		UNIT_TEST_BLOCK_END()
		
		mState = kFinished;
	}
}
