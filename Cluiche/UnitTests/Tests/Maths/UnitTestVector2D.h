#pragma once

#include "UnitTests/Tests/Maths/UnitTestMaths.h"

namespace UnitTests
{
	class UnitTestVector2D: public UnitTestMaths
	{
	public:
		UnitTestVector2D(const Dia::Core::Containers::String32& name);
		UnitTestVector2D(void);
		
		void DoTest();
	};
}