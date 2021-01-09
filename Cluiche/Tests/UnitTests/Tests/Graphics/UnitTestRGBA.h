#pragma once

#include "UnitTests/Tests/Graphics/UnitTestGraphics.h"

namespace UnitTests
{
	class UnitTestRGBA: public UnitTestGraphics
	{
	public:
		UnitTestRGBA(const Dia::Core::Containers::String32& name);
		UnitTestRGBA(void);
		
		void DoTest();
	};
}