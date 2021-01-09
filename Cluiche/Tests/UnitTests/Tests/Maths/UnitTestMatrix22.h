#pragma once

#include "UnitTests/Tests/Maths/UnitTestMaths.h"

namespace UnitTests
{
	class UnitTestMatrix22: public UnitTestMaths
	{
	public:
		UnitTestMatrix22(const Dia::Core::Containers::String32& name);
		UnitTestMatrix22(void);
		
		void DoTest();
	};
}