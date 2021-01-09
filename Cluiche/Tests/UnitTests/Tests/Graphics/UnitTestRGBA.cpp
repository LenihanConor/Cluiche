
#include "UnitTests/Tests/Graphics/UnitTestRGBA.h"

#include "UnitTests/Infrastructure/UnitTestMacros.h"

#include <DiaGraphics/Misc/RGBA.h>

namespace UnitTests
{	
	UnitTestRGBA::UnitTestRGBA(const Dia::Core::Containers::String32& name)
		: UnitTestGraphics(name)
	{}

	UnitTestRGBA::UnitTestRGBA(void)
		: UnitTestGraphics()
	{}

	void UnitTestRGBA::DoTest()
	{
		UNIT_TEST_BLOCK_START()
			
			UNIT_TEST_POSITIVE(Dia::Graphics::RGBA::Green == Dia::Graphics::RGBA(0, 255, 0, 255), "RBGA");		
			UNIT_TEST_POSITIVE(Dia::Graphics::RGBA::Blue == Dia::Graphics::RGBA(0, 0, 255, 255), "RBGA");
			UNIT_TEST_POSITIVE(Dia::Graphics::RGBA::Red == Dia::Graphics::RGBA(255, 0, 0, 255), "RBGA");
			UNIT_TEST_POSITIVE(Dia::Graphics::RGBA::White == Dia::Graphics::RGBA(255, 255, 255, 255), "RBGA");
			UNIT_TEST_POSITIVE(Dia::Graphics::RGBA::Black == Dia::Graphics::RGBA(0, 0, 0, 255), "RBGA");
			
		UNIT_TEST_BLOCK_END()
		
		mState = kFinished;
	}
}
