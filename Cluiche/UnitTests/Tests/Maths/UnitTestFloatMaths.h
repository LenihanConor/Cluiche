#pragma once

#include "UnitTests/Tests/Maths/UnitTestMaths.h"

namespace UnitTests
{
	class UnitTestFloatMaths: public UnitTestMaths
	{
	public:
		UnitTestFloatMaths(const Dia::Core::Containers::String32& name);
		UnitTestFloatMaths(void);
		
		void DoTest();
	};
}