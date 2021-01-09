#pragma once

#include "UnitTests/Tests/Maths/UnitTestMaths.h"

namespace UnitTests
{
	class UnitTestIntersectionTest: public UnitTestMaths
	{
	public:
		UnitTestIntersectionTest(const Dia::Core::Containers::String32& name);
		UnitTestIntersectionTest(void);
		
		void DoTest();
	};
}